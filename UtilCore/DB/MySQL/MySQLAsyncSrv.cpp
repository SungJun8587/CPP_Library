
//***************************************************************************
// MySQLAsyncSrv.cpp : implementation of the CMySQLAsyncSrv class.
//
//***************************************************************************

#include "pch.h"
#include "MySQLAsyncSrv.h"

extern CThreadManager* gpThreadManager;

// [버그 수정] 컴파일러 레벨에서 static 로컬 초기화의 Thread-Safety를 100% 보장하는 싱글턴 패턴 적용
std::shared_ptr<CMySQLAsyncSrv> CMySQLAsyncSrv::Instance() {
	static std::shared_ptr<CMySQLAsyncSrv> instance = std::make_shared<CMySQLAsyncSrv>();
	return instance;
}

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CMySQLAsyncSrv::CMySQLAsyncSrv()
{
	_nDBCount = 0;
	_bOpen = false;
	_nMaxThreadCnt = 0;
	_bStopThread = false;
	_pMySQLConnPools = nullptr;
}

CMySQLAsyncSrv::~CMySQLAsyncSrv()
{
	StopThread();
	Clear();

	if( _pMySQLConnPools != nullptr )
	{
		for( int32 i = 0; i < _nDBCount; i++ )
		{
			if( _pMySQLConnPools[i] != nullptr )
			{
				delete _pMySQLConnPools[i]; // xnew 대응 안전 해제
			}
		}
		SAFE_DELETE_ARRAY(_pMySQLConnPools);
	}

	_nMaxThreadCnt = 0;
	_bOpen = false;
	_nDBCount = 0;
}

void CMySQLAsyncSrv::Clear()
{
	std::unique_lock<std::shared_mutex> lockGuard(_mutex);

	while( !_queueDBAsyncRq.empty() )
	{
		st_DBAsyncRq* pAsyncRq = _queueDBAsyncRq.front();
		_queueDBAsyncRq.pop();
		if( pAsyncRq != nullptr ) SAFE_DELETE(pAsyncRq);
	}

	for( auto& iter : _mapCommand )
	{
		iter.second.reset();
	}
	_mapCommand.clear();
}

std::shared_ptr<CDBAsyncSrvHandler> CMySQLAsyncSrv::Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler)
{
	_mapCommand.insert(COMMAND_MAP::value_type(command, handler));
	return handler;
}

bool CMySQLAsyncSrv::StartService(std::vector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt)
{
	return InitMySQL(dbNodeVec, nMaxThreadCnt);
}

bool CMySQLAsyncSrv::InitMySQL(std::vector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt)
{
	if( 0 == nMaxThreadCnt )
		_nMaxThreadCnt = static_cast<int32>(SYSTEM::CoreCount());
	else
		_nMaxThreadCnt = nMaxThreadCnt;

	_nDBCount = static_cast<int32>(dbNodeVec.size());
	if( _nDBCount <= 0 )
		return true;

	_pMySQLConnPools = new CMySQLConnPool * [_nDBCount];
	int32 nIdx = 0;

	for( auto& iter : dbNodeVec )
	{
		if( nIdx >= _nDBCount ) break;

		_pMySQLConnPools[nIdx] = new CMySQLConnPool(_nMaxThreadCnt);
		if( nullptr == _pMySQLConnPools[nIdx] )
		{
			LOG_ERROR(_T("Failed to alloc CMySQLConnPool"));
			return false;
		}

		if( false == _pMySQLConnPools[nIdx]->Init(iter._tszDBHost, iter._tszDBUserId, iter._tszDBPasswd, iter._tszDBName, iter._nPort) )
		{
			LOG_ERROR(_T("Failed to Initialize CMySQLConnPool"));
			Clear();
			return false;
		}
		++nIdx;
	}

	_bOpen = true;
	return true;
}

void CMySQLAsyncSrv::StartIoThreads()
{
	if( gpThreadManager == nullptr ) return;

	for( int32 i = 0; i < _nMaxThreadCnt; i++ )
	{
		// [버그 수정] [=] 람다 복사 캡처로 발생하던 객체 소멸 시점의 댕글링 포인터 방지를 위해 
		// 명시적인 안전 멤버 함수 바인딩 포인터 전달 방식으로 교체
		gpThreadManager->CreateThread(std::bind(&CMySQLAsyncSrv::RunningThread, this));
	}
}

bool CMySQLAsyncSrv::RunningThread()
{
	if( _bOpen )
	{
		Action();
	}
	return true;
}

bool CMySQLAsyncSrv::Action()
{
	static uint64 cumulateCallCnt = 0;
	while( !_bStopThread.load() )
	{
		st_DBAsyncRq* pAsyncRq = Pop();
		if( pAsyncRq == nullptr )
		{
			continue; // 서버 종료 흐름 진입 시 대기 후 안전 루프 탈출
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
				uint16 logIdent = pAsyncRq->callIdent; // [버그 수정] Use-After-Free 방지를 위한 식별자 보관

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

int CMySQLAsyncSrv::Push(st_DBAsyncRq* pAsyncRq)
{
	std::unique_lock<std::shared_mutex> lockGuard(_mutex);

	if( _bStopThread.load() ) return 0;

	_queueDBAsyncRq.push(pAsyncRq);
	int queueSize = static_cast<int>(_queueDBAsyncRq.size());

	// [성능 최적화] 무조건 다 깨우는 notify_all() 대신 대기 스레드 딱 하나만 일깨우도록 처리해 불필요한 컨텍스트 스위칭 최소화
	_cva.notify_one();

	return queueSize;
}

st_DBAsyncRq* CMySQLAsyncSrv::Pop()
{
	static int queueCount = 2;
	std::unique_lock<std::shared_mutex> lockGuard(_mutex);

	// 큐가 비어있는 상태라면 데이터가 들어오거나 종료 명령이 떨어질 때까지 완전 대기
	_cva.wait(lockGuard, [this]() { return !_queueDBAsyncRq.empty() || _bStopThread.load(); });

	// 종료 절차 진행 중에 큐까지 완전히 소비했다면 즉시 안전 해제 반환
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

	// [버그 제거] 불필요하게 소비자가 다른 소비자를 연속적으로 강제 기상시키던 잘못된 고비용 notify_all() 제거

	return pAsyncRq;
}

CMySQLConnPool* CMySQLAsyncSrv::GetAccountConnPool(void)
{
	assert(_pMySQLConnPools != nullptr && _nDBCount > 0);
	return _pMySQLConnPools[0];
}

CMySQLConnPool* CMySQLAsyncSrv::GetMySQLConnPool(uint64 m_nID)
{
	assert(_pMySQLConnPools != nullptr && _nDBCount > 0);

	int32 nIdx = _nDBCount - 1;
	if( 2 < _nDBCount )
	{
		if( 0 < m_nID )
			nIdx = (m_nID % (_nDBCount - 1)) + 1;
	}

	return _pMySQLConnPools[nIdx];
}

CMySQLConnPool* CMySQLAsyncSrv::GetLogConnPool()
{
	assert(_pMySQLConnPools != nullptr && _nDBCount > 2);
	return _pMySQLConnPools[2];
}