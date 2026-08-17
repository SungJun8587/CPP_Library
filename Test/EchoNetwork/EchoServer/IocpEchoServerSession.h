
//***************************************************************************
// IocpEchoServerSession.h : interface for the CIocpEchoServerSession class.
//
//***************************************************************************

#ifndef __IOCPECHOSERVERSESSION_H__
#define __IOCPECHOSERVERSESSION_H__

#ifndef __IOCPSESSION_H__
#include <Network/IOCP/IocpSession.h>
#endif

//***************************************************************************
// @class CIocpEchoServerSession
// @brief 서버용 IOCP 에코 세션 클래스
// @details 클라이언트의 연결 수락, 연결 해제, 패킷 수신 및 송신(에코) 이벤트를 처리합니다.
//***************************************************************************
class CIocpEchoServerSession : public CIocpSession
{
public:
	CIocpEchoServerSession();
	virtual ~CIocpEchoServerSession();

	//***************************************************************************
	// @brief 클라이언트 연결 성공 시 호출되는 콜백 함수입니다.
	//***************************************************************************
	virtual void OnConnected() override;

	//***************************************************************************
	// @brief 클라이언트 연결 종료 시 호출되는 콜백 함수입니다.
	//***************************************************************************
	virtual void OnDisconnect() override;

	//***************************************************************************
	// @brief 패킷 수신 시 호출되는 콜백 함수입니다.
	// @details 수신된 데이터를 화면에 출력하고 받은 데이터를 그대로 에코백(Send)합니다.
	// @param buffer 수신된 데이터가 담긴 바이트 버퍼 포인터
	// @param len 수신된 데이터의 길이 (바이트 단위)
	// @return 처리한 데이터의 바이트 길이를 반환합니다.
	//***************************************************************************
	virtual int32 OnRecv(BYTE* buffer, int32 len) override;

	//***************************************************************************
	// @brief 패킷 전송 완료 시 호출되는 콜백 함수입니다.
	// @param len 전송된 데이터의 길이 (바이트 단위)
	//***************************************************************************
	virtual void OnSend(int32 len) override;
};

#endif // __IOCPECHOSERVERSESSION_H__