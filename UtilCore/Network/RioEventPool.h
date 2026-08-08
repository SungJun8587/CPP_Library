
//***************************************************************************
// RioEventPool.h : Hot Path 할당 오버헤드를 없애는 고성능 이벤트 풀
//
//***************************************************************************

#ifndef __RIOEVENTPOOL_H__
#define __RIOEVENTPOOL_H__

#ifndef	__CONTAINERS_H__
#include <Memory/Containers.h>
#endif

#ifndef	__RIOEVENT_H__
#include <Network/RioEvent.h>
#endif

#ifndef	__RIOOBJECT_H__
#include <Network/RioObject.h>
#endif

#include <atomic>   // std::atomic을 위한 헤더
#include <cassert>  // 단언문(assert)을 위한 헤더
#include <new>      // std::nothrow를 위한 헤더

//***************************************************************************
// @class CRioEventPool
// @brief 매번 heap에 동적 할당/해제하는 비용을 제거하기 위한 프리 리스트 기반 풀.
// @details
//      Alloc()은 I/O를 등록하는 로직 스레드에서, Free()는 CRioCore::ProcessRioResult가
//      실행되는 RIO 워커 스레드에서 호출되므로 서로 다른 스레드가 동시에 freelist head를
//      조작합니다. 이 구현은 최소한의 스핀락으로 head 조작 구간을 보호합니다.
//***************************************************************************
class CRioEventPool
{
public:
    //***************************************************************************
    // @brief CRioEventPool 생성자
    // @details 프리 리스트 헤드와 용량, 통계 카운터들을 초기화합니다.
    //***************************************************************************
    CRioEventPool() noexcept
        : _head(nullptr)
        , _capacity(0)
        , _exhaustionCount(0)
        , _lastLogTick(0)
    {
    }

    //***************************************************************************
    // @brief CRioEventPool 소멸자
    // @details 풀이 소멸될 때 Release()를 호출하여 할당된 모든 자원을 안전하게 해제합니다.
    //***************************************************************************
    ~CRioEventPool() noexcept
    {
        Release();
    }

    //***************************************************************************
    // @brief 지정된 수량만큼 이벤트를 미리 메모리에 연속 할당하고 프리 리스트로 엮습니다.
    // @details 
    //      - std::nothrow를 사용하여 예외 전파를 차단하고 nullptr 반환 처리
    //      - 생성 실패 혹은 컨테이너 푸시 실패 시 롤백(Rollback)을 수행하여 메모리/객체 누수 원천 차단
    // @param capacity 추가로 미리 할당할 이벤트 개수
    // @return 성공 시 true, 실패 시 false 반환
    //***************************************************************************
    bool Initialize(size_t capacity)
    {
        if( capacity == 0 )
            return false;

        // 초기화 과정 중 동시 접근을 막기 위한 락 획득 (프로파일러 연동)
        PLockGuard guard(_lock, __FUNCTION__);

        // 1. std::nothrow를 이용한 원시 메모리 할당 (예외 전파 방지)
        void* rawBuffer = ::operator new(sizeof(CRioEvent) * capacity, std::nothrow);
        if( rawBuffer == nullptr )
            return false;

        CRioEvent* newBlock = static_cast<CRioEvent*>(rawBuffer);
        size_t constructedCount = 0;

        // 2. 각 이벤트 객체 기본 생성자 호출 (Placement New) 및 디버그 초기 상태 설정
        for( size_t i = 0; i < capacity; ++i )
        {
            try
            {
                ::new (&newBlock[i]) CRioEvent();
                _DEBUG_SET_INITIAL_STATE(&newBlock[i]);
                constructedCount++;
            }
            catch( ... )
            {
                // 생성 중 예외 발생 시 지금까지 생성된 객체들의 소멸자 역순 호출
                for( size_t j = 0; j < constructedCount; ++j )
                {
                    newBlock[j].~CRioEvent();
                }
                ::operator delete(rawBuffer);
                return false;
            }
        }

        // 3. CVector push_back 실패 시 롤백 처리 (메모리 누수 방지)
        MemoryBlock block;
        block.ptr = rawBuffer;
        block.count = capacity;

        try
        {
            _memoryBlocks.push_back(block);
        }
        catch( ... )
        {
            for( size_t j = 0; j < capacity; ++j )
            {
                newBlock[j].~CRioEvent();
            }
            ::operator delete(rawBuffer);
            return false;
        }

        // 4. 생성된 블록들을 기존 프리 리스트(Free List) 체인에 연결 (LIFO 구조)
        for( size_t i = 0; i < capacity; ++i )
        {
            newBlock[i].SetNextFree(_head);
            _head = &newBlock[i];
        }

        _capacity += capacity;
        return true;
    }

