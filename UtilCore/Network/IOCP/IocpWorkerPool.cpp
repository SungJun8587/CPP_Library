
//***************************************************************************
// IocpWorkerPool.cpp: implementation of the CIocpWorkerPool class.
//
//***************************************************************************

#include "pch.h"
#include "IocpWorkerPool.h"

//***************************************************************************
// @brief 워커 스레드 수를 결정합니다. 0이면 하드웨어 스레드 수 기반으로 자동 산정합니다.
//***************************************************************************
uint32 CIocpWorkerPool::ResolveThreadCount(uint32 requested)
{
	if( requested != 0 )
		return requested;

	const unsigned int hwThreads = std::thread::hardware_concurrency();
	return (hwThreads > 0) ? hwThreads : 2;
}

//***************************************************************************
// @brief 워커 스레드를 구동합니다 (CThreadManager를 통해 TLS 초기화 및 종료 감지 적용).
//***************************************************************************
bool CIocpWorkerPool::Start(CIocpCore* iocpCore, uint32 requestedCount)
{
	if( iocpCore == nullptr )
		return false;

	const uint32 count = ResolveThreadCount(requestedCount);

	for( uint32 i = 0; i < count; ++i )
	{
		const bool created = _threadManager.CreateThread([this, iocpCore]() {
			while( !_threadManager.IsShuttingDown() )
			{
				// 음수 반환은 QUIT 수거 또는 IOCP 치명 오류 — 이 워커는 루프를 종료한다.
				if( iocpCore->DispatchBatch(Iocp::kWorkerPollTimeoutMs) < 0 )
					break;
			}
			});

		if( !created )
			return false;
	}

	return true;
}

//***************************************************************************
// @brief 워커 스레드를 깨워 종료시키고 모두 Join합니다.
//***************************************************************************
void CIocpWorkerPool::Stop(CIocpCore* iocpCore)
{
	_threadManager.RequestShutdown();

	if( iocpCore != nullptr && iocpCore->GetHandle() != INVALID_HANDLE_VALUE )
	{
		const size_t threadCount = _threadManager.GetThreadCount();
		for( size_t i = 0; i < threadCount; ++i )
		{
			if( ::PostQueuedCompletionStatus(iocpCore->GetHandle(), 0, 0, nullptr) == FALSE )
				LOG_ERROR(_T("[CIocpWorkerPool] PostQueuedCompletionStatus failed: error=%lu"), ::GetLastError());
		}
	}

	_threadManager.JoinThreads();

	// Join이 끝나 스레드가 없으므로 종료 플래그를 풀어 같은 풀로 Start()를 다시 호출할 수 있게 한다.
	_threadManager.ResetShutdown();
}