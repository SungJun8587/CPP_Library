
//***************************************************************************
// OdbcAsyncSrv.cpp : implementation of the COdbcAsyncSrv class.
//
//***************************************************************************

#include "pch.h"
#include "OdbcAsyncSrv.h"
#include <algorithm>

extern CThreadManager* gpThreadManager;

// [동시성 보장] Meyers' Singleton: C++11부터 함수 내 static 지역 변수 초기화는
// 컴파일러에 의해 100% 스레드 안전하게(magic statics) 보장된다.
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
	ClearOdbcPools();

	_nMaxThreadCnt = 0;
	_bOpen = false;
	_nDBCount = 0;
}

void COdbcAsyncSrv::Clear()
{
	std::unique_lock<std::mutex> lockGuard(_mutex);

	while( !_queueDBAsyncRq.empty() )
	{
		st_DBAsyncRq* pAsyncRq = _queueDBAsyncRq.front();
		_queueDBAsyncRq.pop();
		if( pAsyncRq != nullptr ) SAFE_DELETE(pAsyncRq);
	}
}

//***************************************************************************
// _pOdbcConnPools 배열의 각 풀을 안전하게 해제한다.
// InitOdbc가 중간에 실패했을 때(일부만 생성된 상태)와, 소멸자 양쪽에서 공용으로 호출된다.
// COdbcConnPool은 BaseAllocator를 상속하므로 operator delete가 이미 RawAllocator
// 경로로 오버라이드되어 있다 - 즉 평범한 delete(SAFE_DELETE)만으로 충분하며
// xdelete가 필요 없다.
//***************************************************************************
void COdbcAsyncSrv::ClearOdbcPools()
{
	if( _pOdbcConnPools == nullptr ) return;

	for( int32 i = 0; i < _nDBCount; i++ )
	{
		SAFE_DELETE(_pOdbcConnPools[i]);
	}
	SAFE_DELETE_ARRAY(_pOdbcConnPools);
}

std::shared_ptr<CDBAsyncSrvHandler> COdbcAsyncSrv::Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler)
{
	_mapCommand.insert(COMMAND_MAP::value_type(command, handler));
	return handler;
}

bool COdbcAsyncSrv::StartService(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt)
{
	return InitOdbc(dbNodeVec, nMaxThreadCnt);
}

