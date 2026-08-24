
//***************************************************************************
// HttpConnPoolCommon.h : common types shared by HTTP connection pool classes.
//
//***************************************************************************

#ifndef __HTTPCONNPOOLCOMMON_H__
#define __HTTPCONNPOOLCOMMON_H__

#ifndef	__NETWORKREDEFINEDATATYPE_H__
#include <Network/NetworkRedefineDataType.h>
#endif

#ifndef	__HTTPCLIENTCORE_H__
#include <Network/HTTP/HttpClientCore.h>
#endif

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
};

#endif // ndef __HTTPCONNPOOLCOMMON_H__
