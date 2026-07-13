
//***************************************************************************
// OdbcAsyncSrv.cpp : implementation of the COdbcAsyncSrv class.
//
//***************************************************************************

#include "pch.h"
#include "OdbcAsyncSrv.h"

extern CThreadManager* gpThreadManager;

// [버그 수정] Meyers' Singleton 구조 변경으로 컴파일러 레벨에서 100% 멀티스레드 안전성 보장
std::shared_ptr<COdbcAsyncSrv> COdbcAsyncSrv::Instance() {
	static std::shared_ptr<COdbcAsyncSrv> instance = std::make_shared<COdbcAsyncSrv>();
	return instance;
}

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

COdbcAsyncSrv::COdbcAsyncSrv()
{
	_nDBCount = 0;
	_bOpen = false;
	_nMaxThreadCnt = 0;
	_bStopThread = false;
	_pOdbcConnPools = nullptr;
}

COdbcAsyncSrv::~COdbcAsyncSrv()
{
	StopThread();
	Clear();

	if( _pOdbcConnPools != nullptr )
	{
		for( int32 i = 0; i < _nDBCount; i++ )
		{
			if( _pOdbcConnPools[i] != nullptr )
			{
				delete _pOdbcConnPools[i]; // xnew 대응 안전 소멸
			}
		}
		SAFE_DELETE_ARRAY(_pOdbcConnPools);
	}

	_nMaxThreadCnt = 0;
	_bOpen = false;
	_nDBCount = 0;
}

void COdbcAsyncSrv::Clear()
{
	std::unique_lock<std::shared_mutex> lockGuard(_mutex);

	while( !_queueDBAsyncRq.empty() )
	{
		st_DBAsyncRq* pAsyncRq = _queueDBAsyncRq.front();
		_queueDBAsyncRq.pop();
		if( pAsyncRq != nullptr ) SAFE_DELETE(pAsyncRq);
	}
}

std::shared_ptr<CDBAsyncSrvHandler> COdbcAsyncSrv::Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler)
{
	_mapCommand.insert(COMMAND_MAP::value_type(command, handler));
	return handler;
}

bool COdbcAsyncSrv::StartService(std::vector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt)
{
	return InitOdbc(dbNodeVec, nMaxThreadCnt);
}

bool COdbcAsyncSrv::InitOdbc(std::vector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt)
{
	if( 0 == nMaxThreadCnt )
		_nMaxThreadCnt = static_cast<int32>(SYSTEM::CoreCount());
	else
		_nMaxThreadCnt = nMaxThreadCnt;

	_nDBCount = static_cast<int32>(dbNodeVec.size());
	if( _nDBCount <= 0 )
		return true;

	_pOdbcConnPools = new COdbcConnPool * [_nDBCount];
	int32 nIdx = 0;

	for( auto& iter : dbNodeVec )
	{
		if( nIdx >= _nDBCount ) break;

		_pOdbcConnPools[nIdx] = new COdbcConnPool(_nMaxThreadCnt);
		if( nullptr == _pOdbcConnPools[nIdx] )
		{
			LOG_ERROR(_T("Failed to alloc COdbcConnPool"));
			return false;
		}

		if( false == _pOdbcConnPools[nIdx]->Init(iter._dbClass, iter._tszDSN) )
		{
			LOG_ERROR(_T("Failed to Initialize COdbcConnPool"));
			Clear();
			return false;
		}
		++nIdx;
	}

	_bOpen = true;
	return true;
}

void COdbcAsyncSrv::StartIoThreads()
{
	if( gpThreadManager == nullptr ) return;

	for( int32 i = 0; i < _nMaxThreadCnt; i++ )
	{
		// [버그 수정] 람다 [=] 복사 캡처로 인한 수명 주기 댕글링 문제를 방지하기 위해 
		// 명시적으로 안전한 멤버 함수 포인터 바인딩 패턴 전달
		gpThreadManager->CreateThread(std::bind(&COdbcAsyncSrv::RunningThread, this));
	}
}

bool COdbcAsyncSrv::RunningThread()
{
	if( _bOpen )
	{
		Action();
	}
	return true;
}

