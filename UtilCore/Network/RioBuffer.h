
//***************************************************************************
// RioBuffer.h : interface for the CRioBuffer class.
//
//***************************************************************************

#ifndef __RIOSBUFFER_H__
#define __RIOSBUFFER_H__

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef __LOCKFREESLOTSTACK_H__
#include <Containers/Stack/LockFreeSlotStack.h>
#endif

#include <winsock2.h>
#include <mswsock.h>

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <malloc.h>
#include <memory>
#include <mutex>
#include <stdexcept>

//***************************************************************************
// @class CRioBuffer
// @brief Windows RIO(Registered I/O) 환경에 최적화된 고정 크기 슬롯 기반 버퍼 관리 클래스입니다.
//
// @details
// 하나의 큰 연속 메모리 영역을 할당받아 OS에 등록(RIORegisterBuffer)하고, 전체 영역을 
// 동일한 크기의 슬롯(Slot)들로 나누어 관리합니다. 앞서 구현한 `CLockFreeSlotStack`을 
// 활용하여 멀티스레드 환경에서 락(Lock) 없이 고속으로 슬롯의 할당과 반환을 수행합니다.
//
// 설계 구조:
//   하나의 큰 메모리 영역
//          |
//          v
//   RIORegisterBuffer()
//          |
//          v
//   +------+------+------+------+
//   |Slot 0|Slot 1|Slot 2| ...  |
//   +------+------+------+------+
//          |
//          v
//   CLockFreeSlotStack
//
// 주요 특징 및 방어 메커니즘:
//  - 고정 크기 슬롯 및 단일 RIO_BUFFERID 관리
//  - 락 프리 Free Slot 스택 기반 MPMC 최적화
//  - Slot generation을 통한 Stale completion 방어
//  - Slot state CAS를 통한 Double free 방어
//  - Active Allocator Count를 통한 Shutdown/Alloc race 방어
//  - Outstanding slot count를 통한 안전한 RIO 버퍼 생명주기 관리
//***************************************************************************
class CRioBuffer
{
public:
    //***************************************************************************
    // @enum SlotState
    // @brief 각 슬롯의 현재 사용 상태를 나타내는 열거형입니다.
    //***************************************************************************
    enum class SlotState : uint8_t
    {
        Free = 0,
        InUse = 1
    };

    //***************************************************************************
    // @struct SlotToken
    // @brief RIO_BUF 정보와 세대(Generation), 인덱스를 함께 관리하여 Stale completion을 방어하는 토큰 구조체입니다.
    //***************************************************************************
    struct SlotToken
    {
        RIO_BUF Buffer{};
        uint32_t SlotIndex{ kInvalidSlot };
        uint64_t Generation{ 0 };

        //***************************************************************************
        // @brief 토큰이 유효한 슬롯을 가리키고 있는지 확인합니다.
        // @return true: 유효함, false: 유효하지 않음
        //***************************************************************************
        bool IsValid() const noexcept
        {
            return SlotIndex != kInvalidSlot;
        }

        //***************************************************************************
        // @brief 토큰 정보를 초기화합니다.
        //***************************************************************************
        void Reset() noexcept
        {
            Buffer = {};
            SlotIndex = kInvalidSlot;
            Generation = 0;
        }
    };

public:
    //***************************************************************************
    // 상수 정의
    //***************************************************************************
    static constexpr uint32_t kInvalidSlot = (std::numeric_limits<uint32_t>::max)();
    static constexpr size_t kPageAlignment = 4096;

