
//***************************************************************************
// RioBuffer.cpp : implementation of the CRioBuffer class.
//
//***************************************************************************

#include "pch.h"
#include "RioBuffer.h"

//***************************************************************************
// @brief CRioBuffer 기본 생성자
//***************************************************************************
CRioBuffer::CRioBuffer() noexcept
    : _buffer(nullptr)
    , _bufferId(RIO_INVALID_BUFFERID)
    , _slotCount(0)
    , _slotSize(0)
    , _totalSize(0)
    , _alignment(0)
    , _slotState(nullptr)
    , _freeStack(nullptr)
    , _allocatedCount(0)
    , _initialized(false)
{
}

//***************************************************************************
// @brief CRioBuffer 소멸자
//
// @details
//     Shutdown()을 호출하여 RIO resource를 안전하게 해제합니다.
//
//     아직 slot이 남아 있는 경우 Shutdown()은 false를 반환하며,
//     이 상태에서 객체가 소멸하면 resource lifetime invariant 위반으로
//     판단하여 assert 후 terminate합니다.
//
//***************************************************************************
CRioBuffer::~CRioBuffer() noexcept
{
    const bool shutdownResult = Shutdown();

    if( !shutdownResult )
    {
        assert(false && "CRioBuffer destroyed while resource is still active");

        // 절대로 _buffer를 강제로 해제하지 않습니다.
        //
        // outstanding slot이 존재하는 상태에서 강제 해제하면
        // caller가 보유한 RIO_BUF가 dangling resource가 됩니다.
        std::terminate();
    }
}

//***************************************************************************
// @brief Power-of-two 여부 확인
// @param value 검증할 값
// @return bool 2의 거듭제곱 여부
//***************************************************************************
bool CRioBuffer::IsPowerOfTwo(size_t value) noexcept
{
    if( value == 0 ) return false;
    return (value & (value - 1)) == 0;
}

//***************************************************************************
// @brief 메모리 정렬값 검증
// @param alignment 검증할 정렬 바이트 크기
// @return bool 정렬 값 유효성 여부
//
// @details
//     _aligned_malloc()에서 사용할 수 있도록 power-of-two 정렬을
//     요구합니다.
//
//***************************************************************************
bool CRioBuffer::IsValidAlignment(size_t alignment) noexcept
{
    if( alignment < sizeof(void*) ) return false;
    if( !IsPowerOfTwo(alignment) ) return false;
    return true;
}

//***************************************************************************
// @brief slot index 검증
// @param slotIndex 검증할 슬롯 인덱스
// @return bool 슬롯 인덱스 유효성 여부
//
// @note
//     호출자는 lifecycle shared lock을 확보한 상태에서 호출합니다.
//***************************************************************************
bool CRioBuffer::ValidateSlotIndex(uint32_t slotIndex) const noexcept
{
    if( !_initialized ) return false;
    if( slotIndex >= _slotCount ) return false;
    if( _slotState == nullptr || _freeStack == nullptr ) return false;
    if( _buffer == nullptr || _bufferId == RIO_INVALID_BUFFERID ) return false;

    return true;
}

//***************************************************************************
// @brief Initialize 파라미터 검증
// @param slotCount 전체 슬롯 개수
// @param slotSize 개별 슬롯 크기 (Bytes)
// @param alignment 메모리 정렬 단위 (Bytes)
// @return bool 파라미터 유효성 여부
//***************************************************************************
bool CRioBuffer::ValidateBufferParameters(uint32_t slotCount, uint32_t slotSize, size_t alignment) const noexcept
{
    if( slotCount == 0 || slotSize == 0 ) return false;
    if( !IsValidAlignment(alignment) ) return false;

    // size_t overflow 검사
    const size_t count = static_cast<size_t>(slotCount);
    const size_t size = static_cast<size_t>(slotSize);

    if( count > std::numeric_limits<size_t>::max() / size ) return false;

    const size_t totalSize = count * size;
    if( totalSize == 0 ) return false;

    return true;
}