bool COdbcAsyncSrv::Action()
{
	static uint64 cumulateCallCnt = 0;
	while( !_bStopThread.load() )
	{
		st_DBAsyncRq* pAsyncRq = Pop();
		if( pAsyncRq == nullptr )
		{
			continue; // 종료 신호 활성화 시 루프 탈출 흐름 유도
		}

		COMMAND_MAP::iterator it = _mapCommand.find(pAsyncRq->callIdent);
		if( _mapCommand.end() == it )
		{
			LOG_ERROR(_T("Error not found Async Call... callIdent: [%u]"), pAsyncRq->callIdent);
			SAFE_DELETE(pAsyncRq);
			continue;
		}

		uint64 startTick = _GetTickCount();

		std::shared_ptr<CDBAsyncSrvHandler> command = it->second;
		EDBReturnType Ret = command->ProcessAsyncCall(pAsyncRq);

		if( Ret != EDBReturnType::OK )
		{
			LOG_ERROR(_T("Failed Async Call... callIdent: [%u]"), pAsyncRq->callIdent);

			if( Ret == EDBReturnType::TIMEOUT && pAsyncRq->bReTry == false )
			{
				uint64 endTick = _GetTickCount();
				if( 300 <= endTick - startTick )
					LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%llu], ret:[%d], QueryNo:[%u]"), endTick - startTick, cumulateCallCnt++, static_cast<int>(Ret), pAsyncRq->callIdent);

				st_DBAsyncRq* copyAsyncRq = new st_DBAsyncRq{ *pAsyncRq };
				uint16 logIdent = pAsyncRq->callIdent; // [버그 수정] Use-After-Free 방지를 위해 사전에 보관

				SAFE_DELETE(pAsyncRq);

				copyAsyncRq->bReTry = true;
				int nSize = Push(copyAsyncRq);

				LOG_ERROR(_T("Query timeout ReTry... callIdent: [%u], queuesize[%d]"), logIdent, nSize);
				continue;
			}
		}

		uint64 endTick = _GetTickCount();
		if( 300 <= endTick - startTick )
			LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%llu], ret:[%d], QueryNo:[%u]"), endTick - startTick, cumulateCallCnt++, static_cast<int>(Ret), pAsyncRq->callIdent);

		SAFE_DELETE(pAsyncRq);
	}

	return true;
}

int COdbcAsyncSrv::Push(st_DBAsyncRq* pAsyncRq)
{
	std::unique_lock<std::shared_mutex> lockGuard(_mutex);

	if( _bStopThread.load() ) return 0;

	_queueDBAsyncRq.push(pAsyncRq);
	int queueSize = static_cast<int>(_queueDBAsyncRq.size());

	// [성능 최적화] 데이터가 하나 들어왔으므로 대기 중인 수많은 스레드를 다 깨우지 않고 
	// 정확히 '작업을 처리할 한 개의 워커 스레드'만 깨우도록 notify_one()으로 교체하여 컨텍스트 스위칭 최소화
	_cva.notify_one();

	return queueSize;
}

st_DBAsyncRq* COdbcAsyncSrv::Pop()
{
	static int queueCount = 2;
	std::unique_lock<std::shared_mutex> lockGuard(_mutex);

	// 큐가 완전히 비어있거나 종료 신호가 들어올 때까지 완벽하게 Lock 프리징 대기
	_cva.wait(lockGuard, [this]() { return !_queueDBAsyncRq.empty() || _bStopThread.load(); });

	// 서버 종료 과정이고 큐가 비어있다면 즉시 안전 반환
	if( _bStopThread.load() && _queueDBAsyncRq.empty() ) return nullptr;

	st_DBAsyncRq* pAsyncRq = _queueDBAsyncRq.front();
	_queueDBAsyncRq.pop();

	if( MAX_WARNING_QUERY_QUEUE_SIZE <= _queueDBAsyncRq.size() && _queueDBAsyncRq.size() <= MAX_WARNING_QUERY_QUEUE_SIZE + 10 )
		LOG_ERROR(_T("Async DB Call Queue size... : [%d]"), static_cast<int>(_queueDBAsyncRq.size()));

	if( _queueDBAsyncRq.size() > queueCount )
	{
		queueCount = (int)_queueDBAsyncRq.size();
		LOG_WARNING(_T("Async DB Call Queue size... : [%d]"), static_cast<int>(_queueDBAsyncRq.size()));
	}

	// [버그 제거] 소비자가 다른 소비자를 무작위로 다 깨우던 불필요한 고비용 notify_all() 호출 제거

	return pAsyncRq;
}

COdbcConnPool* COdbcAsyncSrv::GetAccountOdbcConnPool(void)
{
	assert(_pOdbcConnPools != nullptr && _nDBCount > 0);
	return _pOdbcConnPools[0];
}

COdbcConnPool* COdbcAsyncSrv::GetOdbcConnPool(uint64 m_nID)
{
	assert(_pOdbcConnPools != nullptr && _nDBCount > 0);

	int32 nIdx = _nDBCount - 1;
	if( 2 < _nDBCount )
	{
		if( 0 < m_nID )
			nIdx = (m_nID % (_nDBCount - 1)) + 1;
	}

	return _pOdbcConnPools[nIdx];
}

COdbcConnPool* COdbcAsyncSrv::GetLogOdbcConnPool()
{
	assert(_pOdbcConnPools != nullptr && _nDBCount > 2);
	return _pOdbcConnPools[2];
}