    //***************************************************************************
    // Allocation State 패킹 구조
    // 64비트 아토믹 변수 하나로 상위 비트는 Shutdown 상태, 하위 비트는 Active Allocator 개수를 관리합니다.
    //  - bit 63    : Shutdown 상태 플래그
    //  - bit 0~62  : 현재 진행 중인 AllocSlot() 호출 개수
    //***************************************************************************
    static constexpr uint64_t kShutdownBit = 1ull << 63;
    static constexpr uint64_t kActiveAllocatorMask = ~kShutdownBit;

public:
    //***************************************************************************
    // @brief 지정한 슬롯 개수와 크기로 RIO 버퍼를 초기화하고 커널 메모리에 등록합니다.
    // @param rioTable RIO 확장 함수 테이블 포인터
    // @param slotCount 관리할 총 슬롯 개수
    // @param slotSize 각 슬롯 하나의 바이트 크기
    // @throws std::invalid_argument, std::overflow_error, std::bad_alloc, std::runtime_error
    //***************************************************************************
    CRioBuffer(RIO_EXTENSION_FUNCTION_TABLE* rioTable, uint32_t slotCount, uint32_t slotSize)
        : _rioTable(rioTable)
        , _slotCount(slotCount)
        , _slotSize(slotSize)
        , _freeStack(slotCount)
    {
        if( _rioTable == nullptr )
        {
            throw std::invalid_argument("CRioBuffer: rioTable is null.");
        }

        if( _slotCount == 0 )
        {
            throw std::invalid_argument("CRioBuffer: slotCount must be greater than zero.");
        }

        if( _slotSize == 0 )
        {
            throw std::invalid_argument("CRioBuffer: slotSize must be greater than zero.");
        }

        const uint64_t totalSize64 = static_cast<uint64_t>(_slotCount) * static_cast<uint64_t>(_slotSize);

        if( totalSize64 == 0 )
        {
            throw std::overflow_error("CRioBuffer: total buffer size is zero.");
        }

        if( totalSize64 > static_cast<uint64_t>((std::numeric_limits<uint32_t>::max)()) )
        {
            throw std::overflow_error("CRioBuffer: slotCount * slotSize exceeds uint32_t.");
        }

        _totalSize = static_cast<uint32_t>(totalSize64);

        _generation = std::make_unique<std::atomic<uint64_t>[]>(_slotCount);
        _state = std::make_unique<std::atomic<SlotState>[]>(_slotCount);

        for( uint32_t i = 0; i < _slotCount; ++i )
        {
            _generation[i].store(0, std::memory_order_relaxed);
            _state[i].store(SlotState::Free, std::memory_order_relaxed);
        }

        _bufferPtr = static_cast<char*>(_aligned_malloc(static_cast<size_t>(_totalSize), kPageAlignment));

        if( _bufferPtr == nullptr )
        {
            throw std::bad_alloc();
        }

        _bufferId = _rioTable->RIORegisterBuffer(_bufferPtr, _totalSize);

        if( _bufferId == RIO_INVALID_BUFFERID )
        {
            _aligned_free(_bufferPtr);
            _bufferPtr = nullptr;
            throw std::runtime_error("CRioBuffer: RIORegisterBuffer failed.");
        }

        _initialized.store(true, std::memory_order_release);
    }

    //***************************************************************************
    // 복사 및 이동 생성자/대입 연산자 삭제 (리소스 독점 소유)
    //***************************************************************************
    CRioBuffer(const CRioBuffer&) = delete;
    CRioBuffer& operator=(const CRioBuffer&) = delete;
    CRioBuffer(CRioBuffer&&) = delete;
    CRioBuffer& operator=(CRioBuffer&&) = delete;

    //***************************************************************************
    // @brief 소멸자: 셧다운 수행 후 등록된 버퍼를 해제하고 메모리를 반환합니다.
    //***************************************************************************
    ~CRioBuffer()
    {
        Shutdown();
    }

public:
    //***************************************************************************
    // @brief 락 프리 스택과 상태 CAS를 통해 신규 슬롯을 안전하게 할당받습니다.
    // @param outToken [out] 할당된 슬롯의 정보와 세대가 기록될 SlotToken 참조
    // @return true: 할당 성공, false: 슬롯 고갈 또는 셧다운 진행 중
    //***************************************************************************
    bool AllocSlot(SlotToken& outToken)
    {
        outToken.Reset();

        if( !TryEnterAllocation() )
        {
            return false;
        }

        uint32_t slotIndex = kInvalidSlot;

        if( !_freeStack.Pop(slotIndex) )
        {
            LeaveAllocation();
            return false;
        }

        if( slotIndex >= _slotCount )
        {
            // 방어적 복구:
            // Pop()은 성공했지만 비정상적인 index가 나온 경우
            // 유효하지 않은 index를 FreeStack에 다시 넣으면 안 된다.
            LeaveAllocation();
            return false;
        }

        SlotState expected = SlotState::Free;

        if( !_state[slotIndex].compare_exchange_strong(expected, SlotState::InUse, std::memory_order_acq_rel, std::memory_order_acquire) )
        {
            // Pop()으로 FreeStack에서 제거된 Slot이므로 반드시 복구한다.
            _freeStack.Push(slotIndex);
            LeaveAllocation();
            return false;
        }

        const uint64_t generation = _generation[slotIndex].fetch_add(1, std::memory_order_acq_rel) + 1;
        const uint64_t offset64 = static_cast<uint64_t>(slotIndex) * static_cast<uint64_t>(_slotSize);

        if( offset64 > static_cast<uint64_t>((std::numeric_limits<ULONG>::max)()) )
        {
            _state[slotIndex].store(SlotState::Free, std::memory_order_release);
            _freeStack.Push(slotIndex);
            LeaveAllocation();
            return false;
        }

        if( _slotSize > static_cast<uint32_t>((std::numeric_limits<ULONG>::max)()) )
        {
            _state[slotIndex].store(SlotState::Free, std::memory_order_release);
            _freeStack.Push(slotIndex);
            LeaveAllocation();
            return false;
        }

        _outstanding.fetch_add(1, std::memory_order_acq_rel);

        outToken.Buffer.BufferId = _bufferId;
        outToken.Buffer.Offset = static_cast<ULONG>(offset64);
        outToken.Buffer.Length = static_cast<ULONG>(_slotSize);
        outToken.SlotIndex = slotIndex;
        outToken.Generation = generation;

        LeaveAllocation();

        return true;
    }

