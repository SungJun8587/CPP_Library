
//***************************************************************************
// RioServiceHelper.cpp : implementation of the CRioServiceHelper class.
//
//***************************************************************************

#include "pch.h"
#include "RioServiceHelper.h"

//***************************************************************************
// @brief 이벤트 풀, RIO 코어, 워커 스레드 그룹, 글로벌 수신 버퍼를 순서대로 준비합니다.
//***************************************************************************
bool CRioServiceHelper::StartCore(CRioCore& core, CRioEventPool& eventPool, CRioBufferRef& globalRecvBuffer, ULONG_PTR cqIdentifier, uint32 workerThreadCount)
{
	// 1. 이벤트 풀 초기화
	if( !eventPool.Initialize(Rio::kServiceEventPoolCapacity) )
	{
		LOG_ERROR(_T("[Error] CRioEventPool Initialize failed!"));
		return false;
	}

	// 2. RIO 함수 테이블 조회(WSAIoctl)에만 쓰는 소켓으로 코어를 초기화한다.
	//    테이블을 얻은 뒤에는 필요 없으므로 바로 닫는다(실제 accept용 소켓은 CRioListener가 따로 만든다).
	SOCKET tableSocket = CSocketUtils::CreateRioSocket();
	if( tableSocket == INVALID_SOCKET )
	{
		LOG_ERROR(_T("[Error] CreateRioSocket failed! WSAError: %d"), ::WSAGetLastError());
		eventPool.Release();
		return false;
	}

	const bool initialized = core.Initialize(tableSocket, Rio::kServiceMaxCompletionResults, cqIdentifier, &eventPool);
	CSocketUtils::Close(tableSocket);

	if( !initialized )
	{
		LOG_ERROR(_T("[Error] CRioCore Initialize failed!"));
		eventPool.Release();
		return false;
	}

	// 3. 멀티 워커 스레드 그룹 구동. 세션을 등록하기 전에 코어가 Running 상태여야
	//    PostInitialReceive()가 정상 동작한다. 0을 넘기면 CRioCore가 hardware_concurrency()/2로 산정한다.
	CRioCore* corePtr = &core;

	const bool workerStarted = core.StartWorkers(workerThreadCount, [corePtr]()
		{
			while( corePtr->GetState() == Rio::State::Running )
			{
				corePtr->DispatchBatch(Rio::DispatchMode::Wait);
			}
		});

	if( !workerStarted )
	{
		LOG_ERROR(_T("[Error] CRioCore StartWorkers failed!"));
		core.RequestStop();
		if( core.Shutdown() == Rio::ShutdownResult::Success )
			eventPool.Release();
		return false;
	}

	// 4. 수신용 글로벌 CRioBuffer 초기화
	globalRecvBuffer = MakeShared<CRioBuffer>();
	if( !globalRecvBuffer->Initialize(&core.GetRioTable(), Rio::kServiceGlobalRecvSlotCount, Rio::kServiceGlobalRecvSlotSize) )
	{
		LOG_ERROR(_T("[Error] Global receive CRioBuffer Initialize failed!"));
		globalRecvBuffer.reset();
		core.RequestStop();
		if( core.Shutdown() == Rio::ShutdownResult::Success )
			eventPool.Release();
		return false;
	}

	return true;
}

//***************************************************************************
// @brief RIO 코어를 정지하고 글로벌 수신 버퍼와 이벤트 풀을 해제합니다.
//***************************************************************************
bool CRioServiceHelper::StopCore(const CRioCoreRef& core, CRioBufferRef& globalRecvBuffer, CRioEventPool& eventPool)
{
	if( core )
	{
		core->RequestStop();

		// outstanding I/O drain이 끝나야 버퍼/이벤트를 참조하는 요청이 없음이 보장된다.
		const Rio::ShutdownResult result = core->Shutdown();
		if( result != Rio::ShutdownResult::Success )
		{
			LOG_ERROR(_T("[Error] CRioCore Shutdown did not complete cleanly (result: %d). Buffer/event pool are kept alive."), static_cast<int32>(result));
			return false;
		}
	}

	globalRecvBuffer.reset();
	eventPool.Release();
	return true;
}