//***************************************************************************
// @brief RIO Function Table 검증
// @param rioTable 검증할 RIO 확장 함수 테이블 포인터
// @return bool 테이블 구조체 및 필수 함수 포인터 유효성 여부
// @details
//     CRioBuffer는 RIO extension table을 소유하지 않습니다.
//     따라서 단순히 pointer가 존재하는지만 검사하지 않고
//     실제 Buffer 등록/해제에 필요한 함수 포인터까지 검증합니다.
//***************************************************************************
bool CRioBuffer::ValidateRioTable(const RIO_EXTENSION_FUNCTION_TABLE* rioTable) const noexcept
{
    if( rioTable == nullptr )
        return false;

    if( rioTable->cbSize < sizeof(RIO_EXTENSION_FUNCTION_TABLE) )
        return false;

    if( rioTable->RIORegisterBuffer == nullptr )
        return false;

    if( rioTable->RIODeregisterBuffer == nullptr )
        return false;

    return true;
}

//***************************************************************************
// @brief aligned memory를 할당합니다.
// @param totalSize 할당할 전체 메모리 크기 (Bytes)
// @param alignment 메모리 정렬 단위 (Bytes)
// @return bool 메모리 할당 성공 여부
//***************************************************************************
bool CRioBuffer::AllocateMemory(size_t totalSize, size_t alignment) noexcept
{
    if( totalSize == 0 || !IsValidAlignment(alignment) ) return false;

    void* buffer = ::_aligned_malloc(totalSize, alignment);
    if( buffer == nullptr ) return false;

    _buffer = buffer;
    return true;
}

//***************************************************************************
// @brief aligned memory를 해제합니다.
//***************************************************************************
void CRioBuffer::ReleaseMemory() noexcept
{
    if( _buffer != nullptr )
    {
        ::_aligned_free(_buffer);
        _buffer = nullptr;
    }
}

//***************************************************************************
// @brief RIO Registered Buffer 등록
// @return bool RIO 버퍼 등록 성공 여부
//***************************************************************************
bool CRioBuffer::RegisterBuffer() noexcept
{
    if( _rioTable == nullptr )
        return false;

    if( _rioTable->RIORegisterBuffer == nullptr )
        return false;

    if( _buffer == nullptr || _totalSize == 0 )
        return false;

    // RIORegisterBuffer()의 DataLength는 DWORD입니다.
    if( _totalSize > static_cast<size_t>(std::numeric_limits<DWORD>::max()) )
        return false;

    const RIO_BUFFERID bufferId =
        _rioTable->RIORegisterBuffer(
            static_cast<PCHAR>(_buffer),
            static_cast<DWORD>(_totalSize)
        );

    if( bufferId == RIO_INVALID_BUFFERID )
    {
        _bufferId = RIO_INVALID_BUFFERID;
        return false;
    }

    _bufferId = bufferId;

    return true;
}

//***************************************************************************
// @brief RIO Registered Buffer 등록 해제
//***************************************************************************
void CRioBuffer::UnregisterBuffer() noexcept
{
    if( _bufferId == RIO_INVALID_BUFFERID )
        return;

    if( _rioTable != nullptr &&
        _rioTable->RIODeregisterBuffer != nullptr )
    {
        _rioTable->RIODeregisterBuffer(_bufferId);
    }

    _bufferId = RIO_INVALID_BUFFERID;
}

//***************************************************************************
// @brief 런타임 상태를 초기화합니다.
//***************************************************************************
void CRioBuffer::ResetRuntimeState() noexcept
{
    _rioTable = nullptr;

    _buffer = nullptr;
    _bufferId = RIO_INVALID_BUFFERID;

    _slotCount = 0;
    _slotSize = 0;
    _totalSize = 0;
    _alignment = 0;

    _slotState.reset();
    _freeStack.reset();

    _allocatedCount.store(0, std::memory_order_relaxed);
    _initialized = false;
}

