
//***************************************************************************
// HttpConnPoolCommon.h : common types shared by HTTP connection pool classes.
//
//***************************************************************************

#ifndef UC_HTTPCONNPOOLCOMMON_H
#define UC_HTTPCONNPOOLCOMMON_H

#include <Network/HTTP/HttpClientCore.h>

#include <functional>
#include <memory>

// 세션의 연결 상태 변화(연결 완료 / 연결 종료) 통지 콜백. IOCP/RIO 둘 다
// ConnectOneMoreSession()이 "게시만 하고" 즉시 리턴하는 비동기 계약이라
// (RioConnectDispatcher 도입 이후 RIO도 IOCP와 동일), 실제 연결 완료/종료는
// 항상 이 콜백을 통해서만 통지된다 — 엔진에 따른 분기가 필요 없다.
// connected==true: 이 세션이 방금 연결 완료됨(OnConnected() 훅에서 호출).
// connected==false: 이 세션이 끊어짐, 연결 실패 포함(OnDisconnected() 훅에서
//                    호출) — 풀은 이 세션을 더 이상 사용하지 말고 재연결을
//                    스케줄해야 함.
using HttpConnStateHandler = std::function<void(CSessionRef, bool)>;

//***************************************************************************
// @class IHttpConnPool
// @brief 엔진(IOCP/RIO) 비의존 HTTP 커넥션 풀 인터페이스
//***************************************************************************
class IHttpConnPool
{
public:
	virtual ~IHttpConnPool() = default;

	virtual bool Start() = 0;
	virtual void Close() = 0;

	//***************************************************************************
	// @brief 완성된 HTTP 요청 패킷을 이 풀이 관리하는 host로 비동기 전송한다.
	// @param data 요청 패킷 바이트 (예: CHttpRequestBuilder::Build()의 결과)
	// @param len data의 길이
	// @param onComplete 응답 완결(성공 또는 에러) 시 호출되는 콜백
	// @details 즉시 보낼 수 있는 유휴 커넥션이 없으면 내부 대기열에 쌓였다가
	//          커넥션이 확보되는 대로 순서대로 처리된다. data는 이 호출 안에서
	//          내부적으로 복사되므로, 호출부가 반환 후 즉시 버퍼를 해제해도
	//          안전하다(대기열에 오래 남아있을 수 있어 원본 수명에 의존할 수 없음).
	//***************************************************************************
	virtual void SendRequest(const char* data, size_t len, HttpRequestCompletionHandler onComplete) = 0;

	//***************************************************************************
	// @brief 이 풀이 현재 붙들고 있는(연결 완료 + 연결 시도 중 포함) 세션 수를 반환한다.
	// @return size_t 살아있는 세션 수
	// @details Close()가 세션 정리를 비동기로만 게시하고 실제 정리 완료는 IOCP/RIO
	//          워커 스레드가 완료 통지를 처리해야 끝나기 때문에, "Close() 호출 =
	//          즉시 정리 완료"가 아니다. 이 값이 0이 될 때까지 기다린 뒤에야
	//          워커 스레드를 안전하게 멈춰도 된다 — 그렇지 않으면 QUIT_KEY가
	//          아직 처리 안 된 세션 정리 완료 패킷보다 먼저 큐에서 뽑혀 나가면서
	//          그 세션이 영원히 정리 안 되는(메모리 릭으로 보이는) 레이스가
	//          생길 수 있다(호출부의 권장 종료 시퀀스는 CHttpConnPoolManager.h의
	//          WaitUntilAllSessionsClosed()/SetAllSessionsClosedHandler() 참고).
	//***************************************************************************
	virtual size_t GetActiveSessionCount() = 0;

	//***************************************************************************
	// @brief 이 풀의 활성 세션 수가 변할 때마다 호출되는 콜백을 등록한다.
	// @param handler 새 세션 수를 인자로 받는 콜백. 실제로 값이 바뀌지 않았는데도
	//        호출될 수 있다(멱등하게/방어적으로 처리할 것) — 세션 연결/해제
	//        시점마다 통지하는 구현이라, 정확히 "변화가 있을 때만"이 보장되진
	//        않는다.
	// @details CHttpConnPoolManager가 폴링 없이 "세션 수가 0이 됐다"를 알아채기
	//          위해 쓴다(WaitUntilAllSessionsClosed()/SetAllSessionsClosedHandler()
	//          가 이 훅으로 구현돼 있음) — 그 외 목적으로 직접 쓸 필요는 보통 없다.
	//***************************************************************************
	virtual void SetSessionCountChangedHandler(std::function<void(size_t)> handler) = 0;
};
using IHttpConnPoolRef = std::shared_ptr<IHttpConnPool>;

#endif // ndef UC_HTTPCONNPOOLCOMMON_H