
//***************************************************************************
// RioWorker.h : interface for the CRioWorker class.
//
//***************************************************************************

#ifndef __RIOWORKER_H__
#define __RIOWORKER_H__

#ifndef __RIOCOMMON_H__
#include <Network/RioCommon.h>
#endif

#ifndef __RIOCORE_H__
#include <Network/RioCore.h>
#endif

#ifndef	__RIOEVENTPOOL_H__
#include <Network/RioEventPool.h>
#endif

//***************************************************************************
// @class CRioWorker
// @brief 전용 CQ와 IOCP를 소유하고 독립된 스레드에서 비동기 I/O 완료를 폴링/대기하는 클래스.
// @details
//      - 1 Worker : 1 CQ 구조를 통해 Cross-Worker Completion 유입을 원천 차단합니다.
//      - CRioCore를 내부에 포함하여 CQ 배치 수거 및 스핀 방지 루프를 실행합니다.
//***************************************************************************
class CRioWorker
{
    static constexpr uint32 kCqBurstMargin = 1024; // 트래픽 폭주 대응용 마진

public:
    static constexpr int32 kCorruptCq = Rio::kCorruptCq;

    CRioWorker();
    ~CRioWorker();

    CRioWorker(const CRioWorker&) = delete;
    CRioWorker& operator=(const CRioWorker&) = delete;
    CRioWorker(CRioWorker&&) = delete;
    CRioWorker& operator=(CRioWorker&&) = delete;

    bool Initialize(
        SOCKET representativeSocket,
        DWORD maxSessionCount,
        DWORD maxOutstandingRecv,
        DWORD maxOutstandingSend,
        ULONG_PTR cqIdentifier,
        CRioEventPool* eventPool);

    int32 ProcessLoopOnce();
    void Start();
    void Shutdown();

    CRioCore& GetRioCore();
    const CRioCore& GetRioCore() const;
    ULONG_PTR GetCqIdentifier() const;
    bool IsRunning() const;

private:
    CRioCore            _rioCore;       // RIO 코어 엔진 (함수 테이블, CQ, IOCP 관리)
    ULONG_PTR           _cqIdentifier;  // CQ 고유 식별 키
    CRioEventPool*      _eventPool;     // 이벤트 프리 리스트 풀 포인터
    
    std::atomic<bool>   _isRunning;     // Worker loop 자체의 실행 허용 여부(스레드 간 가시성 보장을 위해 atomic)
};

#endif // __RIOWORKER_H__