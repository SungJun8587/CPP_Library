
//***************************************************************************
// DBAsyncPushHelper.h : DB 비동기 요청 생성/게시 공용 헬퍼
//
//***************************************************************************

#ifndef UC_DBASYNCPUSHHELPER_H
#define UC_DBASYNCPUSHHELPER_H

#include <functional>
#include <memory>

//***************************************************************************
// @brief st_DBAsyncRq 파생 요청을 만들어 TSrv(DB 비동기 서비스 싱글턴)에 게시하는 공용 헬퍼(논블로킹).
// @param cmd 해당 TSrv::Regist()에 등록된 callIdent(BYTE, 0~255)
// @param initializer 생성된 요청 객체(TReq*)에 나머지 필드를 채우는 콜백
// @param maxQueueCapacity 이 값 이상이면 게시 자체를 시도하지 않고 실패 처리
// @return bool 게시 성공 여부. false면 요청이 생성되거나 큐에 들어가지 않음
//***************************************************************************
template <typename TSrv, typename TReq>
bool TryPushDBAsyncRequest(BYTE cmd, std::function<void(TReq*)> initializer, size_t maxQueueCapacity)
{
	// 1. 큐 크기를 논블로킹으로 확인해 maxQueueCapacity 이상이면 즉시 실패 반환
	if( static_cast<size_t>(TSrv::Instance()->GetQueryQueueSize()) >= maxQueueCapacity )
		return false;

	// 2. std::make_unique<TReq>()로 예외 안전하게 요청 생성 + callIdent 설정
	auto pDBAsync = std::make_unique<TReq>();
	pDBAsync->callIdent = cmd;

	// 3. 호출자가 준 initializer로 나머지 필드 채움
	if( initializer )
		initializer(pDBAsync.get());

	// 4. AddOutstandingRequest() 후 Push() — Push()가 실패(0 반환)하면 SubOutstandingRequest()로 카운터를 대칭적으로 되돌린다.
	TSrv::Instance()->AddOutstandingRequest();

	if( TSrv::Instance()->Push(std::move(pDBAsync)) == 0 )
	{
		TSrv::Instance()->SubOutstandingRequest();
		return false;
	}

	return true;
}

//***************************************************************************
// @brief st_DBAsyncRq 파생 요청을 만들어 TSrv에 게시하는 공용 헬퍼(블로킹).
// @param cmd 해당 TSrv::Regist()에 등록된 callIdent(BYTE, 0~255)
// @param initializer 생성된 요청 객체(TReq*)에 나머지 필드를 채우는 콜백
// @param maxQueueCapacity WaitPushCapacity()에 그대로 전달할 상한
// @return bool 게시 성공 여부
//***************************************************************************
template <typename TSrv, typename TReq>
bool PushDBAsyncRequestBlocking(BYTE cmd, std::function<void(TReq*)> initializer, size_t maxQueueCapacity)
{
	// 1. WaitPushCapacity()로 큐에 여유 공간이 생길 때까지 대기
	TSrv::Instance()->WaitPushCapacity(maxQueueCapacity);

	// 2. std::make_unique<TReq>()로 예외 안전하게 요청 생성 + callIdent 설정
	auto pDBAsync = std::make_unique<TReq>();
	pDBAsync->callIdent = cmd;

	// 3. 호출자가 준 initializer로 나머지 필드 채움
	if( initializer )
		initializer(pDBAsync.get());

	// 4. AddOutstandingRequest() 후 Push() — Push()가 실패(0 반환)하면 SubOutstandingRequest()로 카운터를 대칭적으로 되돌린다.
	TSrv::Instance()->AddOutstandingRequest();

	if( TSrv::Instance()->Push(std::move(pDBAsync)) == 0 )
	{
		TSrv::Instance()->SubOutstandingRequest();
		return false;
	}

	return true;
}

//***************************************************************************
// @brief st_DBAsyncRq 파생 요청을 만들어 인스턴스 참조(srv)에 게시하는 공용 헬퍼(논블로킹).
// @param srv DB 비동기 서비스 인스턴스 참조(CAdoAsyncSrv, CMySQLAsyncSrv, COdbcAsyncSrv)
// @param cmd 해당 TSrv::Regist()에 등록된 callIdent(BYTE, 0~255)
// @param initializer 생성된 요청 객체(TReq*)에 나머지 필드를 채우는 콜백
// @param maxQueueCapacity 이 값 이상이면 게시 자체를 시도하지 않고 실패 처리
// @return bool 게시 성공 여부. false면 요청이 생성되거나 큐에 들어가지 않음
//***************************************************************************
template <typename TSrv, typename TReq>
bool TryPushDBAsyncRequest(TSrv& srv, BYTE cmd, std::function<void(TReq*)> initializer, size_t maxQueueCapacity)
{
	// 1. 큐 크기를 논블로킹으로 확인해 maxQueueCapacity 이상이면 즉시 실패 반환
	if( static_cast<size_t>(srv.GetQueryQueueSize()) >= maxQueueCapacity )
		return false;

	// 2. std::make_unique<TReq>()로 예외 안전하게 요청 생성 + callIdent 설정
	auto pDBAsync = std::make_unique<TReq>();
	pDBAsync->callIdent = cmd;

	// 3. 호출자가 준 initializer로 나머지 필드 채움
	if( initializer )
		initializer(pDBAsync.get());

	// 4. AddOutstandingRequest() 후 Push() — Push()가 실패(0 반환)하면 SubOutstandingRequest()로 카운터를 대칭적으로 되돌린다.
	srv.AddOutstandingRequest();

	if( srv.Push(std::move(pDBAsync)) == 0 )
	{
		srv.SubOutstandingRequest();
		return false;
	}

	return true;
}

//***************************************************************************
// @brief st_DBAsyncRq 파생 요청을 만들어 인스턴스 참조(srv)에 게시하는 공용 헬퍼(블로킹).
// @param srv DB 비동기 서비스 인스턴스 참조(CAdoAsyncSrv, CMySQLAsyncSrv, COdbcAsyncSrv)
// @param cmd 해당 TSrv::Regist()에 등록된 callIdent(BYTE, 0~255)
// @param initializer 생성된 요청 객체(TReq*)에 나머지 필드를 채우는 콜백
// @param maxQueueCapacity WaitPushCapacity()에 그대로 전달할 상한
// @return bool 게시 성공 여부
//***************************************************************************
template <typename TSrv, typename TReq>
bool TryPushDBAsyncRequestBlocking(TSrv& srv, BYTE cmd, std::function<void(TReq*)> initializer, size_t maxQueueCapacity)
{
	// 1. WaitPushCapacity()로 큐에 여유 공간이 생길 때까지 대기
	srv.WaitPushCapacity(maxQueueCapacity);

	// 2. std::make_unique<TReq>()로 예외 안전하게 요청 생성 + callIdent 설정
	auto pDBAsync = std::make_unique<TReq>();
	pDBAsync->callIdent = cmd;

	// 3. 호출자가 준 initializer로 나머지 필드 채움
	if( initializer )
		initializer(pDBAsync.get());

	// 4. AddOutstandingRequest() 후 Push() — Push()가 실패(0 반환)하면 SubOutstandingRequest()로 카운터를 대칭적으로 되돌린다.
	srv.AddOutstandingRequest();

	if( srv.Push(std::move(pDBAsync)) == 0 )
	{
		srv.SubOutstandingRequest();
		return false;
	}

	return true;
}

#endif // ndef UC_DBASYNCPUSHHELPER_H