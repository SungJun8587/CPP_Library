
//***************************************************************************
// IocpEchoClientSession.h : interface for the CIocpEchoClientSession class.
//
//***************************************************************************

#ifndef __IOCPECHOCLIENTSESSION_H__
#define __IOCPECHOCLIENTSESSION_H__

#ifndef __IOCPSESSION_H__
#include <Network/IOCP/IocpSession.h>
#endif

//***************************************************************************
// @class CIocpEchoClientSession
// @brief 클라이언트용 IOCP 에코 세션 클래스
// @details 서버와의 연결 및 데이터 수신 이벤트를 처리합니다.
//***************************************************************************
class CIocpEchoClientSession : public CIocpSession
{
public:
	//***************************************************************************
	// @brief 서버 연결 성공 시 호출되는 콜백 함수입니다.
	//***************************************************************************
	virtual void OnConnected() override;

	//***************************************************************************
	// @brief 서버로부터 패킷 수신 시 호출되는 콜백 함수입니다.
	// @param buffer 수신된 데이터가 담긴 바이트 버퍼 포인터
	// @param len 수신된 데이터의 길이 (바이트 단위)
	// @return 처리한 데이터의 바이트 길이를 반환합니다.
	//***************************************************************************
	virtual int32 OnRecv(BYTE* buffer, int32 len) override;

	// 응답 대기 상태 설정 및 확인 함수
	void SetWaitingForEcho(bool waiting) { _waitingForEcho = waiting; }
	bool IsWaitingForEcho() const { return _waitingForEcho; }

private:
	std::atomic<bool> _waitingForEcho{ false };
};

#endif // __IOCPECHOCLIENTSESSION_H__