    //***************************************************************************
    // @brief SlotToken을 철저히 검증한 뒤 슬롯 상태를 Free로 전환하고 스택에 반환합니다.
    // @param token 반환할 SlotToken 참조
    // @return true: 반환 성공, false: 유효성 검증 실패 또는 Double free 감지
    //***************************************************************************
    bool FreeSlot(const SlotToken& token)
    {
        if( !token.IsValid() )
        {
            return false;
        }

        if( token.SlotIndex >= _slotCount )
        {
            return false;
        }

        if( token.Buffer.BufferId != _bufferId )
        {
            return false;
        }

        if( _slotSize == 0 )
        {
            return false;
        }

        if( (token.Buffer.Offset % _slotSize) != 0 )
        {
            return false;
        }

        const uint64_t expectedOffset = static_cast<uint64_t>(token.SlotIndex) * static_cast<uint64_t>(_slotSize);

        if( expectedOffset > static_cast<uint64_t>((std::numeric_limits<ULONG>::max)()) )
        {
            return false;
        }

        if( token.Buffer.Offset != static_cast<ULONG>(expectedOffset) )
        {
            return false;
        }

        if( token.Buffer.Length != static_cast<ULONG>(_slotSize) )
        {
            return false;
        }

        const uint64_t endOffset = static_cast<uint64_t>(token.Buffer.Offset) + static_cast<uint64_t>(token.Buffer.Length);

        if( endOffset > static_cast<uint64_t>(_totalSize) )
        {
            return false;
        }

        const uint64_t currentGeneration = _generation[token.SlotIndex].load(std::memory_order_acquire);

        if( currentGeneration != token.Generation )
        {
            return false;
        }

        SlotState expected = SlotState::InUse;

        if( !_state[token.SlotIndex].compare_exchange_strong(expected, SlotState::Free, std::memory_order_acq_rel, std::memory_order_acquire) )
        {
            return false;
        }

        _freeStack.Push(token.SlotIndex);

        const uint32_t previousOutstanding = _outstanding.fetch_sub(1, std::memory_order_acq_rel);

        assert(previousOutstanding > 0);

        if( previousOutstanding == 1 )
        {
            std::lock_guard<std::mutex> lock(_shutdownMutex);
            _shutdownCv.notify_all();
        }

        return true;
    }

public:
    //***************************************************************************
    // @brief 버퍼의 안전한 종료(Shutdown)를 수행합니다 (신규 할당 차단 및 대기).
    //***************************************************************************
    void Shutdown() noexcept
    {
        BeginShutdown();

        WaitForAllocators();
        WaitForOutstanding();

        DestroyRegisteredBuffer();
    }

    //***************************************************************************
    // @brief 현재 셧다운 상태인지 확인합니다.
    // @return true: 셧다운 진행/완료됨, false: 정상 운영 중
    //***************************************************************************
    bool IsShutdown() const noexcept
    {
        return (_allocationState.load(std::memory_order_acquire) & kShutdownBit) != 0;
    }

    //***************************************************************************
    // @brief 현재 활성화된 할당기(Allocator) 스레드 수를 조회합니다.
    // @return Active allocator 카운트
    //***************************************************************************
    uint64_t GetActiveAllocatorCount() const noexcept
    {
        return _allocationState.load(std::memory_order_acquire) & kActiveAllocatorMask;
    }

