
//***************************************************************************
// HttpConnPoolCommon.h : common types shared by HTTP connection pool classes.
//
//***************************************************************************

#ifndef UC_HTTPCONNPOOLCOMMON_H
#define UC_HTTPCONNPOOLCOMMON_H

#include <Network/HTTP/HttpClientCore.h>
#include <Network/NetAddress.h>

#include <chrono>
#include <functional>
#include <memory>

//***************************************************************************
// @brief 세션의 연결 상태 변화(연결 완료 / 연결 종료) 통지 콜백
// @details IOCP/RIO 모두 ConnectOneMoreSession()은 연결 시도를 게시만 하고 즉시 리턴하므로,
//          실제 연결 완료/종료는 항상 이 콜백으로만 통지된다 — 엔진별 분기가 필요 없다.
//          connected==true : 이 세션이 방금 연결 완료됨 (OnConnected() 훅에서 호출)
//          connected==false: 이 세션이 끊어짐, 연결 실패 포함 (OnDisconnected() 훅에서 호출) —
//                            풀은 이 세션을 더 쓰지 말고 재연결을 스케줄해야 한다.
//***************************************************************************
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
	// @param timeout 이 요청의 무응답 타임아웃. 0이면 풀 기본값(SetDefaultRequestTimeout), 음수면 이 요청은
	//        타임아웃 없음, 양수면 그 값. 기준 시각은 SendRequest() 호출 시점(풀 대기 시간 포함)이며
	//        응답 데이터가 도착할 때마다 갱신된다. 초과하면 onComplete(false, parser)가 호출되고
	//        parser.IsTimedOut()이 true가 된다.
	// @details 즉시 보낼 수 있는 유휴 커넥션이 없으면 내부 대기열에 쌓였다가 커넥션이 확보되는 대로
	//          순서대로 처리된다. data는 이 호출 안에서 복사되므로 호출부는 반환 직후 버퍼를 해제해도 된다.
	//          풀이 닫혔으면 onComplete(false, ...)가 호출된다.
	//***************************************************************************
	virtual void SendRequest(const char* data, size_t len, HttpRequestCompletionHandler onComplete,
		std::chrono::milliseconds timeout) = 0;

	// 풀 기본 타임아웃을 사용하는 편의 오버로드
	void SendRequest(const char* data, size_t len, HttpRequestCompletionHandler onComplete)
	{
		SendRequest(data, len, std::move(onComplete), std::chrono::milliseconds::zero());
	}

	//***************************************************************************
	// @brief timeout 인자 없이(또는 0으로) 보낸 요청에 적용할 풀 기본 무응답 타임아웃을 설정합니다.
	//        0 이하면 기본적으로 타임아웃을 두지 않습니다.
	//***************************************************************************
	virtual void SetDefaultRequestTimeout(std::chrono::milliseconds timeout) = 0;

	//***************************************************************************
	// @brief 앞으로 만드는 새 연결이 접속할 원격 주소를 바꿉니다 (DNS 재해석 결과 반영).
	// @details 이미 연결된 세션은 그대로 두고, 이후 생성되는 연결(재연결, 풀 확장)만 새 주소를 쓴다.
	//          기존 연결이 옛 주소의 장애로 죽으면 풀의 재연결/GET 재시도가 새 주소로 이어준다.
	//***************************************************************************
	virtual void SetRemoteAddress(const CNetAddress& address) = 0;

	//***************************************************************************
	// @brief 이 풀이 현재 붙들고 있는(연결 완료 + 연결 시도 중 포함) 세션 수를 반환한다.
	// @details Close() 이후에도 세션 정리 완료 통지가 뒤늦게 올 수 있으므로, 이 값이 0이 될 때까지
	//          기다린 뒤에 IOCP/RIO 워커 스레드를 멈춘다. 권장 종료 시퀀스는 CHttpConnPoolManager의
	//          WaitUntilAllSessionsClosed()/SetAllSessionsClosedHandler() 참고.
	//***************************************************************************
	virtual size_t GetActiveSessionCount() = 0;

	//***************************************************************************
	// @brief 이 풀의 연결 완료 세션 수가 변할 때마다 호출되는 콜백을 등록한다.
	// @param handler 새 세션 수를 인자로 받는 콜백. 세션 연결/해제 통지마다 호출되므로 값이 실제로
	//        바뀌지 않았어도 불릴 수 있다 — 멱등하게 처리할 것.
	// @details CHttpConnPoolManager가 폴링 없이 "세션 수가 0이 됐다"를 알기 위해 쓴다. 그 외 용도로
	//          직접 쓸 일은 드물다.
	//***************************************************************************
	virtual void SetSessionCountChangedHandler(std::function<void(size_t)> handler) = 0;
};

#endif // ndef UC_HTTPCONNPOOLCOMMON_H