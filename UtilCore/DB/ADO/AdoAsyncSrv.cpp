
//***************************************************************************
// AdoAsyncSrv.cpp : implementation of the CAdoAsyncSrv class.
//
//***************************************************************************

#include "pch.h"
#include "AdoAsyncSrv.h"

extern CThreadManager* gpThreadManager;

//***************************************************************************
// @brief 싱글톤 인스턴스를 반환합니다.
// @return CAdoAsyncSrv 공유 포인터
//***************************************************************************
std::shared_ptr<CAdoAsyncSrv> CAdoAsyncSrv::Instance() {
	static std::shared_ptr<CAdoAsyncSrv> instance = std::make_shared<CAdoAsyncSrv>();
	return instance;
}

//***************************************************************************
// @brief 생성자: 기본 멤버 초기화
//***************************************************************************
CAdoAsyncSrv::CAdoAsyncSrv()
{
	_nDBCount = 0;
	_bOpen = false;
	_nMaxThreadCnt = 0;
	_bStopThread = false;
	_pAdoConnPools = nullptr;
}

//***************************************************************************
// @brief 소멸자: 남은 작업 처리 후 리소스 정리
//***************************************************************************
CAdoAsyncSrv::~CAdoAsyncSrv()
{
	FlushRemainingTasks();
	StopThread();
	Clear();
	ClearAdoPools();

	_nMaxThreadCnt = 0;
	_bOpen = false;
	_nDBCount = 0;
}

//***************************************************************************
// @brief 큐를 비웁니다.
// @details 큐에 남아있는 모든 요청을 안전하게 삭제합니다.
//***************************************************************************
void CAdoAsyncSrv::Clear()
{
	// [2번 수정] 크기를 먼저 읽고 그 값만큼 SwapChunk하는 대신, 큐 전체를 한 번의 락 구간 안에서
	// 통째로 이관하는 전용 Swap()을 사용한다. 대상 큐가 비어 있으므로 내부적으로 O(1) 컨테이너
	// 스왑이 되고, "크기 조회 이후 들어온 항목이 이번 드레인에서 누락되는" TOCTOU 여지도 없앤다.
	CQueue<std::unique_ptr<st_DBAsyncRq>> tempQueue;
	_queueDBAsyncRq.Swap(tempQueue);

	// tempQueue가 비워질 때 unique_ptr이 알아서 메모리를 해제하므로 별도의 SAFE_DELETE가 불필요합니다.
	while( !tempQueue.empty() )
	{
		tempQueue.pop();
	}
}

//***************************************************************************
// @brief 남은 작업을 강제로 처리합니다.
// @details 스레드를 중단하고 큐에 남은 요청을 메인 스레드에서 직접 실행합니다.
//***************************************************************************
void CAdoAsyncSrv::FlushRemainingTasks()
{
	LOG_INFO(_T("Main program requested to flush remaining async ADO tasks..."));

	_bStopThread.store(true);
	_cva.notify_all();
	_cvProducer.notify_all();

	// [2번 수정] Clear()와 동일하게 GetSize()+SwapChunk 대신 Swap()으로 한 번에 이관한다.
	CQueue<std::unique_ptr<st_DBAsyncRq>> tempQueue;
	_queueDBAsyncRq.Swap(tempQueue);

	int32 remainingCount = static_cast<int32>(tempQueue.size());
	if( remainingCount > 0 )
	{
		LOG_INFO(_T("Processing %d remaining ADO requests in main thread..."), remainingCount);

		while( !tempQueue.empty() )
		{
			std::unique_ptr<st_DBAsyncRq> pAsyncRq = std::move(tempQueue.front());
			tempQueue.pop();

			if( pAsyncRq == nullptr ) continue;

			COMMAND_MAP::iterator it = _mapCommand.find(pAsyncRq->callIdent);
			if( it != _mapCommand.end() )
			{
				std::shared_ptr<CDBAsyncSrvHandler> command = it->second;
				EDBReturnType Ret = command->ProcessAsyncCall(pAsyncRq.get());

				if( Ret != EDBReturnType::OK )
				{
					LOG_ERROR(_T("Failed to process ADO task during manual flush... callIdent: [%u]"), pAsyncRq->callIdent);
				}
			}
			else
			{
				LOG_ERROR(_T("Error not found command handler for ADO task... callIdent: [%u]"), pAsyncRq->callIdent);
			}

			SubOutstandingRequest();
		}
	}

	LOG_INFO(_T("Manual flush completed for ADO. All tasks processed."));
}