    //***************************************************************************
    // @brief 현재 진행 중이거나 반환되지 않은 outstanding slot 개수를 조회합니다.
    // @return Outstanding 개수
    //***************************************************************************
    uint32_t GetOutstandingCount() const noexcept
    {
        return _outstanding.load(std::memory_order_acquire);
    }

    //***************************************************************************
    // Getter 메서드 그룹
    //***************************************************************************

    //***************************************************************************
    // @brief OS에 등록된 RIO 버퍼의 고유 ID를 반환합니다.
    // @return RIO_BUFFERID 값
    //***************************************************************************
    RIO_BUFFERID GetBufferId() const noexcept { return _bufferId; }

    //***************************************************************************
    // @brief RIO 버퍼가 할당된 전체 연속 메모리 영역의 시작 주소를 반환합니다.
    // @return 메모리 시작 주소 포인터
    //***************************************************************************
    char* GetBufferPtr() const noexcept { return _bufferPtr; }

    //***************************************************************************
    // @brief 전체 메모리 영역의 총 바이트 크기를 반환합니다.
    // @return 전체 바이트 크기
    //***************************************************************************
    uint32_t GetTotalSize() const noexcept { return _totalSize; }

    //***************************************************************************
    // @brief 버퍼가 관리하는 총 슬롯의 개수를 반환합니다.
    // @return 총 슬롯 개수
    //***************************************************************************
    uint32_t GetSlotCount() const noexcept { return _slotCount; }

    //***************************************************************************
    // @brief 각 슬롯 하나의 바이트 크기를 반환합니다.
    // @return 슬롯 당 바이트 크기
    //***************************************************************************
    uint32_t GetSlotSize() const noexcept { return _slotSize; }

    //***************************************************************************
    // @brief 현재 가용한 슬롯의 대략적인 개수를 조회합니다.
    // @return 가용한 슬롯 수
    //***************************************************************************
    uint32_t GetFreeCount() const noexcept
    {
        const uint32_t outstanding = _outstanding.load(std::memory_order_acquire);

        return outstanding <= _slotCount ? _slotCount - outstanding : 0;
    }

private:
    //***************************************************************************
    // @brief 셧다운 이전에 원자적으로 할당 진입 권한을 획득합니다.
    // @return true: 진입 성공, false: 셧다운 상태여서 진입 거부
    //***************************************************************************
    bool TryEnterAllocation() noexcept
    {
        uint64_t state = _allocationState.load(std::memory_order_acquire);

        for( ;;)
        {
            if( (state & kShutdownBit) != 0 )
            {
                return false;
            }

            const uint64_t activeCount = state & kActiveAllocatorMask;

            if( activeCount == kActiveAllocatorMask )
            {
                return false;
            }

            const uint64_t newState = state + 1;

            if( _allocationState.compare_exchange_weak(state, newState, std::memory_order_acquire, std::memory_order_relaxed) )
            {
                return true;
            }
        }
    }

    //***************************************************************************
    // @brief AllocSlot 작업 완료 후 Active Allocator 카운트를 감소시킵니다.
    //***************************************************************************
    void LeaveAllocation() noexcept
    {
        const uint64_t previous = _allocationState.fetch_sub(1, std::memory_order_release);

        assert((previous & kActiveAllocatorMask) != 0);

        const uint64_t previousActiveCount = previous & kActiveAllocatorMask;
        const bool shutdownStarted = (previous & kShutdownBit) != 0;

        if( shutdownStarted && previousActiveCount == 1 )
        {
            std::lock_guard<std::mutex> lock(_shutdownMutex);
            _shutdownCv.notify_all();
        }
    }

    //***************************************************************************
    // @brief 셧다운 비트를 원자적으로 설정하여 신규 할당 진입을 원천 차단합니다.
    // @return true: 이번 호출로 최초 셧다운 진입, false: 이미 셧다운 상태였음
    //***************************************************************************
    bool BeginShutdown() noexcept
    {
        uint64_t state = _allocationState.load(std::memory_order_acquire);

        for( ;;)
        {
            if( (state & kShutdownBit) != 0 )
            {
                return false;
            }

            const uint64_t newState = state | kShutdownBit;

            if( _allocationState.compare_exchange_weak(state, newState, std::memory_order_acq_rel, std::memory_order_acquire) )
            {
                return true;
            }
        }
    }