//***************************************************************************
// @brief RIO Buffer 초기화
// @param rioTable CRioCore가 소유하는 RIO_EXTENSION_FUNCTION_TABLE.
// @param slotCount 전체 슬롯 개수
// @param slotSize 개별 슬롯 크기 (Bytes)
// @param alignment 메모리 정렬 단위 (Bytes)
// @return bool 초기화 성공 여부
//***************************************************************************
bool CRioBuffer::Initialize(const RIO_EXTENSION_FUNCTION_TABLE* rioTable, uint32_t slotCount, uint32_t slotSize, size_t alignment) noexcept
{
    std::unique_lock<std::shared_mutex> lifecycleLock(_lifecycleMutex);

    //***********************************************************************
    // 이미 초기화된 경우 중복 초기화 방지
    //***********************************************************************
    if( _initialized )
        return false;

    //***********************************************************************
    // RIO function table 검증
    //***********************************************************************
    if( !ValidateRioTable(rioTable) )
        return false;

    //***********************************************************************
    // 이전 초기화 실패 후 남아 있는 비정상 상태 방어
    //***********************************************************************
    if( _buffer != nullptr || _bufferId != RIO_INVALID_BUFFERID || _slotState != nullptr || _freeStack != nullptr )
    {
        assert(false && "CRioBuffer contains stale runtime resources");
        // 안전하게 여기서 강제 해제하지 않습니다.
        return false;
    }

    //***********************************************************************
    // parameter validation
    //***********************************************************************
    if( !ValidateBufferParameters(slotCount, slotSize, alignment) ) return false;

    const size_t count = static_cast<size_t>(slotCount);
    const size_t size = static_cast<size_t>(slotSize);
    const size_t totalSize = count * size;

    //***********************************************************************
    // RIORegisterBuffer는 DWORD 길이를 사용합니다.
    //***********************************************************************
    if( totalSize > static_cast<size_t>(std::numeric_limits<DWORD>::max()) ) return false;

    //***********************************************************************
    // RIO function table 연결
    //
    // CRioBuffer는 ownership을 갖지 않습니다.
    // CRioCore의 table lifetime이 CRioBuffer보다 길어야 합니다.
    //***********************************************************************
    _rioTable = rioTable;

    //***********************************************************************
    // configuration 설정
    //***********************************************************************
    _slotCount = slotCount;
    _slotSize = slotSize;
    _totalSize = totalSize;
    _alignment = alignment;

    _allocatedCount.store(0, std::memory_order_relaxed);

    //***********************************************************************
    // Buffer memory allocation
    //***********************************************************************
    if( !AllocateMemory(_totalSize, _alignment) )
    {
        ResetRuntimeState();
        return false;
    }

    //***********************************************************************
    // Slot state allocation
    //***********************************************************************
    try
    {
        _slotState = std::make_unique<std::atomic<uint8_t>[]>(slotCount);
    }
    catch( ... )
    {
        ReleaseMemory();
        ResetRuntimeState();
        return false;
    }

    //***********************************************************************
    // 모든 slot은 최초 Free 상태
    //***********************************************************************
    for( uint32_t i = 0; i < slotCount; ++i )
    {
        _slotState[i].store(static_cast<uint8_t>(Rio::SlotState::Free), std::memory_order_relaxed);
    }

    //***********************************************************************
    // Lock-free free slot stack 생성
    //
    // CLockFreeSlotStack constructor가 내부 free list를
    // 0 -> 1 -> 2 -> ... -> N-1 순으로 구성합니다.
    //***********************************************************************
    try
    {
        _freeStack = std::make_unique<CLockFreeSlotStack>(slotCount);
    }
    catch( ... )
    {
        _slotState.reset();
        ReleaseMemory();
        ResetRuntimeState();
        return false;
    }

    //***********************************************************************
    // RIO Registered Buffer 등록
    //***********************************************************************
    if( !RegisterBuffer() )
    {
        _freeStack.reset();
        _slotState.reset();
        ReleaseMemory();
        ResetRuntimeState();
        return false;
    }

    //***********************************************************************
    // 모든 초기화가 완료된 이후에만 initialized=true
    //***********************************************************************
    _initialized = true;

    return true;
}