//***************************************************************************
// @brief ADO 연결 풀을 정리합니다.
// @details 모든 연결 풀 객체를 삭제합니다.
//***************************************************************************
void CAdoAsyncSrv::ClearAdoPools()
{
	if( _pAdoConnPools == nullptr ) return;

	for( int32 i = 0; i < _nDBCount; i++ )
	{
		SAFE_DELETE(_pAdoConnPools[i]);
	}
	SAFE_DELETE_ARRAY(_pAdoConnPools);
}

//***************************************************************************
// @brief 명령 핸들러를 등록합니다.
// @param command 명령 식별자
// @param handler 명령 핸들러 객체
// @return 등록된 핸들러
//***************************************************************************
std::shared_ptr<CDBAsyncSrvHandler> CAdoAsyncSrv::Regist(const BYTE command, std::shared_ptr<CDBAsyncSrvHandler> const handler)
{
	_mapCommand.insert(COMMAND_MAP::value_type(command, handler));
	return handler;
}

//***************************************************************************
// @brief 서비스 시작
// @param dbNodeVec DB 노드 벡터
// @param nMaxThreadCnt 최대 스레드 수
// @return 성공 여부
//***************************************************************************
bool CAdoAsyncSrv::StartService(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt)
{
	return InitAdo(dbNodeVec, nMaxThreadCnt);
}

//***************************************************************************
// @brief ADO 초기화
// @param dbNodeVec DB 노드 벡터
// @param nMaxThreadCnt 최대 스레드 수
// @return 성공 여부
//***************************************************************************
bool CAdoAsyncSrv::InitAdo(CVector<CDBNode> dbNodeVec, const int32 nMaxThreadCnt)
{
	_bStopThread.store(false);

	if( 0 == nMaxThreadCnt )
		_nMaxThreadCnt = static_cast<int32>(SYSTEM::CoreCount());
	else
		_nMaxThreadCnt = nMaxThreadCnt;

	_nDBCount = static_cast<int32>(dbNodeVec.size());
	if( _nDBCount <= 0 )
		return true;

	_pAdoConnPools = new CAdoConnPool * [_nDBCount]();

	CAdoConnPool::TReconnectConfig reconnectCfg;
	reconnectCfg.nWorkerCount = std::max(4, _nMaxThreadCnt / 4);

	int32 nIdx = 0;
	for( auto& iter : dbNodeVec )
	{
		if( nIdx >= _nDBCount ) break;

		_pAdoConnPools[nIdx] = new CAdoConnPool(_nMaxThreadCnt);
		if( nullptr == _pAdoConnPools[nIdx] )
		{
			// [4번 수정] 실패 원인을 구분할 수 있도록 로그 추가 (ODBC판과 동일한 수준으로 맞춤)
			LOG_ERROR(_T("InitAdo: Failed to alloc CAdoConnPool (index=%d)"), nIdx);
			ClearAdoPools();
			return false;
		}

		if( false == _pAdoConnPools[nIdx]->Init(iter._dbClass, iter._tszDSN, 5, reconnectCfg) )
		{
			// [4번 수정] 실패 원인을 구분할 수 있도록 로그 추가
			LOG_ERROR(_T("InitAdo: Failed to Initialize CAdoConnPool (index=%d)"), nIdx);
			ClearAdoPools();
			return false;
		}
		++nIdx;
	}

	_bOpen = true;
	return true;
}

//***************************************************************************
// @brief IO 스레드를 시작합니다.
// @details ThreadManager를 통해 실행 스레드를 생성합니다.
//***************************************************************************
void CAdoAsyncSrv::StartIoThreads()
{
	if( gpThreadManager == nullptr ) return;

	for( int32 i = 0; i < _nMaxThreadCnt; i++ )
	{
		gpThreadManager->CreateThread([this]() { RunningThread(); });
	}
}

//***************************************************************************
// @brief 스레드 실행 루프
// @return 항상 true
//***************************************************************************
bool CAdoAsyncSrv::RunningThread()
{
	if( _bOpen )
	{
		Action();
	}
	return true;
}