    //***************************************************************************
    // @brief 풀에서 가용한 CRioEvent 객체 하나를 할당받습니다.
    // @param type 비동기 작업 이벤트 타입 (Receive 또는 Send)
    // @param ownerObj 이벤트를 소유하는 연동 객체 레퍼런스 (`CRioObjectRef`)
    // @return 할당된 CRioEvent 포인터 (고갈 시 nullptr -> 상위 계층 백프레셔/연결 종료 처리 필수)
    //***************************************************************************
    CRioEvent* Alloc(CRioEvent::EventType type, CRioObjectRef ownerObj)
    {
        CRioEvent* evt = nullptr;

        {
            // [락 영역] 순수하게 프리 리스트 Pop 조작만 담당하여 경합 최소화
            PLockGuard guard(_lock, __FUNCTION__);

            if( _head != nullptr )
            {
                evt = _head;
                _head = _head->GetNextFree();

                // 디버그 상태 검증: Free 상태였음을 보장받고 InUse 상태로 전환
#ifdef _DEBUG
                assert(evt->GetDebugState() == EEventState::Free && "Double Allocation or Corrupted Event State Detected!");
                evt->SetDebugState(EEventState::InUse);
#endif
            }
        } // 락 즉시 해제 (Unlock)

        // 풀이 고갈된 경우 락 외부에서 안전하게 통계 및 쓰로틀링 로깅 처리
        if( evt == nullptr )
        {
            RecordExhaustion();
            return nullptr;
        }

        // 락 외부에서 객체 초기화 수행
        evt->Initialize(type, ownerObj);
        return evt;
    }

    //***************************************************************************
    // @brief 사용이 끝난 CRioEvent 객체를 풀에 반환합니다.
    // @details 
    //      [명확한 Lifecycle Contract 계약 유지]
    //      1. Reset(): 페이로드(데이터 및 소유권) 영역 초기화 담당
    //      2. Debug state validation: 풀(Pool)의 수명 관리 상태 검증 담당 (Double Free 방지)
    //      3. Free-list publish: 락 내부에서 프리 리스트 체인 복구 담당
    // @param evt 반환할 CRioEvent 포인터
    //***************************************************************************
    void Free(CRioEvent* evt)
    {
        if( evt == nullptr )
            return;

        // 1. 페이로드 영역 초기화 (데이터 및 소유권 정리)
        evt->Reset();

        {
            // 가용 리스트에 다시 추가하기 위한 락 획득 (Push)
            PLockGuard guard(_lock, __FUNCTION__);

            // 2. 풀 측면의 Lifecycle 계약 검증: InUse -> Free 전환 (Double Free 방지)
#ifdef _DEBUG
            assert(evt->GetDebugState() == EEventState::InUse && "Double Free Detected!");
            evt->SetDebugState(EEventState::Free);
#endif

            // 3. 프리 리스트에 반환 노드 퍼블리시
            evt->SetNextFree(_head);
            _head = evt;
        }
    }