//***************************************************************************
// @brief 사용 가능한 slot 하나를 할당합니다.
// @param[out] outSlotIndex 할당된 슬롯의 인덱스가 저장될 참조 변수
// @return bool 슬롯 할당 성공 여부
//
// @details
//
//     lifecycle shared lock을 잡은 상태에서 전체 작업을 수행합니다.
//
//     따라서:
//
//         AllocSlot()
//             shared lock
//                 Pop()
//                 state CAS
//                 allocatedCount++
//
//     가 하나의 lifetime domain 안에 존재합니다.
//
//     Shutdown()은 exclusive lock이므로 위 작업이 완료되기 전에는
//     내부 buffer를 파괴할 수 없습니다.
//
//***************************************************************************
bool CRioBuffer::AllocSlot(uint32_t& outSlotIndex) noexcept
{
    outSlotIndex = Rio::kInvalidSlotIndex;

    std::shared_lock<std::shared_mutex> lifecycleLock(_lifecycleMutex);

    if( !_initialized ) return false;
    if( _freeStack == nullptr || _slotState == nullptr ) return false;
    if( _buffer == nullptr || _bufferId == RIO_INVALID_BUFFERID ) return false;

    uint32_t slotIndex = Rio::kInvalidSlotIndex;

    //***********************************************************************
    // Lock-free free list에서 slot 하나 Pop
    //***********************************************************************
    if( !_freeStack->Pop(slotIndex) ) return false;

    //***********************************************************************
    // 방어적 index validation
    //
    // 정상적인 CLockFreeSlotStack에서는 발생하지 않아야 합니다.
    //***********************************************************************
    if( slotIndex >= _slotCount )
    {
        assert(false && "CLockFreeSlotStack returned invalid slot index");

        // stack이 이미 해당 slot을 제거했기 때문에 여기서 Push하여 복구합니다.
        _freeStack->Push(slotIndex);
        return false;
    }

    //***********************************************************************
    // Free -> Allocated CAS
    //
    // 정상적인 free stack invariant가 유지된다면 반드시 성공해야 합니다.
    //***********************************************************************
    uint8_t expected = static_cast<uint8_t>(Rio::SlotState::Free);
    const bool stateChanged = _slotState[slotIndex].compare_exchange_strong(
        expected, static_cast<uint8_t>(Rio::SlotState::Allocated),
        std::memory_order_acq_rel, std::memory_order_acquire);

    if( !stateChanged )
    {
        assert(false && "Free-list slot is not in Free state");

        // 내부 invariant가 깨졌으므로 slot을 다시 넣지 않습니다.
        // 이미 상태가 Allocated인 slot을 Push하면 double ownership이 발생할 수 있기 때문입니다.
        return false;
    }

    //***********************************************************************
    // allocated count 증가
    //***********************************************************************
    const uint32_t previous = _allocatedCount.fetch_add(1, std::memory_order_acq_rel);

    if( previous == std::numeric_limits<uint32_t>::max() )
    {
        assert(false && "CRioBuffer allocated count overflow");

        // 이론적으로 slotCount가 uint32_t 범위이므로 실제 발생하지 않아야 합니다.
        // 그래도 방어적으로 state를 원복합니다.
        _allocatedCount.fetch_sub(1, std::memory_order_acq_rel);

        uint8_t allocated = static_cast<uint8_t>(Rio::SlotState::Allocated);
        const bool reverted = _slotState[slotIndex].compare_exchange_strong(
            allocated, static_cast<uint8_t>(Rio::SlotState::Free),
            std::memory_order_acq_rel, std::memory_order_acquire);

        if( !reverted )
        {
            assert(false && "Failed to rollback slot allocation state");
        }

        _freeStack->Push(slotIndex);
        return false;
    }

    outSlotIndex = slotIndex;
    return true;
}