//***************************************************************************
// @brief 요청 처리 루프
// @details 큐에서 요청을 꺼내 핸들러로 처리합니다.
// @return 항상 true
//***************************************************************************
bool CAdoAsyncSrv::Action()
{
	CQueue<std::unique_ptr<st_DBAsyncRq>> localQueue;

	while( !_bStopThread.load() )
	{
		std::unique_ptr<st_DBAsyncRq> pAsyncRq = Pop(localQueue);
		if( pAsyncRq == nullptr ) continue;

		COMMAND_MAP::iterator it = _mapCommand.find(pAsyncRq->callIdent);
		if( _mapCommand.end() == it )
		{
			// [3번 수정] 핸들러 미등록으로 이 요청을 더 이상 처리하지 않고 버리는 경로이므로,
			// Push() 이전에 호출부가 걸어둔 AddOutstandingRequest()와 짝을 맞춰 감소시켜야 한다.
			// 이 호출이 없으면 미등록 callIdent가 한 번이라도 들어올 때마다 outstanding 카운터가
			// 영구히 어긋난다.
			SubOutstandingRequest();
			continue;
		}

		std::shared_ptr<CDBAsyncSrvHandler> command = it->second;
		EDBReturnType Ret = command->ProcessAsyncCall(pAsyncRq.get());

		if( Ret != EDBReturnType::OK )
		{
			if( Ret == EDBReturnType::TIMEOUT && pAsyncRq->bReTry == false )
			{
				pAsyncRq->bReTry = true;
				int nSize = Push(std::move(pAsyncRq));
				if( nSize == 0 )
				{
					LOG_ERROR(_T("Failed to retry ADO query because service is stopping..."));
				}
				continue;
			}
		}

		SubOutstandingRequest();
	}

	return true;
}

//***************************************************************************
// @brief 큐에 요청을 추가합니다.
// @param pAsyncRq 비동기 요청 객체
// @return 큐 크기 (0이면 실패)
//***************************************************************************
int CAdoAsyncSrv::Push(std::unique_ptr<st_DBAsyncRq> pAsyncRq)
{
	if( _bStopThread.load() ) return 0;

	// [1번 수정] PushAndGetSize()가 잡는 큐 내부 락과 별개로, Pop()이 대기하는 조건(_cva)을
	// 보호하는 _mutex를 여기서도 잡은 뒤 notify_one()까지 같은 임계구역 안에서 수행한다.
	// - 기존에는 이 구간이 _mutex 밖에서 실행되어, 컨슈머가 "predicate 검사 후 실제 wait()
	//   진입 전" 사이의 틈에 push+notify가 끼어들면 notify가 유실될 수 있었다(lost wakeup).
	// - Pop()도 SwapChunk() 호출 전에 이미 같은 _mutex를 먼저 잡으므로, 잠금 순서는 항상
	//   (_mutex → 큐 내부 락)으로 일관되어 데드락 위험이 없다.
	int queueSize = 0;
	{
		std::lock_guard<std::mutex> lockGuard(_mutex);
		queueSize = static_cast<int>(_queueDBAsyncRq.PushAndGetSize(std::move(pAsyncRq)));
		_cva.notify_one();
	}

	return queueSize;
}

