
//***************************************************************************
// RioEchoClientSession.h : interface for the CRioEchoClientSession class.
//
//***************************************************************************

#ifndef __RIOECHOCLIENTSESSION_H__
#define __RIOECHOCLIENTSESSION_H__

#ifndef __RIOSESSION_H__
#include <Network/RIO/RioSession.h>
#endif

//***************************************************************************
// @class CRioEchoClientSession
// @brief 클라이언트용 RIO 에코 세션 클래스
// @details RIO 환경에서 서버와의 연결 및 수신 데이터를 처리합니다.
//***************************************************************************
class CRioEchoClientSession : public CRioSession
{
public:
	//***************************************************************************
	// @brief 서버 연결 성공 시 호출되는 오버라이드 함수입니다.
	//***************************************************************************
	virtual void OnConnected() override;

	//***************************************************************************
	// @brief 서버 연결 종료 시 호출되는 오버라이드 함수입니다.
	// @param reason 세션 종료 사유
	//***************************************************************************
	virtual void OnDisconnected(Rio::CloseReason reason) override;

	//***************************************************************************
	// @brief 서버로부터 데이터 수신 시 호출되는 오버라이드 함수입니다.
	//***************************************************************************
	virtual void OnDataReceived() override;

	// 응답 대기 상태 설정 및 확인 함수
	void SetWaitingForEcho(bool waiting) { _waitingForEcho = waiting; }
	bool IsWaitingForEcho() const { return _waitingForEcho; }

private:
	std::atomic<bool> _waitingForEcho{ false };
};

#endif // __RIOECHOCLIENTSESSION_H__