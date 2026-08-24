
//***************************************************************************
// RioConnectDispatcher.h : interface for the CRioConnectDispatcher class.
//
//***************************************************************************

#ifndef __RIOCONNECTDISPATCHER_H__
#define __RIOCONNECTDISPATCHER_H__

#ifndef __RIOCOMMON_H__
#include <Network/RIO/RioCommon.h>
#endif

#ifndef __RIOSESSION_H__
#include <Network/RIO/RioSession.h>
#endif

#ifndef __RIOCONNECTEVENT_H__
#include <Network/RIO/RioConnectEvent.h>
#endif

#include <WinSock2.h>
#include <thread>
#include <atomic>

//***************************************************************************
// @class CRioConnectDispatcher
// @brief CRioSession::ConnectAsync()가 게시하는 ConnectEx 완료 통지 전용 디스패처
//
// @details
//      CRioCore와 완전히 분리된 자기 소유 IOCP 핸들 + 전용 워커 스레드 1개로
//      CRioSession::ConnectAsync()가 게시하는 ConnectEx 완료 통지만 처리한다.
//
//      [분리하는 이유]
//      CRioCore::DispatchBatch()는 RIODequeueCompletion() 전용 경로라,
//      ConnectEx처럼 RIO CQ가 아니라 일반 OVERLAPPED로 오는 완료를 섞어 넣으면
//      IsValidCompletionPacket() 검증에 걸려 CRioCore 전체가 MarkFaulted()로
//      죽는다(진단 이력 참고). 그래서 이 디스패처는 CRioCore와 100% 독립된
//      리소스(IOCP 핸들, 워커 스레드)를 갖는다.
//
//      [워커 스레드 1개로 충분한 이유]
//      연결 시도는(재연결 포함) 빈도가 낮으므로 워커 스레드는 1개로 충분하다고
//      판단했다 — 다수의 세션이 동시에 ConnectEx를 게시해도, "게시" 자체는
//      블로킹 없이 즉시 반환되고 이 스레드는 오직 완료 통지만 순차 소비하면
//      되기 때문에(각 세션의 실제 후속 처리 — RIOCreateRequestQueue/Init 등 —
//      가 소켓/CQ I/O로 블로킹되는 일은 없음, 전부 논블로킹 API) 워커 1개가
//      병목이 될 가능성은 낮다. 다만 후속 처리 안에서 실제로 블로킹 호출이
//      섞이게 되면(예: DNS 조회 등) 재검토 필요.
//***************************************************************************
class CRioConnectDispatcher
{
public:
	CRioConnectDispatcher() = default;
	~CRioConnectDispatcher() { Shutdown(); }

	CRioConnectDispatcher(const CRioConnectDispatcher&) = delete;
	CRioConnectDispatcher& operator=(const CRioConnectDispatcher&) = delete;

	//***************************************************************************
	// @brief 전용 IOCP를 생성하고 워커 스레드 1개를 시작합니다.
	// @return bool 성공 여부. 이미 시작된 상태에서 재호출하면 false.
	//***************************************************************************
	bool Start();

	//***************************************************************************
	// @brief 워커 스레드를 정지하고 IOCP 핸들을 닫습니다. 소멸자에서도 호출되며
	//        여러 번 호출해도 안전합니다(idempotent).
	//***************************************************************************
	void Shutdown();

	//***************************************************************************
	// @brief 연결을 시도할 소켓을 이 디스패처의 전용 IOCP에 연결합니다.
	//        ConnectEx 게시 전에 반드시 호출해야 합니다.
	// @param socket 대상 소켓 핸들
	// @return bool 성공 여부
	//***************************************************************************
	bool RegisterSocket(SOCKET socket) const;

	//***************************************************************************
	// @brief 내부 IOCP 핸들을 반환합니다 (진단/테스트용).
	//***************************************************************************
	HANDLE GetHandle() const noexcept { return _iocpHandle; }

private:
	//***************************************************************************
	// @brief 워커 스레드 루프. ConnectEx 완료 통지를 받아 해당 세션의
	//        ProcessConnectEx()로 위임합니다.
	//***************************************************************************
	void WorkerLoop();

private:
	HANDLE _iocpHandle{ nullptr };			// 이 디스패처 전용 IOCP 핸들 (CRioCore의 IOCP와 완전히 별개)
	std::thread _workerThread;				// ConnectEx 완료 통지를 처리하는 전용 워커 스레드 (1개)
	std::atomic<bool> _running{ false };	// Start()~Shutdown() 사이 실행 상태 플래그, WorkerLoop()의 종료 조건으로도 사용
};

#endif // ndef __RIOCONNECTDISPATCHER_H__