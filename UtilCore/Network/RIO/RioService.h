//***************************************************************************
// RioService.h : interface for the CRioServerService & CRioClientService.
//
//***************************************************************************

#ifndef __RIOSERVICE_H__
#define __RIOSERVICE_H__

#ifndef __NET_SERVICE_H__
#include <Network/NetService.h>
#endif

#ifndef __RIO_CORE_H__
#include <Network/Rio/RioCore.h>
#endif

#ifndef __RIO_LISTENER_H__
#include <Network/Rio/RioListener.h>
#endif

// 전방 선언
class CRioBuffer;

//***************************************************************************
// @class CRioServerService
// @brief RIO(Registered I/O) 기반의 서버 전용 서비스 클래스
// @details
// 역할:
//      1. CRioListener를 내부에 두고 동기 블로킹 accept 루프 및 RIO RQ 할당 총괄 제어
//      2. 클라이언트 접속 시 세션 팩토리를 통해 세션을 생성하고 콜백을 통해 관리
//***************************************************************************
class CRioServerService : public CNetService
{
public:
	CRioServerService(CNetAddress address, CRioCoreRef rioCore, SessionFactory factory, int32 maxSessionCount = 1);
	virtual ~CRioServerService() = default;

	//***************************************************************************
	// @brief RIO 서버 구동 (Listener 생성 및 Accept 루프 시작)
	// @return bool 성공 시 true, 실패 시 false
	//***************************************************************************
	virtual bool	Start() override;

	//***************************************************************************
	// @brief RIO 서버 서비스 종료 (Listener 정지 및 세션 일괄 해제)
	//***************************************************************************
	virtual void	Close() override;

	//***************************************************************************
	// @brief 서비스 전역에서 관리되는 수신용 글로벌 CRioBuffer 객체를 반환합니다.
	// @return CRioBuffer* 수신 버퍼 포인터
	//***************************************************************************
	CRioBuffer* GetGlobalRecvBuffer() { return _globalRecvBuffer.get(); }

	//***************************************************************************
	// @brief RIO 전송에 사용되는 메모리 버퍼의 고유 ID(RIO_BUFFERID)를 반환합니다.
	// @return RIO_BUFFERID 전송 버퍼 ID
	//***************************************************************************
	RIO_BUFFERID GetSendBufferId() const { return _sendBufferId; }

	//***************************************************************************
	// @brief 소속된 RIO Core 객체를 반환합니다.
	// @return CRioCoreRef RIO Core 참조 객체
	//***************************************************************************
	CRioCoreRef		GetRioCore() const { return _rioCore; }

private:
	CRioCoreRef		_rioCore = nullptr;						// 연동된 RIO 코어 참조
	CRioListenerRef _listener = nullptr;					// RIO 접속 수락 리스너
	CRioEventPool	_eventPool;
	CRioBufferRef	_globalRecvBuffer;						// 클라이언트 비동기 수신(RIOReceive)용 글로벌 RIO 버퍼 객체
	void*			_sendBufferPtr = nullptr;				// RIO 전송을 위해 예약 및 커밋된 대용량 송신 메모리의 시작 주소 포인터
	RIO_BUFFERID	_sendBufferId = RIO_INVALID_BUFFERID;	// OS에 등록(RIORegisterBuffer)된 송신 메모리 영역의 고유 식별자 핸들
};

//***************************************************************************
// @class CRioClientService
// @brief RIO(Registered I/O) 기반의 클라이언트 전용 서비스 클래스
// @details
// 역할:
//      1. 지정된 개수만큼 RIO 세션을 생성하고 RIO 링 버퍼 및 RQ 설정 준비
//      2. 클라이언트 네트워크 연결, 소켓 등록 및 수명 주기 관리
//***************************************************************************
class CRioClientService : public CNetService
{
public:
	//***************************************************************************
	// @brief CRioClientService 생성자
	// @param address 접속할 서버의 네트워크 주소 정보
	// @param rioCore 연동할 RIO 코어 참조 객체
	// @param factory 세션 객체 생성을 위한 팩토리 함수
	// @param maxSessionCount 생성 및 관리할 최대 클라이언트 세션 수 (기본값: 1)
	//***************************************************************************
	CRioClientService(CNetAddress address, CRioCoreRef rioCore, SessionFactory factory, int32 maxSessionCount = 1);

	//***************************************************************************
	// @brief CRioClientService 소멸자
	//***************************************************************************
	virtual ~CRioClientService() = default;

	//***************************************************************************
	// @brief RIO 클라이언트 서비스를 구동합니다 (소켓 생성, 서버 접속 및 RIO RQ 바인딩).
	// @return bool 서비스 가동 및 연결 성공 시 true, 실패 시 false
	//***************************************************************************
	virtual bool	Start() override;

	//***************************************************************************
	// @brief RIO 클라이언트 서비스를 종료하고 자원(송신 메모리 등)을 해제합니다.
	//***************************************************************************
	virtual void	Close() override;

	//***************************************************************************
	// @brief 소속된 RIO Core 객체의 참조를 반환합니다.
	// @return CRioCoreRef RIO Core 참조 객체
	//***************************************************************************
	CRioCoreRef		GetRioCore() const { return _rioCore; }

	//***************************************************************************
	// @brief 서비스 전역에서 비동기 수신(Receive)용으로 관리되는 CRioBuffer 객체를 반환합니다.
	// @return CRioBuffer* 글로벌 수신 버퍼 포인터
	//***************************************************************************
	CRioBuffer* GetGlobalRecvBuffer() { return _globalRecvBuffer.get(); }

	//***************************************************************************
	// @brief RIO 전송에 사용되는 메모리 영역의 고유 식별자(RIO_BUFFERID)를 반환합니다.
	// @return RIO_BUFFERID 전송 버퍼 ID 핸들
	//***************************************************************************
	RIO_BUFFERID	GetSendBufferId() const { return _sendBufferId; }

private:
	CRioCoreRef		_rioCore = nullptr;						// 연동된 RIO 코어 참조 객체
	CRioEventPool	_eventPool;
	CRioBufferRef	_globalRecvBuffer;						// 클라이언트 비동기 수신(RIOReceive)용 글로벌 RIO 버퍼 객체
	void*			_sendBufferPtr = nullptr;				// RIO 전송을 위해 예약 및 커밋된 대용량 송신 메모리 시작 주소 포인터
	RIO_BUFFERID	_sendBufferId = RIO_INVALID_BUFFERID;	// OS에 등록(RIORegisterBuffer)된 송신 메모리 영역의 고유 식별자 핸들
};


#endif // ndef __RIOSERVICE_H__