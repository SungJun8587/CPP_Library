
//***************************************************************************
// RioWorker.cpp : implementation of the CRioWorker class.
//
//***************************************************************************

#include "pch.h"
#include "RioWorker.h"

//***************************************************************************
// @brief CRioWorker 생성자
//***************************************************************************
CRioWorker::CRioWorker()
    : _cqIdentifier(0)
    , _eventPool(nullptr)
    , _isRunning(false)
{
}

//***************************************************************************
// @brief CRioWorker 소멸자
// @details 안전한 리소스 정리를 위해 Shutdown()을 호출합니다.
//***************************************************************************
CRioWorker::~CRioWorker()
{
    Shutdown();
}

//***************************************************************************
// @brief RIO 워커 및 내부 CRioCore 엔진을 초기화합니다.
//
// @param representativeSocket 함수 테이블 바인딩용 대표 소켓
// @param maxSessionCount 시스템 최대 동시 접속 세션 수
// @param maxOutstandingRecv 세션당 최대 수신 허용치
// @param maxOutstandingSend 세션당 최대 송신 허용치
// @param cqIdentifier 무결성 검증을 위한 고유 CQ 키
// @param eventPool 이벤트를 할당받고 반환할 CRioEventPool 포인터
//
// @return 초기화 성공 시 true, 실패 시 false 반환
//
// @details
//      1. eventPool 유효성을 검사합니다.
//      2. 세션 수 및 Send/Recv 허용치를 기반으로 총 I/O 용량을 계산합니다.
//      3. eventPool의 용량이 계산된 totalCapacity보다 작으면 그 차이만큼
//         CRioEventPool::Initialize()를 추가 호출하여 CQ 용량과
//         이벤트 풀 용량이 어긋나지 않도록 맞춥니다.
//      4. 내부 CRioCore 엔진 초기화를 수행합니다.
//***************************************************************************
bool CRioWorker::Initialize(
    SOCKET representativeSocket,
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
//
// @return int32 처리된 이벤트 개수, CQ 손상 시 kCorruptCq(-1) 반환
//
// @details
//      - 워커 구동 상태(_isRunning)를 원자적으로 확인합니다.
//      - CRioCore::DispatchBatch()를 호출하여 CQ에서 완결 이벤트를 배치 단위로 수거 및 처리합니다.
//***************************************************************************
int32 CRioWorker::ProcessLoopOnce()
{
    if( !_isRunning.load(std::memory_order_acquire) )
        return 0;

    return _rioCore.DispatchBatch();
}

//***************************************************************************
// @brief 워커 가동 상태 플래그를 활성화합니다.
//***************************************************************************
void CRioWorker::Start()
{
    _isRunning.store(true, std::memory_order_release);
}

//***************************************************************************
// @brief 워커 구동을 중지하고 내부 RIO 자원을 안전하게 정리합니다.
//
// @details
//      - 워커 구동 플래그를 false로 전환합니다.
//      - 내부 CRioCore::Shutdown()을 호출하여 워커의 Stop을 요청합니다.
//      - CRioCore::Shutdown()은 내부적으로 워커 스레드의 join을 수행합니다.
//      - 워커가 종료된 이후 Outstanding I/O를 Drain하고,
//        RIO Completion Queue 및 IOCP 등의 자원을 최종 해제합니다.
//
//      따라서 호출자는 별도로 워커 스레드를 join할 필요가 없습니다.
//      CRioWorker::Shutdown()이 반환된 시점에는 CRioCore의 Shutdown 정책에
//      따라 최종 자원 해제가 완료되었거나, DrainTimeout/CorruptCq/
//      DispatchError 등의 실패 결과가 발생한 상태입니다.
//
// @note
//      CRioCore::Shutdown()이 실패하여 Closed 상태에 도달하지 못한 경우에도
//      CRioWorker는 해당 Core를 즉시 파괴해서는 안 됩니다.
//      CRioCore의 GetState() 및 GetLastShutdownResult()를 통해
//      최종 상태를 확인해야 합니다.
//***************************************************************************
void CRioWorker::Shutdown()
{
    _isRunning.store(false, std::memory_order_release);
    _rioCore.Shutdown();
}

//***************************************************************************
// @brief 워커가 관리 중인 CRioCore 객체 참조를 반환합니다.
//***************************************************************************
CRioCore& CRioWorker::GetRioCore() { return _rioCore; }
const CRioCore& CRioWorker::GetRioCore() const { return _rioCore; }

//***************************************************************************
// @brief CQ 고유 식별 키를 반환합니다.
//***************************************************************************
ULONG_PTR CRioWorker::GetCqIdentifier() const { return _cqIdentifier; }

//***************************************************************************
// @brief 워커 구동 여부를 반환합니다.
//***************************************************************************
bool CRioWorker::IsRunning() const { return _isRunning.load(std::memory_order_acquire); }