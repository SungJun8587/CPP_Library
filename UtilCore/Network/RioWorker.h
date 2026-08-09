
//***************************************************************************
// RioWorker.h : RIO 워커 스레드 제어 및 CQ 배치 처리 담당 클래스
//
//***************************************************************************

#ifndef __RIOWORKER_H__
#define __RIOWORKER_H__

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <mswsock.h>
#include <mswsock.h>
#include <cstdint>
#include <atomic>
#include "RioCore.h"
#include "RioEventPool.h"

//===========================================================================
// @class CRioWorker
// @brief 전용 CQ와 IOCP를 소유하고 독립된 스레드에서 비동기 I/O 완료를 폴링/대기하는 클래스.
// @details
//     - 1 Worker : 1 CQ 구조를 통해 Cross-Worker Completion 유입을 원천 차단합니다.
//     - CRioCore를 내부에 포함하여 CQ 배치 수거 및 스핀 방지 루프를 실행합니다.
//===========================================================================
class CRioWorker
{
    static constexpr uint32 kCqBurstMargin = 1024; // 트래픽 폭주 대응용 마진

public:
    static constexpr int32 kCorruptCq = CRioCore::kCorruptCq;

    //***************************************************************************
    // @brief CRioWorker 생성자
    //***************************************************************************
    CRioWorker()
        : _cqIdentifier(0), _eventPool(nullptr), _isRunning(false)
    {
    }

    //***************************************************************************
    // @brief CRioWorker 소멸자
    // @details 안전한 리소스 정리를 위해 Shutdown()을 호출합니다.
    //***************************************************************************
    ~CRioWorker()
    {
        Shutdown();
    }

    CRioWorker(const CRioWorker&) = delete;
    CRioWorker& operator=(const CRioWorker&) = delete;
    CRioWorker(CRioWorker&&) = delete;
    CRioWorker& operator=(CRioWorker&&) = delete;

    //***************************************************************************
        // @brief RIO 워커 및 내부 CRioCore 엔진을 초기화합니다.
        // @details eventPool의 용량이 이번에 계산된 totalCapacity보다 작으면 그 차이만큼
        //          자동으로 CRioEventPool::Initialize()를 추가 호출하여 CQ 용량과
        //          이벤트 풀 용량이 서로 어긋나지 않도록 맞춥니다.
        // @param representativeSocket 함수 테이블 바인딩용 대표 소켓
        // @param maxSessionCount 시스템 최대 동시 접속 세션 수
        // @param maxOutstandingRecv 세션당 최대 수신 허용치
        // @param maxOutstandingSend 세션당 최대 송신 허용치
        // @param cqIdentifier 무결성 검증을 위한 고유 CQ 키
        // @param eventPool 이벤트를 할당받고 반환할 CRioEventPool 포인터
        // @return 초기화 성공 시 true, 실패 시 false 반환
        //***************************************************************************
    bool Initialize(SOCKET representativeSocket,
        DWORD maxSessionCount,
        DWORD maxOutstandingRecv,
        DWORD maxOutstandingSend,
        ULONG_PTR cqIdentifier,
        CRioEventPool* eventPool)
    {
        if( eventPool == nullptr )
            return false;

        uint64_t totalIoPerSession = static_cast<uint64_t>(maxOutstandingRecv) + static_cast<uint64_t>(maxOutstandingSend);
        uint64_t totalCapacity = (static_cast<uint64_t>(maxSessionCount) * totalIoPerSession) + kCqBurstMargin;

        if( totalCapacity == 0 || totalCapacity > RIO_MAX_CQ_SIZE )
            return false;

        // CQ 용량과 이벤트 풀 용량이 어긋나지 않도록, 부족한 만큼 풀을 보강한다.
        size_t currentPoolCapacity = eventPool->GetCapacity();
        if( currentPoolCapacity < totalCapacity )
        {
            size_t shortfall = static_cast<size_t>(totalCapacity) - currentPoolCapacity;
            if( !eventPool->Initialize(shortfall) )
                return false;
        }

        _cqIdentifier = cqIdentifier;
        _eventPool = eventPool;

        // 내부 CRioCore 초기화
        return _rioCore.Initialize(representativeSocket, static_cast<ULONG>(totalCapacity), _cqIdentifier, _eventPool);
    }

    //***************************************************************************
    // @brief 워커 스레드의 메인 루프에서 반복 호출하여 이벤트를 처리합니다.
    // @return int32 처리된 이벤트 개수, CQ 손상 시 kCorruptCq(-1) 반환
    //***************************************************************************
    int32 ProcessLoopOnce()
    {
        if( !_isRunning.load(std::memory_order_acquire) )
            return 0;

        return _rioCore.DispatchBatch();
    }

    //***************************************************************************
    // @brief 워커 가동 상태 플래그를 활성화합니다.
    //***************************************************************************
    void Start()
    {
        _isRunning.store(true, std::memory_order_release);
    }

    //***************************************************************************
    // @brief 워커 가동을 중지하고 내부 RIO 자원을 정리합니다.
    // @details 워커 스레드가 DispatchBatch() 안(특히 GQCS 대기 중)에 있을 수 있으므로,
    //          호출 측은 워커 스레드를 join한 뒤에 이 함수가 최종 정리를 마쳤음을
    //          전제로 인스턴스를 파괴해야 합니다. 이 함수 자체는 스레드를 기다리지 않습니다.
    //***************************************************************************
    void Shutdown()
    {
        _isRunning.store(false, std::memory_order_release);
        _rioCore.Shutdown();
    }

    //***************************************************************************
    // @brief 워커가 관리 중인 CRioCore 객체 참조를 반환합니다.
    // @return CRioCore& CRioCore 레퍼런스
    //***************************************************************************
    CRioCore& GetRioCore() { return _rioCore; }
    const CRioCore& GetRioCore() const { return _rioCore; }

    ULONG_PTR GetCqIdentifier() const { return _cqIdentifier; }
    bool IsRunning() const { return _isRunning.load(std::memory_order_acquire); }

private:
    CRioCore          _rioCore;      // RIO 코어 엔진 (함수 테이블, CQ, IOCP 관리)
    ULONG_PTR         _cqIdentifier; // CQ 고유 식별 키
    CRioEventPool* _eventPool;    // 이벤트 프리 리스트 풀 포인터
    std::atomic<bool> _isRunning;    // 워커 구동 상태 플래그 (스레드 간 가시성 보장을 위해 atomic)
};

#endif // __RIOWORKER_H__