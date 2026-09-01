
//***************************************************************************
// ChunkedSwapQueue.h : interface for the CChunkedSwapQueue class.
//
//***************************************************************************

#ifndef UC_CHUNKEDSWAPQUEUE_H
#define UC_CHUNKEDSWAPQUEUE_H

#include <BaseRedefineDataType.h>
#include <Containers/Queue/QueueCommon.h>
#include <Memory/Containers.h>
#include <Thread/PlatformLock.h>

#include <type_traits>

//***************************************************************************
// @class CChunkedSwapQueue
// @brief 단일 큐와 청킹(Chunking) 기능을 지원하는 스레드 세이프 스왑 큐.
// 
// @details 
// 데이터 양이 폭발적이거나 일시적으로 몰릴 때, 컨슈머가 한 번에 처리하는 양을 
// 세밀하게 조절(청킹)하여 프레임 드랍이나 과부하를 방지해야 하는 상황에 적합합니다.
// 
// 주요 사용처 및 이점:
//  - IOCP 서버의 네트워크 I/O 스레드 → 로직 스레드 간 패킷 전달 (SwapChunk로 처리량 제어)
//  - 대량의 요청이 몰릴 때 컨슈머의 부하 분산 및 스파이크 현상 방지
//  - 큐 크기(_size)를 아토믹으로 실시간 모니터링해야 하는 경우
// 
// 동시성 패턴:
//  - MPMC(Multiple Producer, Multiple Consumer)를 지원
//  - 내부 큐 접근은 단일 락으로 직렬화
//  - SwapChunk()를 통해 consumer별 처리량을 제한할 수 있음
//
// outQueue 소유권 / 스레드 안전성 계약:
//  - Swap()/SwapChunk()의 outQueue 파라미터는 이 클래스가 보호하는 대상이 아닙니다.
//    호출자가 소유하는 로컬 출력 큐이며, 동일한 outQueue 인스턴스를 여러 스레드가
//    동시에 넘기는 경우 호출자가 별도의 동기화를 제공해야 합니다.
//    권장 사용법은 스레드별로 서로 다른 local outQueue를 사용하는 것입니다.
//
// 정지(Stop) 관련 계약:
//  - Stop()은 내부 락(_lock) 안에서 플래그를 세팅하므로, Stop()이 반환한 "이후" 시작되는
//    모든 Push 계열 호출은 반드시 드롭된다(happens-before 보장). Stop()과 동시에 진행
//    중이던 Push 호출은 락 선점 순서에 따라 정지 전/후 어느 쪽으로든 처리될 수 있다.
//  - Push 계열 함수는 모두 "삽입 성공 여부"를 반환값으로 명시적으로 알려준다. 정지 상태에서
//    호출되어 드롭된 경우와 정상 성공을 반환값만으로 구분할 수 있다.
//  - Start()로 정지 상태를 해제하고 재사용할 수 있다.
//
// 예외 안전성 전제:
//  - PushBatch()는 루프 도중 실패 시 _size/items 상태를 롤백하지 않는다. 이는
//    내부 컨테이너(CQueue<T> = std::queue<T, CDeque<T>>)의 원소 삽입이 예외를
//    던지지 않는다는 전제 위에서 의도적으로 생략한 것이다. 이 전제는 다음 두 조건이
//    모두 성립할 때만 유효하다:
//      1) StlAllocator<T>::allocate()가 PoolAllocator::Alloc/AllocAligned를 거쳐
//         gpMemory(CMemory)로 위임되는 USE_GPMEMORY 빌드일 것 (이 경로는 할당
//         실패 시 ASSERT_CRASH로 종료하며 예외를 던지지 않음을 확인함).
//      2) T의 이동 생성자가 noexcept일 것 (아래 static_assert로 강제).
//    USE_GPMEMORY가 정의되지 않은 폴백 빌드(StlAllocator가 ::operator new로
//    직접 위임하는 경우)에서는 allocate()가 std::bad_alloc을 던질 수 있으므로
//    이 전제가 깨진다. 그런 빌드에서 이 큐를 사용할 계획이라면 PushBatch()의
//    예외 안전성을 재검토해야 한다.
//***************************************************************************
template<typename T>
class CChunkedSwapQueue
{
    static_assert(
        std::is_nothrow_move_constructible_v<T>,
        "CChunkedSwapQueue<T> requires nothrow move constructible T.");

public:
    CChunkedSwapQueue() = default;
    ~CChunkedSwapQueue() = default;