    //***************************************************************************
    // @brief 진행 중인 모든 AllocSlot 호출이 종료될 때까지 대기합니다.
    //***************************************************************************
    void WaitForAllocators()
    {
        if( GetActiveAllocatorCount() == 0 )
        {
            return;
        }

        std::unique_lock<std::mutex> lock(_shutdownMutex);

        _shutdownCv.wait(lock, [this]()
            {
                return GetActiveAllocatorCount() == 0;
            });
    }

    //***************************************************************************
    // @brief 모든 outstanding 슬롯이 반환될 때까지 대기합니다.
    //***************************************************************************
    void WaitForOutstanding()
    {
        if( _outstanding.load(std::memory_order_acquire) == 0 )
        {
            return;
        }

        std::unique_lock<std::mutex> lock(_shutdownMutex);

        _shutdownCv.wait(lock, [this]()
            {
                return _outstanding.load(std::memory_order_acquire) == 0;
            });
    }

    //***************************************************************************
    // @brief 등록된 RIO 버퍼와 정렬 메모리를 안전하게 해제합니다.
    //***************************************************************************
    void DestroyRegisteredBuffer() noexcept
    {
        if( !_initialized.exchange(false, std::memory_order_acq_rel) )
        {
            return;
        }

        assert(GetActiveAllocatorCount() == 0);
        assert(_outstanding.load(std::memory_order_acquire) == 0);

        if( _bufferId != RIO_INVALID_BUFFERID )
        {
            if( _rioTable != nullptr )
            {
                _rioTable->RIODeregisterBuffer(_bufferId);
            }

            _bufferId = RIO_INVALID_BUFFERID;
        }

        if( _bufferPtr != nullptr )
        {
            _aligned_free(_bufferPtr);
            _bufferPtr = nullptr;
        }
    }

private:
    //***************************************************************************
    // 멤버 변수 선언
    //***************************************************************************

    // RIO 관련 핸들 및 포인터
    RIO_EXTENSION_FUNCTION_TABLE* _rioTable{ nullptr };   // RIO 확장 함수 테이블 포인터
    RIO_BUFFERID                  _bufferId{ RIO_INVALID_BUFFERID }; // OS에 등록된 RIO 메모리 영역의 고유 ID
    char* _bufferPtr{ nullptr };     // OS에 등록된 전체 연속 메모리 영역의 시작 주소

    // 버퍼 구성 설정 값
    uint32_t                      _slotCount{ 0 };           // 관리하는 총 슬롯의 개수
    uint32_t                      _slotSize{ 0 };            // 각 슬롯 하나의 바이트 크기
    uint32_t                      _totalSize{ 0 };           // 전체 메모리 영역의 총 바이트 크기 (_slotCount * _slotSize)

    // 락 프리 슬롯 인덱스 관리 스택
    CLockFreeSlotStack            _freeStack;                // 가용한 슬롯들의 인덱스를 락 프리로 관리하는 MPMC 스택

    // 슬롯별 세대 카운터 및 상태 배열 (Stale completion / Double free 방지)
    std::unique_ptr<std::atomic<uint64_t>[]> _generation;    // 각 슬롯별 재사용 세대 번호 배열 (Stale completion 방어)
    std::unique_ptr<std::atomic<SlotState>[]> _state;        // 각 슬롯별 현재 사용 상태 배열 (Free / InUse, Double free 방어)

    // 현재 사용 중인 슬롯(outstanding) 개수
    std::atomic<uint32_t>         _outstanding{ 0 };         // 현재 대여되어 사용 중이거나 I/O 진행 중인 슬롯의 총 개수

    // 할당 상태 통합 제어 아토믹 (Shutdown 플래그 + Active allocator count)
    std::atomic<uint64_t>         _allocationState{ 0 };     // 셧다운 플래그(bit 63)와 활성 할당기 수(bit 0~62)를 동시 제어하는 아토믹 변수

    // 초기화 완료 여부 플래그
    std::atomic<bool>             _initialized{ false };     // 생성자 초기화 완료 및 정상 사용 가능 여부 플래그

    // 셧다운 동기화를 위한 뮤텍스 및 조건 변수 (일반 I/O 경로에는 영향 없음)
    mutable std::mutex            _shutdownMutex;            // 셧다운 대기 및 조건 변수 동기화를 위한 뮤텍스
    std::condition_variable       _shutdownCv;               // 활성 할당기 및 outstanding 슬롯 반환을 대기하기 위한 조건 변수
};

#endif // __RIOSBUFFER_H__