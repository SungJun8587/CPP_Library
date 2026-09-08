
//***************************************************************************
// DBAsyncPushHelper.h : DB 비동기 요청 생성/게시 공용 헬퍼
//
//***************************************************************************

#ifndef UC_DBASYNCPUSHHELPER_H
#define UC_DBASYNCPUSHHELPER_H

#include <memory>
#include <type_traits>

//***************************************************************************
// @brief st_DBAsyncRq 파생 요청을 만들어 srv(DB 비동기 서비스 인스턴스)에
//        게시하는 공용 헬퍼(논블로킹).
// @details
// [스레드 제약] IOCP 워커/네트워크 콜백 스레드 등, 절대 블로킹되면 안 되는
// 스레드에서만 호출할 것. 큐가 maxQueueCapacity 이상이면 대기하지 않고
// 즉시 false를 반환한다.
// [수정 — std::function 제거] 이전에는 initializer가 std::function<void(TReq*)>
// 였다 — DB 요청 하나당 호출되는 고빈도 경로인데, 캡처 크기가 SBO(보통
// 16바이트 내외)를 넘는 람다를 넘기면 매 호출마다 std::function 내부에서
// 힙 할당이 발생했다. Fn을 템플릿 파라미터로 받아 perfect-forwarding하는
// 방식으로 바꿔 타입 소거/힙 할당 없이 인라이닝되도록 한다. TSrv/TReq는
// 이전처럼 호출부가 명시적으로 지정하고(예: PushDBAsyncRequest<COdbcAsyncSrv,
// MyReq>(...)), Fn은 인자로부터 항상 추론되므로 기존 호출 코드는 그대로
// 컴파일된다. nullptr을 넘기던 기존 호출 패턴도 is_invocable 분기로 계속
// 지원한다.
// @tparam TSrv GetQueryQueueSize()/AddOutstandingRequest()/
//         SubOutstandingRequest()/Push()를 갖는 DB 비동기 서비스 타입
//         (COdbcAsyncSrv/CMySQLAsyncSrv/CAdoAsyncSrv 등)
// @tparam TReq st_DBAsyncRq를 상속하고 기본 생성자를 갖는 요청 타입
// @tparam Fn TReq*를 받아 나머지 필드를 채우는 호출 가능 객체 타입(인자로부터
//         자동 추론됨 — 명시적으로 지정하지 않는다). nullptr도 허용된다.
// @param srv 게시 대상 서비스 인스턴스(예: CDbServiceManager::Instance().MemberDB())
// @param cmd 해당 srv에 Regist()로 등록된 callIdent(BYTE, 0~255)
// @param initializer 생성된 요청 객체(TReq*)에 나머지 필드를 채우는 콜백
// @param maxQueueCapacity 이 값 이상이면 게시 자체를 시도하지 않고 실패 처리
// @return 게시 성공 여부. false면 요청이 아예 만들어지지도, 큐에 들어가지도
//         않았다는 뜻 — 호출부가 실패를 알리는 응답을 직접 처리해야 한다.
//***************************************************************************
template <typename TSrv, typename TReq, typename Fn>
bool PushDBAsyncRequest(TSrv& srv, BYTE cmd, Fn&& initializer, size_t maxQueueCapacity)
{
	// 1. 큐 크기를 논블로킹으로 확인해 maxQueueCapacity 이상이면 즉시 실패 반환
	if( static_cast<size_t>(srv.GetQueryQueueSize()) >= maxQueueCapacity )
		return false;

	// 2. std::make_unique<TReq>()로 예외 안전하게 요청 생성 + callIdent 설정
	auto pDBAsync = std::make_unique<TReq>();
	pDBAsync->callIdent = cmd;

	// 3. 호출자가 준 initializer로 나머지 필드 채움
	//    (nullptr 등 TReq*를 받아 호출할 수 없는 타입이면 컴파일 타임에 스킵)
	if constexpr( std::is_invocable_v<Fn, TReq*> )
	{
		initializer(pDBAsync.get());
	}

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
// @brief st_DBAsyncRq 파생 요청을 만들어 srv에 게시하는 공용 헬퍼(블로킹).
// @details
// [스레드 제약] 이 함수는 srv.WaitPushCapacity()로 큐에 여유가 생길 때까지
// **호출 스레드를 재운다**. IOCP 워커나 네트워크 콜백처럼 다른 세션의
// I/O 처리와 스레드를 공유하는 곳에서 부르면, 그 세션들까지 함께 지연된다
// — 반드시 전용 피더/워커 스레드에서만 사용할 것. 불확실하면
// PushDBAsyncRequest()(논블로킹)를 쓰는 게 안전하다.
// [수정 — std::function 제거] PushDBAsyncRequest()와 동일한 이유로 Fn을
// 템플릿 파라미터로 받아 perfect-forwarding한다.
// @tparam TSrv WaitPushCapacity()/AddOutstandingRequest()/
//         SubOutstandingRequest()/Push()를 갖는 DB 비동기 서비스 타입
// @tparam TReq st_DBAsyncRq를 상속하고 기본 생성자를 갖는 요청 타입
// @tparam Fn TReq*를 받아 나머지 필드를 채우는 호출 가능 객체 타입(인자로부터
//         자동 추론됨 — 명시적으로 지정하지 않는다). nullptr도 허용된다.
// @param srv 게시 대상 서비스 인스턴스
// @param cmd 해당 srv에 Regist()로 등록된 callIdent(BYTE, 0~255)
// @param initializer 생성된 요청 객체(TReq*)에 나머지 필드를 채우는 콜백
// @param maxQueueCapacity WaitPushCapacity()에 그대로 전달할 상한
// @return 게시 성공 여부.
//***************************************************************************
template <typename TSrv, typename TReq, typename Fn>
bool PushDBAsyncRequestBlocking(TSrv& srv, BYTE cmd, Fn&& initializer, size_t maxQueueCapacity)
{
	// 1. WaitPushCapacity()로 큐에 여유 공간이 생길 때까지 대기
	srv.WaitPushCapacity(maxQueueCapacity);

	// 2. std::make_unique<TReq>()로 예외 안전하게 요청 생성 + callIdent 설정
	auto pDBAsync = std::make_unique<TReq>();
	pDBAsync->callIdent = cmd;

	// 3. 호출자가 준 initializer로 나머지 필드 채움
	//    (nullptr 등 TReq*를 받아 호출할 수 없는 타입이면 컴파일 타임에 스킵)
	if constexpr( std::is_invocable_v<Fn, TReq*> )
	{
		initializer(pDBAsync.get());
	}

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