//***************************************************************************
// @brief 사용한 slot을 free list에 반환합니다.
// @param slotIndex 반납할 슬롯 인덱스
// @return bool 슬롯 반납 성공 여부
//
// @details
//
//     CLockFreeSlotStack::Push()는 void 반환형이므로:
//
//         const bool pushed = _freeStack->Push(slotIndex);
//
//     와 같은 코드를 사용하지 않습니다.
//
//     단순히:
//
//         _freeStack->Push(slotIndex);
//
//     로 호출합니다.
//
//***************************************************************************
bool CRioBuffer::FreeSlot(uint32_t slotIndex) noexcept
{
    std::shared_lock<std::shared_mutex> lifecycleLock(_lifecycleMutex);

    if( !_initialized ) return false;
    if( _freeStack == nullptr || _slotState == nullptr ) return false;
    if( _buffer == nullptr || _bufferId == RIO_INVALID_BUFFERID ) return false;
    if( slotIndex >= _slotCount )
    {
        assert(false && "CRioBuffer::FreeSlot invalid slot index");
        return false;
    }

    //***********************************************************************
    // Allocated -> Free CAS
    //
    // 이미 Free 상태라면 double-free로 판단합니다.
    //***********************************************************************
    uint8_t expected = static_cast<uint8_t>(Rio::SlotState::Allocated);
    const bool stateChanged = _slotState[slotIndex].compare_exchange_strong(
        expected, static_cast<uint8_t>(Rio::SlotState::Free),
        std::memory_order_acq_rel, std::memory_order_acquire);

    if( !stateChanged )
    {
        assert(false && "CRioBuffer::FreeSlot double-free or stale slot");
        return false;
    }

    //***********************************************************************
    // free list에 반환
    //
    // Push()는 void 반환형이므로 반환값 검사를 하지 않습니다.
    // CLockFreeSlotStack의 구현상 유효한 index에 대한 Push는 CAS loop를 통해 성공할 때까지 재시도합니다.
    //***********************************************************************
    _freeStack->Push(slotIndex);

    //***********************************************************************
    // allocated count 감소
    //***********************************************************************
    const uint32_t previous = _allocatedCount.fetch_sub(1, std::memory_order_acq_rel);

    if( previous == 0 )
    {
        assert(false && "CRioBuffer allocated count underflow");

        // 논리적으로는 발생해서는 안 됩니다.
        // fetch_sub 자체는 이미 수행되었기 때문에 값을 복구합니다.
        _allocatedCount.fetch_add(1, std::memory_order_acq_rel);
        return false;
    }

    return true;
}

//***************************************************************************
// @brief RIO_BUF descriptor를 생성합니다.
// @param slotIndex 대상 슬롯 인덱스
// @param[out] outBuffer 생성된 RIO_BUF 디스크립터가 저장될 참조 변수
// @return bool RIO_BUF 디스크립터 생성 성공 여부
//
// @details
//
//     반환되는 RIO_BUF:
//
//         BufferId = 등록된 RIO_BUFFERID
//         Offset   = slotIndex * slotSize
//         Length   = slotSize
//
//     RIO API에 직접 전달할 수 있는 descriptor입니다.
//
//***************************************************************************
bool CRioBuffer::GetRioBuffer(uint32_t slotIndex, RIO_BUF& outBuffer) const noexcept
{
    outBuffer.BufferId = RIO_INVALID_BUFFERID;
    outBuffer.Offset = 0;
    outBuffer.Length = 0;

    std::shared_lock<std::shared_mutex> lifecycleLock(_lifecycleMutex);

    if( !ValidateSlotIndex(slotIndex) ) return false;

    //***********************************************************************
    // slot은 caller가 AllocSlot()으로 확보한 상태여야 합니다.
    //***********************************************************************
    const uint8_t state = _slotState[slotIndex].load(std::memory_order_acquire);
    if( state != static_cast<uint8_t>(Rio::SlotState::Allocated) )
    {
        assert(false && "GetRioBuffer called for non-allocated slot");
        return false;
    }

    //***********************************************************************
    // offset overflow 방어
    //***********************************************************************
    const size_t slotIndexSize = static_cast<size_t>(slotIndex);
    const size_t slotSize = static_cast<size_t>(_slotSize);

    if( slotIndexSize > std::numeric_limits<size_t>::max() / slotSize )
    {
        assert(false && "RIO buffer offset overflow");
        return false;
    }

    const size_t offset = slotIndexSize * slotSize;

    if( offset >= _totalSize )
    {
        assert(false && "RIO buffer offset exceeds registered buffer");
        return false;
    }

    if( static_cast<size_t>(_slotSize) > _totalSize - offset )
    {
        assert(false && "RIO buffer length exceeds registered buffer");
        return false;
    }

    // RIO_BUF.Offset / Length는 ULONG입니다.
    if( offset > static_cast<size_t>(std::numeric_limits<ULONG>::max()) )
    {
        assert(false && "RIO buffer offset exceeds ULONG");
        return false;
    }

    if( static_cast<size_t>(_slotSize) > static_cast<size_t>(std::numeric_limits<ULONG>::max()) )
    {
        assert(false && "RIO buffer slot size exceeds ULONG");
        return false;
    }

    //***********************************************************************
    // 최종 descriptor 생성
    //***********************************************************************
    outBuffer.BufferId = _bufferId;
    outBuffer.Offset = static_cast<ULONG>(offset);
    outBuffer.Length = static_cast<ULONG>(_slotSize);

    return true;
}