//***************************************************************************
// DB 노드별 COdbcConnPool을 생성/초기화한다.
//***************************************************************************
bool COdbcAsyncSrv::InitOdbc(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt)
{
	_bStopThread.store(false); // 재시작 시나리오 대비 종료 플래그 초기화

	if( 0 == nMaxThreadCnt )
		_nMaxThreadCnt = static_cast<int32>(SYSTEM::CoreCount());
	else
		_nMaxThreadCnt = nMaxThreadCnt;

	_nDBCount = static_cast<int32>(dbNodeVec.size());
	if( _nDBCount <= 0 )
		return true;

	// 배열을 값 초기화(0)해 두어, 중간에 실패해도 ClearOdbcPools()가
	// 아직 생성되지 않은 슬롯을 쓰레기 포인터로 delete하는 일이 없도록 한다.
	_pOdbcConnPools = new COdbcConnPool * [_nDBCount]();

	// 재연결 워커 수를 풀 크기(= DB 비동기 워커 스레드 수) 대비 비례해서 산정한다.
	// (풀 크기의 10~25% 권장, 최소 4 — DB 재시작 등 대량 동시 장애 복구 속도를 위함)
	COdbcConnPool::TReconnectConfig reconnectCfg;
	reconnectCfg.nWorkerCount = std::max(4, _nMaxThreadCnt / 4);

	int32 nIdx = 0;
	for( auto& iter : dbNodeVec )
	{
		if( nIdx >= _nDBCount ) break;

		// COdbcConnPool이 BaseAllocator를 상속하므로 operator new가 이미 RawAllocator
		// 경로로 오버라이드되어 있다 - 평범한 new로도 실패 시 nullptr을 반환한다.
		_pOdbcConnPools[nIdx] = new COdbcConnPool(_nMaxThreadCnt);
		if( nullptr == _pOdbcConnPools[nIdx] )
		{
			LOG_ERROR(_T("Failed to alloc COdbcConnPool"));
			ClearOdbcPools(); // 이미 만들어진 풀들까지 함께 정리
			return false;
		}

		if( false == _pOdbcConnPools[nIdx]->Init(iter._dbClass, iter._tszDSN, reconnectCfg) )
		{
			LOG_ERROR(_T("Failed to Initialize COdbcConnPool"));
			ClearOdbcPools();
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
		// [성능] std::bind는 내부적으로 타입 소거된 호출 객체를 만들어 컴파일러 인라인 최적화가
		// 잘 들어가지 않는 경우가 많다. this만 캡처하는 람다가 더 가볍고, COdbcConnPool의
		// 워커 스레드들([this](){ ... })과도 스타일이 일치한다.
		gpThreadManager->CreateThread([this]() { RunningThread(); });
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
			continue; // 종료 신호로 깨어난 경우 루프 탈출 흐름으로 진행
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

				// [버그 수정] st_DBAsyncRq는 callIdent별로 실제 쿼리 데이터를 담은 파생 구조체의
				// 베이스 타입이다(2바이트짜리 헤더 구조체가 그대로 쓰일 리 없고, ProcessAsyncCall이
				// callIdent로 실제 파생 타입을 캐스팅해서 쓰는 구조). 기존 코드의
				// `new st_DBAsyncRq{ *pAsyncRq }`는 베이스 타입으로 복사하므로 파생 클래스에 있는
				// 실제 쿼리 파라미터가 전부 잘려나가는 오브젝트 슬라이싱 버그였다 — 재시도된 쿼리가
				// 원래 파라미터를 잃어버린 채로 다시 실행됐을 것이다.
				// 복사본을 새로 만들 필요 없이 원본 객체를 그대로 재사용해 재시도 플래그만 세팅하고
				// 재큐잉한다. 슬라이싱 버그가 사라지는 것은 물론, 타임아웃 재시도 경로(이미 DB가
				// 지연되고 있는 상황)에서 불필요한 heap 할당/해제 한 쌍도 없어진다.
				uint16 logIdent = pAsyncRq->callIdent;
				pAsyncRq->bReTry = true;

				int nSize = Push(pAsyncRq);

				// 종료 신호가 이미 켜진 상태라 큐에 들어가지 못했다면(Push가 0을 반환) 직접 해제
				if( nSize == 0 )
				{
					SAFE_DELETE(pAsyncRq);
				}

				LOG_ERROR(_T("Query timeout ReTry... callIdent: [%u], queuesize[%d]"), logIdent, nSize);
				continue;
			}
		}

#if defined(_DEBUG)
		uint64 endTick = _GetTickCount();
		if( 300 <= endTick - startTick )
			LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%llu], ret:[%d], QueryNo:[%u]"), endTick - startTick, cumulateCallCnt++, static_cast<int>(Ret), pAsyncRq->callIdent);
#else
		uint64 endTick = _GetTickCount();
		if( 1000 <= endTick - startTick )
			LOG_WARNING(_T("Delay Query %lums... cumulateCallCnt[%llu], ret:[%d], QueryNo:[%u]"), endTick - startTick, cumulateCallCnt++, static_cast<int>(Ret), pAsyncRq->callIdent);
#endif

		SAFE_DELETE(pAsyncRq);
	}

	return true;
}

int COdbcAsyncSrv::Push(st_DBAsyncRq* pAsyncRq)
{
	int queueSize = 0;
	{
		std::unique_lock<std::mutex> lockGuard(_mutex);

		if( _bStopThread.load() ) return 0;

		_queueDBAsyncRq.push(pAsyncRq);
		queueSize = static_cast<int>(_queueDBAsyncRq.size());
	} // [성능] notify 전에 락을 먼저 해제해, 깨어난 워커가 즉시 락을 잡지 못하고
	  // 다시 잠드는 불필요한 컨텍스트 스위칭(lock-and-wake-under-lock)을 피한다.

	// [동시성 최적화] 데이터가 하나 들어왔으므로 모든 대기 워커 스레드를 다 깨우지 않고,
	// 정확히 '작업을 처리할 수 있는 워커 하나'만 깨우도록 notify_one()으로 대체해 컨텍스트 스위칭 최소화
	_cva.notify_one();

	return queueSize;
}

st_DBAsyncRq* COdbcAsyncSrv::Pop()
{
	static int queueCount = 2;
	std::unique_lock<std::mutex> lockGuard(_mutex);

	// 큐에 데이터가 들어오거나 종료 신호가 켜질 때까지 대기하며 Lock 스퓨리어스 웨이크업 방지
	_cva.wait(lockGuard, [this]() { return !_queueDBAsyncRq.empty() || _bStopThread.load(); });

	// 종료 상태이고 큐도 비어있다면 즉시 nullptr 반환
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

	// [주의] 소비자가 다른 소비자를 깨우는 것은 불필요하므로 notify_all() 호출하지 않음

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