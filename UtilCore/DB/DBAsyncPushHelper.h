
//***************************************************************************
// DBAsyncPushHelper.h : DB 비동기 요청 생성/게시 공용 헬퍼
//
// [설계 노트] COdbcAsyncSrv/CMySQLAsyncSrv/CAdoAsyncSrv 세 헤더를 직접
// 대조해 이 헬퍼가 쓰는 메서드(Instance/GetQueryQueueSize/
// WaitPushCapacity/AddOutstandingRequest/SubOutstandingRequest/Push)의
// 시그니처가 셋 다 정확히 동일함을 확인했다(2026-09 기준). 공식 공통
// 인터페이스(추상 베이스 등)는 없고 세 클래스가 각자 독립적으로 동일한
// 모양을 반복 구현한 것이므로, 이 헬퍼는 여전히 "덕 타이핑" 계약에
// 의존한다 — 셋 중 하나가 나중에 이 메서드들의 시그니처를 벗어나게
// 바뀌면, 그 타입으로 인스턴스화하는 호출부만 컴파일 에러가 난다(템플릿
// 특성상 사용 시점에만 체크되므로 다른 두 백엔드는 영향 없음).
//
// [공용 라이브러리 승격 시 참고]
// 두 변형(논블로킹/블로킹)을 반드시 구분해서 제공한다 — 호출 스레드의
// 성격에 따라 적합한 게 다르기 때문이다:
//   - TryPushDBAsyncRequest()      : 논블로킹. IOCP 워커/네트워크 콜백
//     스레드처럼 "이 스레드를 절대 재우면 안 되는" 곳 전용. 큐가 포화면
//     그냥 실패 반환.
//   - PushDBAsyncRequestBlocking() : 블로킹(WaitPushCapacity). 배치
//     Producer/Consumer처럼 전용 피더/워커 스레드에서만 쓸 것 — 다른
//     세션의 I/O 처리와 공유하지 않는 스레드여야 안전하다.
// 어느 쪽인지 호출부가 실수하지 않도록 함수 이름 자체에 Blocking을
// 박아뒀다 — 이름만 보고 오용하기 어렵게 하는 의도.
//
// [include 안내] 이 헤더는 어떤 구체적인 AsyncSrv 헤더도 강제로 include하지
// 않는다 — 템플릿이라 실제 인스턴스화 시점(호출부)에만 완전한 타입이
// 필요하다. 사용하는 쪽에서 <DB/OdbcAsyncSrv.h> / <DB/MySQLAsyncSrv.h> /
// <DB/AdoAsyncSrv.h> 중 필요한 걸 직접 include할 것.
//***************************************************************************

#ifndef UC_DBASYNCPUSHHELPER_H
#define UC_DBASYNCPUSHHELPER_H

#include <functional>
#include <memory>

//***************************************************************************
// @brief st_DBAsyncRq 파생 요청을 만들어 TSrv(DB 비동기 서비스 싱글턴)에
//        게시하는 공용 헬퍼(논블로킹).
// @details
// [스레드 제약] IOCP 워커/네트워크 콜백 스레드 등, 절대 블로킹되면 안 되는
// 스레드에서만 호출할 것. 큐가 maxQueueCapacity 이상이면 대기하지 않고
// 즉시 false를 반환한다.
//
// 하는 일(순서대로):
//   1. 큐 크기를 논블로킹으로 확인해 maxQueueCapacity 이상이면 즉시 실패 반환
//   2. std::make_unique<TReq>()로 예외 안전하게 요청 생성 + callIdent 설정
//   3. 호출자가 준 initializer로 나머지 필드 채움
//   4. AddOutstandingRequest() 후 Push() — Push()가 실패(0 반환)하면
//      SubOutstandingRequest()로 카운터를 대칭적으로 되돌린다.
// @tparam TSrv Instance()/GetQueryQueueSize()/AddOutstandingRequest()/
//         SubOutstandingRequest()/Push()를 갖는 DB 비동기 서비스 싱글턴
//         (COdbcAsyncSrv/CMySQLAsyncSrv/CAdoAsyncSrv 등)
// @tparam TReq st_DBAsyncRq를 상속하고 기본 생성자를 갖는 요청 타입
// @param cmd 해당 TSrv::Regist()에 등록된 callIdent(BYTE, 0~255)
// @param initializer 생성된 요청 객체(TReq*)에 나머지 필드를 채우는 콜백
// @param maxQueueCapacity 이 값 이상이면 게시 자체를 시도하지 않고 실패 처리
// @return 게시 성공 여부. false면 요청이 아예 만들어지지도, 큐에 들어가지도
//         않았다는 뜻 — 호출부가 실패를 알리는 응답을 직접 처리해야 한다.
//***************************************************************************
template <typename TSrv, typename TReq>
bool TryPushDBAsyncRequest(BYTE cmd, std::function<void(TReq*)> initializer, size_t maxQueueCapacity)
{
	if( static_cast<size_t>(TSrv::Instance()->GetQueryQueueSize()) >= maxQueueCapacity )
		return false;

	auto pDBAsync = std::make_unique<TReq>();
	pDBAsync->callIdent = cmd;

	if( initializer )
		initializer(pDBAsync.get());

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
// @details
// [스레드 제약] 이 함수는 TSrv::WaitPushCapacity()로 큐에 여유가 생길
// 때까지 **호출 스레드를 재운다**. IOCP 워커나 네트워크 콜백처럼 다른
// 세션의 I/O 처리와 스레드를 공유하는 곳에서 부르면, 그 세션들까지 함께
// 지연된다 — 반드시 전용 피더/워커 스레드에서만 사용할 것. 불확실하면
// TryPushDBAsyncRequest()(논블로킹)를 쓰는 게 안전하다.
//
// Push() 자체의 실패(서비스 종료 시점 등)는 여전히 발생할 수 있으므로
// 반환값을 반드시 확인해야 한다 — WaitPushCapacity()로 공간을 기다리는
// 것과 Push() 성공은 별개다.
// @tparam TSrv Instance()/WaitPushCapacity()/AddOutstandingRequest()/
//         SubOutstandingRequest()/Push()를 갖는 DB 비동기 서비스 싱글턴
// @tparam TReq st_DBAsyncRq를 상속하고 기본 생성자를 갖는 요청 타입
// @param cmd 해당 TSrv::Regist()에 등록된 callIdent(BYTE, 0~255)
// @param initializer 생성된 요청 객체(TReq*)에 나머지 필드를 채우는 콜백
// @param maxQueueCapacity WaitPushCapacity()에 그대로 전달할 상한
// @return 게시 성공 여부.
//***************************************************************************
template <typename TSrv, typename TReq>
bool PushDBAsyncRequestBlocking(BYTE cmd, std::function<void(TReq*)> initializer, size_t maxQueueCapacity)
{
	TSrv::Instance()->WaitPushCapacity(maxQueueCapacity);

	auto pDBAsync = std::make_unique<TReq>();
	pDBAsync->callIdent = cmd;

	if( initializer )
		initializer(pDBAsync.get());

	TSrv::Instance()->AddOutstandingRequest();

	if( TSrv::Instance()->Push(std::move(pDBAsync)) == 0 )
	{
		TSrv::Instance()->SubOutstandingRequest();
		return false;
	}

	return true;
}

#endif // ndef UC_DBASYNCPUSHHELPER_H