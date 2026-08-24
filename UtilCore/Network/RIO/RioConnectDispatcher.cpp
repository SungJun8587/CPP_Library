
//***************************************************************************
// RioConnectDispatcher.cpp : implementation of the CRioConnectDispatcher class.
//
//***************************************************************************

#include "pch.h"
#include "RioConnectDispatcher.h"

//***************************************************************************
// @brief 전용 IOCP를 생성하고 워커 스레드 1개를 시작합니다.
// @return bool 성공 여부
//***************************************************************************
bool CRioConnectDispatcher::Start()
{
	if( _running.exchange(true, std::memory_order_acq_rel) )
		return false; // 이미 실행 중

	_iocpHandle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
	if( _iocpHandle == nullptr )
	{
		_running.store(false, std::memory_order_release);
		return false;
	}

	try
	{
		_workerThread = std::thread([this]() { WorkerLoop(); });
	}
	catch( ... )
	{
		::CloseHandle(_iocpHandle);
		_iocpHandle = nullptr;
		_running.store(false, std::memory_order_release);
		return false;
	}

	return true;
}

//***************************************************************************
// @brief 워커 스레드를 정지하고 IOCP 핸들을 닫습니다.
//***************************************************************************
void CRioConnectDispatcher::Shutdown()
{
	if( !_running.exchange(false, std::memory_order_acq_rel) )
		return; // 이미 정지됨 (idempotent)

	if( _iocpHandle != nullptr )
	{
		// 워커가 GetQueuedCompletionStatus(INFINITE)에서 블로킹 중일 수 있으므로
		// overlapped=nullptr인 wake-up 패킷을 하나 포스팅해 깨운다. 워커 루프는
		// overlapped==nullptr를 정지 신호로 해석한다.
		::PostQueuedCompletionStatus(_iocpHandle, 0, 0, nullptr);
	}

	if( _workerThread.joinable() )
		_workerThread.join();

	if( _iocpHandle != nullptr )
	{
		::CloseHandle(_iocpHandle);
		_iocpHandle = nullptr;
	}
}

//***************************************************************************
// @brief 연결을 시도할 소켓을 이 디스패처의 전용 IOCP에 연결합니다.
// @param socket 대상 소켓 핸들
// @return bool 성공 여부
//***************************************************************************
bool CRioConnectDispatcher::RegisterSocket(SOCKET socket) const
{
	if( _iocpHandle == nullptr || socket == INVALID_SOCKET )
		return false;

	return ::CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket), _iocpHandle, 0, 0) != nullptr;
}

//***************************************************************************
// @brief 워커 스레드 루프. ConnectEx 완료 통지를 받아 해당 세션의
//        ProcessConnectEx()로 위임한다.
// @details GQCS의 성공/실패(BOOL 반환값) 자체는 신뢰하지 않는다 — non-blocking
//          ConnectEx는 실패한 경우에도 completion 자체는 정상적으로 오는 게
//          일반적인 패턴이고(MS 문서 기준), 실제 성공/실패 판정은 항상
//          CRioSession::ProcessConnectEx() 내부의 getsockopt(SO_ERROR)로
//          단일하게 처리한다(IOCP의 ConnectAsync와 동일한 설계 원칙).
//          overlapped==nullptr인 경우만 Shutdown()이 보낸 wake-up 패킷으로
//          간주해 루프 재확인(=정지 조건 검사) 후 빠져나간다.
//***************************************************************************
void CRioConnectDispatcher::WorkerLoop()
{
	while( _running.load(std::memory_order_acquire) )
	{
		DWORD numOfBytes = 0;
		ULONG_PTR completionKey = 0;
		LPOVERLAPPED overlapped = nullptr;

		::GetQueuedCompletionStatus(_iocpHandle, &numOfBytes, &completionKey, &overlapped, INFINITE);

		if( overlapped == nullptr )
		{
			// wake-up(정지) 패킷 또는 핸들 자체 오류 — while 조건에서 _running 재확인
			continue;
		}

		RioConnectEvent* connectEvent = static_cast<RioConnectEvent*>(overlapped);

		// owner를 로컬로 옮겨와 이 스코프 동안 세션 수명을 보장하고, 이벤트
		// 자신은 owner 참조를 놓는다(다음 재사용 대비 및 순환참조 방지 —
		// IOCP RecvEvent/SendEvent의 owner 해제 패턴과 동일).
		CRioSessionRef owner = std::move(connectEvent->owner);
		connectEvent->owner = nullptr;

		if( owner )
			owner->ProcessConnectEx();
	}
}