    //***************************************************************************
    // @brief 단일 아이템을 입력 큐에 안전하게 삽입합니다.
    // @param item 삽입할 데이터 항목
    // @return 정지 상태가 아니어서 정상 삽입되었으면 true, 정지 상태라 드롭되었으면 false
    //***************************************************************************
    bool Push(T item)
    {
        PLockGuard lock(_lock, __FUNCTION__);

        // 정지 여부 확인과 실제 삽입을 같은 락 구간 안에서 처리해,
        // Stop()과의 순서가 락 하나로 항상 직렬화되도록 한다.
        if( _stopped.load(std::memory_order_relaxed) )
            return false;

        _inQueue.push(std::move(item));
        _size.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    //***************************************************************************
    // @brief 락 안에서 푸시와 크기 증가를 원자적으로 처리하여 갱신된 전체 크기를 반환합니다.
    // @param item 삽입할 데이터 항목
    // @return 푸시 후의 전체 큐 크기(성공 시 항상 1 이상). 정지 상태라 드롭된 경우 -1을 반환한다.
    //***************************************************************************
    int64 PushAndGetSize(T item)
    {
        PLockGuard lock(_lock, __FUNCTION__);

        if( _stopped.load(std::memory_order_relaxed) )
            return -1;

        _inQueue.push(std::move(item));
        return _size.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    //***************************************************************************
    // @brief 여러 아이템을 벡터 단위로 일괄 삽입합니다.
    // @param items 삽입할 데이터 항목들이 담긴 벡터 (성공 시에만 내부가 비워짐)
    // @return 정상적으로 일괄 삽입되었으면 true. 정지 상태라 드롭되었으면 false(items는 그대로 유지)
    // @note 예외 안전성은 클래스 상단 "예외 안전성 전제" 주석을 참고할 것.
    //***************************************************************************
    bool PushBatch(CVector<T>& items)
    {
        if( items.empty() )
            return true;

        PLockGuard lock(_lock, __FUNCTION__);

        if( _stopped.load(std::memory_order_relaxed) )
            return false;

        for( auto& item : items )
        {
            _inQueue.push(std::move(item));
        }
        _size.fetch_add(static_cast<int64>(items.size()), std::memory_order_relaxed);
        items.clear();
        return true;
    }

    //***************************************************************************
    // @brief 입력 큐의 모든 요소를 출력 큐로 통째로 스왑(이동)합니다.
    // @param outQueue 데이터를 전달받을 대상 큐 (호출자 소유, 클래스 상단 계약 참고)
    //***************************************************************************
    void Swap(CQueue<T>& outQueue)
    {
        PLockGuard lock(_lock, __FUNCTION__);

        if( _inQueue.empty() )
            return;

        // outQueue가 비어있다면 컨테이너 자체를 O(1)로 통째로 스왑
        if( outQueue.empty() )
        {
            _inQueue.swap(outQueue);
        }
        else
        {
            // outQueue에 잔여물이 있는 경우에만 개별 이동
            while( !_inQueue.empty() )
            {
                outQueue.push(std::move(_inQueue.front()));
                _inQueue.pop();
            }
        }

        _size.store(0, std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 입력 큐에서 지정한 최대 개수(maxCount)만큼만 떼어와 출력 큐로 이동합니다. (청킹 스왑)
    // @note 멀티 스레드 환경에서 하나의 스레드가 백로그 전체를 독점하는 현상을 방지합니다.
    // @param outQueue 데이터를 전달받을 대상 큐 (호출자 소유, 클래스 상단 계약 참고)
    // @param maxCount 한 번에 가져올 최대 아이템 개수
    //***************************************************************************
    void SwapChunk(CQueue<T>& outQueue, size_t maxCount)
    {
        if( maxCount == 0 )
            return;

        PLockGuard lock(_lock, __FUNCTION__);

        if( _inQueue.empty() )
            return;

        size_t movedCount = 0;
        while( !_inQueue.empty() && movedCount < maxCount )
        {
            // T의 값을 이동하여 outQueue로 전달
            outQueue.push(std::move(_inQueue.front()));
            _inQueue.pop();
            ++movedCount;
        }

        // 이동시킨 만큼 전체 크기 카운터를 원자적으로 차감
        _size.fetch_sub(static_cast<int64>(movedCount), std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 큐가 비어있는지 여부를 반환합니다.
    //***************************************************************************
    bool IsEmpty() const
    {
        return _size.load(std::memory_order_relaxed) == 0;
    }

    //***************************************************************************
    // @brief 현재 큐에 대기 중인 전체 아이템 개수를 반환합니다.
    //***************************************************************************
    int64 GetSize() const
    {
        return _size.load(std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 큐를 정지시키고 추가 푸시를 차단합니다.
    // @details 내부 락(_lock) 안에서 플래그를 세팅하므로, 이 함수가 반환한 이후에
    //          시작되는 모든 Push 계열 호출은 반드시 드롭된다(happens-before 보장).
    // @note Start()는 큐를 비우거나 기존 데이터를 폐기하지 않습니다.
    //***************************************************************************
    void Stop()
    {
        PLockGuard lock(_lock, __FUNCTION__);
        _stopped.store(true, std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 정지 상태를 해제해 다시 Push를 받을 수 있게 합니다.
    // @details 큐 내용물 자체는 건드리지 않는다 — 재사용 전 비우려면 Swap()/SwapChunk()를
    //          별도로 호출해야 한다.
    //***************************************************************************
    void Start()
    {
        PLockGuard lock(_lock, __FUNCTION__);
        _stopped.store(false, std::memory_order_relaxed);
    }

    //***************************************************************************
    // @brief 현재 정지 상태인지 조회합니다.
    //***************************************************************************
    bool IsStopped() const
    {
        return _stopped.load(std::memory_order_relaxed);
    }

    CChunkedSwapQueue(const CChunkedSwapQueue&) = delete;
    CChunkedSwapQueue& operator=(const CChunkedSwapQueue&) = delete;
    CChunkedSwapQueue(CChunkedSwapQueue&&) = delete;
    CChunkedSwapQueue& operator=(CChunkedSwapQueue&&) = delete;

private:
    PLock                   _lock;              // 플랫폼 통합 단독 락 객체
    CQueue<T>               _inQueue;           // 내부 입력을 받는 큐 버퍼
    std::atomic<int64>      _size{ 0 };         // 락 경합 없는 빠른 크기 조회를 위한 아토믹 카운터
    std::atomic<bool>       _stopped{ false };  // 정지 플래그. Stop/Start는 _lock으로 직렬화, 조회는 lock-free
};

#endif // ndef UC_CHUNKEDSWAPQUEUE_H