//***************************************************************************
// @brief 큐에서 요청을 꺼냅니다.
// @param localQueue 로컬 큐
// @return 요청 객체 (없으면 nullptr)
//***************************************************************************
std::unique_ptr<st_DBAsyncRq> CAdoAsyncSrv::Pop(CQueue<std::unique_ptr<st_DBAsyncRq>>& localQueue)
{
	if( !localQueue.empty() )
	{
		std::unique_ptr<st_DBAsyncRq> pAsyncRq = std::move(localQueue.front());
		localQueue.pop();
		return pAsyncRq;
	}

	{
		std::unique_lock<std::mutex> lockGuard(_mutex);

		_cva.wait(lockGuard, [this, &localQueue]() {
			return !_queueDBAsyncRq.IsEmpty() || _bStopThread.load();
			});

		if( _bStopThread.load() && _queueDBAsyncRq.IsEmpty() && localQueue.empty() )
			return nullptr;

		// [5번 수정] 큐 크기 경고 로직 재설계.
		//
		// 기존 방식의 두 가지 문제:
		//  (a) LOG_ERROR가 [MAX_WARNING_QUERY_QUEUE_SIZE, +10] 폭 11짜리 좁은 창을 지날 때만
		//      찍혔다 — 부하가 몰려 표본 사이에 크기가 그 창을 건너뛰면 정작 필요한 경고가
		//      누락될 수 있었다.
		//  (b) LOG_WARNING이 "새 최댓값을 경신할 때마다" 무조건 찍혀, 큐가 계속 자라는(=이미
		//      시스템이 과부하인) 바로 그 상황에서 로그 I/O가 함께 폭증했다. 초기값(2)도 너무
		//      낮아 기동 직후 워밍업만으로도 로그가 스팸처럼 남았다.
		//
		// 새 방식:
		//  (a) 임계값을 "넘어서는 순간" 1회만 LOG_ERROR로 알리고(>= 비교, 창 없음), 이후에는
		//      히스테리시스 하한(MAX_WARNING_RESET_QUEUE_SIZE) 아래로 실제로 내려와야 다시
		//      무장(rearm)된다 — 문턱 근처에서 오르내려도 매번 재알림하지 않는다.
		//  (b) "새 최댓값 경신" 시에만 LOG_WARNING 후보로 삼는 것은 유지하되, 실제 로그 출력은
		//      QUEUE_SIZE_WARN_COOLDOWN_MS(기본 5초) 쿨다운을 통과했을 때만 허용한다 — 큐가
		//      계속 자라도 초당 로그 폭주 없이 일정 간격으로만 남는다. 초기 기준치도
		//      INITIAL_WARN_QUEUE_SIZE(1000)로 올려 사소한 워밍업 증가는 무시한다.
		int64 currentSize = _queueDBAsyncRq.GetSize();
		int currentCount = static_cast<int>(currentSize);

		if( !_bMaxWarningActive && currentSize >= MAX_WARNING_QUERY_QUEUE_SIZE )
		{
			_bMaxWarningActive = true;
			LOG_ERROR(_T("Async ADO Call Queue size exceeded critical threshold... : [%d]"), currentCount);
		}
		else if( _bMaxWarningActive && currentSize <= MAX_WARNING_RESET_QUEUE_SIZE )
		{
			_bMaxWarningActive = false; // 히스테리시스 하한 아래로 내려왔으므로 재무장
		}

		if( currentCount > _nLastWarnedQueueSize )
		{
			auto now = std::chrono::steady_clock::now();
			auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
				now - _lastQueueSizeWarnTime).count();

			if( elapsedMs >= QUEUE_SIZE_WARN_COOLDOWN_MS )
			{
				_nLastWarnedQueueSize = currentCount;
				_lastQueueSizeWarnTime = now;
				LOG_WARNING(_T("Async ADO Call Queue size... : [%d]"), currentCount);
			}
		}

		// 전체를 다 가져오는 대신, 한 번에 처리할 적정량(64개)만 떼어옴 (스케일링 개선)
		_queueDBAsyncRq.SwapChunk(localQueue, 64);
	}

	_cvProducer.notify_one();

	if( localQueue.empty() )
		return nullptr;

	std::unique_ptr<st_DBAsyncRq> pAsyncRq = std::move(localQueue.front());
	localQueue.pop();
	return pAsyncRq;
}

//***************************************************************************
// @brief 첫 번째 ADO 연결 풀 반환
// @return 계정용 ADO 연결 풀
//***************************************************************************
CAdoConnPool* CAdoAsyncSrv::GetAccountAdoConnPool(void)
{
	assert(_pAdoConnPools != nullptr && _nDBCount > 0);
	return _pAdoConnPools[0];
}

//***************************************************************************
// @brief ID 기반 ADO 연결 풀 반환
// @param m_nID DB ID
// @return 해당 ADO 연결 풀
//***************************************************************************
CAdoConnPool* CAdoAsyncSrv::GetAdoConnPool(uint64 m_nID)
{
	assert(_pAdoConnPools != nullptr && _nDBCount > 0);

	int32 nIdx = _nDBCount - 1;
	if( 2 < _nDBCount )
	{
		if( 0 < m_nID )
			nIdx = (m_nID % (_nDBCount - 1)) + 1;
	}

	return _pAdoConnPools[nIdx];
}

//***************************************************************************
// @brief 로그용 ADO 연결 풀 반환
// @return 로그 DB 연결 풀
//***************************************************************************
CAdoConnPool* CAdoAsyncSrv::GetLogAdoConnPool()
{
	assert(_pAdoConnPools != nullptr && _nDBCount > 2);
	return _pAdoConnPools[2];
}