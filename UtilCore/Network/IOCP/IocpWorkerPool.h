
//***************************************************************************
// IocpWorkerPool.h : interface for the CIocpWorkerPool class.
//
//***************************************************************************

#ifndef UC_IOCPWORKERPOOL_H
#define UC_IOCPWORKERPOOL_H

#include <Network/IOCP/IocpCommon.h>
#include <Network/IOCP/IocpCore.h>
#include <Thread/ThreadManager.h>

#include <thread>

//***************************************************************************
// @class CIocpWorkerPool
// @brief CIocpCore의 완료 큐를 처리하는 IOCP 워커 스레드 풀.
//
// @details
// CIocpServerService와 CIocpClientService가 공통으로 쓰는 워커 구동/정지 절차를 담당합니다.
//
// 워커 루프:
//      종료 플래그(IsShuttingDown)가 서기 전까지 DispatchBatch(Iocp::kWorkerPollTimeoutMs)를
//      반복하고, 음수(QUIT 수거 또는 IOCP 치명 오류)가 반환되면 루프를 빠져나간다.
//
// 종료 절차 (Stop):
//      RequestShutdown()으로 종료 플래그를 먼저 세운 뒤 워커 수만큼 wake-up 패킷을 게시한다 —
//      패킷이 먼저면 깨어난 워커가 플래그가 서기 전에 DispatchBatch()를 한 번 더 돈다.
//      wake-up 패킷은 QUIT_KEY가 아닌 키 0의 빈 패킷이다. 워커가 종료 플래그로 먼저 빠져나가
//      수거되지 않은 패킷이 남아도, 같은 CIocpCore를 다시 쓰는 다음 서비스의 워커를 종료시키지
//      않기 위함이다. 게시에 실패해도 워커는 kWorkerPollTimeoutMs 안에 종료 플래그를 확인한다.
//      Stop()은 세션 정리가 끝난 뒤에 호출해야 한다 — 세션의 pending I/O 완료 통지는 워커가 처리한다.
//***************************************************************************
class CIocpWorkerPool
{
public:
	CIocpWorkerPool() = default;

	CIocpWorkerPool(const CIocpWorkerPool&) = delete;
	CIocpWorkerPool& operator=(const CIocpWorkerPool&) = delete;

	//***************************************************************************
	// @brief 워커 스레드 수를 결정합니다.
	// @param requested 요청 개수 (0이면 하드웨어 스레드 수 기반 자동 산정, 알 수 없으면 2)
	//***************************************************************************
	static uint32	ResolveThreadCount(uint32 requested);

	//***************************************************************************
	// @brief 워커 스레드를 구동합니다.
	// @param iocpCore 완료 큐를 수거할 IOCP 코어 (풀보다 오래 살아있어야 한다)
	// @param requestedCount 워커 스레드 개수 (0이면 자동 산정)
	// @return 전부 생성했으면 true. 하나라도 실패하면 false이며, 이미 만든 스레드는 Stop()으로 정리한다.
	//***************************************************************************
	bool			Start(CIocpCore* iocpCore, uint32 requestedCount);

	//***************************************************************************
	// @brief 워커 스레드를 깨워 종료시키고 모두 Join합니다.
	// @param iocpCore Start()에 넘긴 IOCP 코어 (nullptr이면 wake-up 없이 플래그만 세운다)
	//***************************************************************************
	void			Stop(CIocpCore* iocpCore);

private:
	CThreadManager	_threadManager;		// 워커 스레드 수명 주기 및 TLS 관리자
};

#endif // ndef UC_IOCPWORKERPOOL_H