    //***************************************************************************
    // @brief 풀이 소멸될 때 할당된 모든 메모리를 해제합니다.
    // @details 
    //      [서버 셧다운 순서 가이드라인]
    //      반드시 아래 순서가 보장된 이후에만 본 Release가 호출되어야 합니다.
    //      1. Stop Accept 
    //      2. Stop new RIO Post 
    //      3. Shutdown sockets 
    //      4. RIO Completion Drain 
    //      5. Outstanding RIO I/O == 0 확립 (이후에만 호출 가능)
    //      6. RIO CQ shutdown 
    //      7. EventPool Release (여기) 
    //      8. Object destruction
    //***************************************************************************
    void Release() noexcept
    {
        PLockGuard guard(_lock, __FUNCTION__);

        for( size_t i = 0; i < _memoryBlocks.size(); ++i )
        {
            void* rawBuffer = _memoryBlocks[i].ptr;
            size_t count = _memoryBlocks[i].count;

            if( rawBuffer != nullptr )
            {
                CRioEvent* blockPtr = static_cast<CRioEvent*>(rawBuffer);

                for( size_t j = 0; j < count; ++j )
                {
                    blockPtr[j].~CRioEvent();
                }

                ::operator delete(rawBuffer);
            }
        }

        _memoryBlocks.clear();
        _head = nullptr;
        _capacity = 0;
        _exhaustionCount.store(0, std::memory_order_relaxed);
        _lastLogTick.store(0, std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief mutable _lock 덕분에 const_cast 없이 안전하게 전체 용량을 조회합니다.
    // @return size_t 풀의 총 이벤트 수용 용량
    //***************************************************************************
    size_t GetCapacity() const
    {
        PLockGuard guard(_lock, __FUNCTION__);
        return _capacity;
    }

private:
    //***************************************************************************
    // @brief 풀 고갈 시 락 외부에서 호출되는 통계 누적 및 원자적 CAS 쓰로틀링 로깅 함수
    // @details 락 외부에서 안전하게 현재 용량(_capacity)을 조회하여 로그에 포함합니다.
    //***************************************************************************
    void RecordExhaustion()
    {
        // 1. 고갈 횟수를 원자적으로 누적
        _exhaustionCount.fetch_add(1, std::memory_order_relaxed);

        ULONGLONG currentTick = ::GetTickCount64();
        ULONGLONG lastTick = _lastLogTick.load(std::memory_order_relaxed);

        // 2. 5초(5000밀리초) 경과 여부 확인 및 CAS를 통한 단일 스레드 로깅 권한 획득 (데이터 레이스 원천 차단)
        if( currentTick - lastTick >= 5000 )
        {
            if( _lastLogTick.compare_exchange_weak(lastTick, currentTick, std::memory_order_relaxed, std::memory_order_relaxed) )
            {
                ULONGLONG missedCount = _exhaustionCount.exchange(0, std::memory_order_relaxed);

                // 락을 통해 안전하게 현재 용량 조회 후 로깅에 활용
                size_t currentCapacity = GetCapacity();

                // 상용 로그 시스템 호출 (예시)
                // LOG_ERROR("CRioEventPool Exhausted! Total occurrences in last 5 seconds: %llu. Capacity: %zu", 
                //           missedCount + 1, currentCapacity);
            }
        }
    }

#ifdef _DEBUG
    //***************************************************************************
    // @brief 디버그 빌드 전용: 생성된 이벤트 객체의 초기 상태를 Free로 설정합니다.
    // @param evt 초기화할 CRioEvent 포인터
    //***************************************************************************
    void _DEBUG_SET_INITIAL_STATE(CRioEvent* evt)
    {
        evt->SetDebugState(EEventState::Free);
    }
#else
    void _DEBUG_SET_INITIAL_STATE(CRioEvent*) {}
#endif

private:
    //***************************************************************************
    // @brief OS로부터 크게 할당받은 원본 메모리 블록 정보를 관리하기 위한 구조체
    //***************************************************************************
    struct MemoryBlock
    {
        void* ptr;      // 할당된 원본 메모리의 바이트/시작 주소
        size_t count;   // 해당 블록에 속한 이벤트 개수
    };

    mutable PLock               _lock;            // SRWLock 기반의 플랫폼 공용 락(데드락 프로파일러 연동 지원)
    CRioEvent*                  _head;            // 가용 이벤트 객체들을 잇는 단일 연결 리스트(Free List)의 시작점
    size_t                      _capacity;        // 풀의 전체 용량
    CVector<MemoryBlock>        _memoryBlocks;    // OS로부터 할당받은 원본 메모리 블록들을 추적하기 위한 벡터

    std::atomic<ULONGLONG>      _exhaustionCount; // 풀 고갈 횟수를 안전하게 추적하기 위한 원자적 카운터
    std::atomic<ULONGLONG>      _lastLogTick;     // 마지막으로 로그를 출력한 시각 (데이터 레이스 방지를 위해 atomic 적용)
};

#endif // __RIOEVENTPOOL_H__