//***************************************************************************
// @brief slot 시작 주소 반환
// @param slotIndex 대상 슬롯 인덱스
// @return void* 슬롯 메모리 시작 주소 (유효하지 않을 경우 nullptr)
//***************************************************************************
void* CRioBuffer::GetSlotAddress(uint32_t slotIndex) noexcept
{
    std::shared_lock<std::shared_mutex> lifecycleLock(_lifecycleMutex);

    if( !ValidateSlotIndex(slotIndex) ) return nullptr;

    const uint8_t state = _slotState[slotIndex].load(std::memory_order_acquire);
    if( state != static_cast<uint8_t>(Rio::SlotState::Allocated) ) return nullptr;

    const size_t index = static_cast<size_t>(slotIndex);
    const size_t size = static_cast<size_t>(_slotSize);

    if( index > std::numeric_limits<size_t>::max() / size ) return nullptr;

    const size_t offset = index * size;
    if( offset >= _totalSize || size > _totalSize - offset ) return nullptr;

    return static_cast<uint8_t*>(_buffer) + offset;
}

//***************************************************************************
// @brief const slot 시작 주소 반환
// @param slotIndex 대상 슬롯 인덱스
// @return const void* 슬롯 메모리 시작 주소 (유효하지 않을 경우 nullptr)
//***************************************************************************
const void* CRioBuffer::GetSlotAddress(uint32_t slotIndex) const noexcept
{
    std::shared_lock<std::shared_mutex> lifecycleLock(_lifecycleMutex);

    if( !ValidateSlotIndex(slotIndex) ) return nullptr;

    const uint8_t state = _slotState[slotIndex].load(std::memory_order_acquire);
    if( state != static_cast<uint8_t>(Rio::SlotState::Allocated) ) return nullptr;

    const size_t index = static_cast<size_t>(slotIndex);
    const size_t size = static_cast<size_t>(_slotSize);

    if( index > std::numeric_limits<size_t>::max() / size ) return nullptr;

    const size_t offset = index * size;
    if( offset >= _totalSize || size > _totalSize - offset ) return nullptr;

    return static_cast<const uint8_t*>(_buffer) + offset;
}

//***************************************************************************
// @brief 현재 free slot 개수를 반환합니다.
// @return uint32_t 사용 가능한 슬롯 개수
//***************************************************************************
uint32_t CRioBuffer::GetFreeCount() const noexcept
{
    std::shared_lock<std::shared_mutex> lifecycleLock(_lifecycleMutex);

    if( !_initialized ) return 0;

    const uint32_t allocated = _allocatedCount.load(std::memory_order_acquire);
    if( allocated > _slotCount )
    {
        assert(false && "CRioBuffer allocated count exceeds slot count");
        return 0;
    }

    return _slotCount - allocated;
}

//***************************************************************************
// @brief 특정 slot이 Allocated 상태인지 확인합니다.
// @param slotIndex 확인할 슬롯 인덱스
// @return bool 할당(Allocated) 상태 여부
//***************************************************************************
bool CRioBuffer::IsSlotAllocated(uint32_t slotIndex) const noexcept
{
    std::shared_lock<std::shared_mutex> lifecycleLock(_lifecycleMutex);

    if( !_initialized || slotIndex >= _slotCount || _slotState == nullptr ) return false;

    return _slotState[slotIndex].load(std::memory_order_acquire) == static_cast<uint8_t>(Rio::SlotState::Allocated);
}

//***************************************************************************
// @brief RIO Buffer Shutdown
// @return bool 종료 처리 성공 여부
//
// @details
//
//     이 함수가 이번 수정에서 가장 중요한 부분입니다.
//
//     Shutdown()은 _lifecycleMutex를 exclusive로 획득합니다.
//
//     AllocSlot()/FreeSlot()은 동일 mutex를 shared로 획득하기 때문에:
//
//         Shutdown()
//             exclusive
//
//         AllocSlot()
//             shared
//
//         FreeSlot()
//             shared
//
//     이 셋은 동시에 실행될 수 없습니다.
//
//     따라서:
//
//         _allocatedCount == 0
//
//     을 확인한 이후 _slotState / _freeStack / _buffer를 해제해도
//     진행 중인 AllocSlot()/FreeSlot()이 내부 메모리에 접근할 수 없습니다.
//
//***************************************************************************
bool CRioBuffer::Shutdown() noexcept
{
    std::unique_lock<std::shared_mutex> lifecycleLock(_lifecycleMutex);

    //***********************************************************************
    // 이미 shutdown된 상태
    //***********************************************************************
    if( !_initialized )
    {
        // 완전히 비어 있는 기본 상태라면 성공으로 취급합니다.
        if( _buffer == nullptr && _bufferId == RIO_INVALID_BUFFERID &&
            _slotState == nullptr && _freeStack == nullptr &&
            _allocatedCount.load(std::memory_order_acquire) == 0 )
        {
            return true;
        }

        assert(false && "CRioBuffer has inconsistent uninitialized state");
        return false;
    }

    //***********************************************************************
    // 현재 보유 중인 slot이 있으면 절대로 resource를 파괴하지 않습니다.
    //***********************************************************************
    const uint32_t allocated = _allocatedCount.load(std::memory_order_acquire);
    if( allocated != 0 )
    {
        assert(false && "CRioBuffer::Shutdown called with outstanding slots");
        return false;
    }

    //***********************************************************************
    // defensive validation
    //***********************************************************************
    if( _slotState == nullptr )
    {
        assert(false && "CRioBuffer slot state is null during Shutdown");
        return false;
    }

    if( _freeStack == nullptr )
    {
        assert(false && "CRioBuffer free stack is null during Shutdown");
        return false;
    }

    //***********************************************************************
    // 모든 slot state가 Free인지 최종 검증
    //
    // _allocatedCount == 0과 함께 이중 검증을 수행합니다.
    //***********************************************************************
    for( uint32_t i = 0; i < _slotCount; ++i )
    {
        const uint8_t state = _slotState[i].load(std::memory_order_acquire);
        if( state != static_cast<uint8_t>(Rio::SlotState::Free) )
        {
            assert(false && "CRioBuffer slot remains allocated during Shutdown");
            return false;
        }
    }

    //***********************************************************************
    // RIO buffer 등록 해제
    //
    // 이 시점에는:
    //     AllocSlot / FreeSlot 완료
    //     allocatedCount == 0
    //     모든 slot == Free
    //
    // 이므로 RIO buffer를 더 이상 caller가 사용할 수 없습니다.
    //***********************************************************************
    UnregisterBuffer();

    //***********************************************************************
    // Lock-free stack 및 slot state 해제
    //***********************************************************************
    _freeStack.reset();
    _slotState.reset();

    //***********************************************************************
    // 실제 aligned buffer 해제
    //***********************************************************************
    ReleaseMemory();

    //***********************************************************************
    // runtime state 초기화
    //***********************************************************************
    _slotCount = 0;
    _slotSize = 0;
    _totalSize = 0;
    _alignment = 0;

    _allocatedCount.store(0, std::memory_order_release);
    _initialized = false;

    return true;
}