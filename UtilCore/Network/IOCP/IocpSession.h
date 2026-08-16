//***************************************************************************
// IocpSession.h : interface for the CIocpSession class.
//
//***************************************************************************

#ifndef __IOCPSESSION_H__
#define __IOCPSESSION_H__

#ifndef __IOCPCORE_H__
#include <Network/IocpCore.h>
#endif

#ifndef __IOCPEVENT_H__
#include <Network/IocpEvent.h>
#endif

#ifndef __RINGBUFFER_H__
#include <Network/RingBuffer.h>
#endif

#include <atomic>
#include <mutex>

class CIocpSession;
using CIocpSessionRef = std::shared_ptr<CIocpSession>;

//***************************************************************************
// @class CIocpSession
// @brief CSession을 상속받는 IOCP 네트워크 통신의 핵심인 연결 세션 추상 기반 클래스.
// @details
// 역할:
//      1. 소켓 관리, WSARecv/WSASend/DisconnectEx 호출 및 완료 이벤트 처리
//      2. Scatter-Gather Send 지원 (여러 패킷을 1회 WSASend로 일괄 전송)
//      3. CRingBuffer를 통한 제로카피 비동기 데이터 수신 관리 (WSARecv)
//      4. 상위 응용 레이어(GameSession 등)로 가상 함수 이벤트(OnConnected 등) 전달
//***************************************************************************
class CIocpSession : public CSession, public CIocpObject
{
public:
	CIocpSession();
	virtual ~CIocpSession();

	// CIocpObject의 순수 가상 함수 구현
	virtual CIocpObjectRef GetIocpObjectPtr() override
	{
		// CSession의 shared_from_this()를 CIocpObject* 타입의 shared_ptr로 안전하게 변환
		return CIocpObjectRef(shared_from_this(), static_cast<CIocpObject*>(this));
	}

public:
	// CIocpObject 인터페이스 구현
	virtual HANDLE	GetHandle() override { return reinterpret_cast<HANDLE>(_socket); }
	virtual void	Dispatch(class CIocpEvent* iocpEvent, int32 numOfBytes = 0) override;

public:
	// CSession 공통 인터페이스 오버라이드
	virtual void	Disconnect(const TCHAR* cause) override;
	virtual bool	IsConnected() const override { return _connected.load(); }
	virtual SOCKET	GetSocket() const override { return _socket; }

	void			Disconnect(Iocp::CloseReason reason);    // 오버로드된 안전한 종료 함수

	//***************************************************************************
	// @brief 소켓 핸들이 유효한 상태인지 확인합니다.
	// @return bool _socket이 INVALID_SOCKET이 아니면 true, 아니면 false
	//***************************************************************************
	bool			IsValid() const { return _socket != INVALID_SOCKET; }

	//***************************************************************************
	// @brief 원격 클라이언트의 네트워크 주소(IP/Port)를 설정합니다.
	// @param netAddr 설정할 CNetAddress 객체
	//***************************************************************************
	void			SetNetAddress(CNetAddress netAddr) { _netAddress = netAddr; }

	//***************************************************************************
	// @brief 원격 클라이언트의 네트워크 주소(IP/Port)를 반환합니다.
	// @return CNetAddress 네트워크 주소 객체
	//***************************************************************************
	CNetAddress		GetNetAddress() const { return _netAddress; }

	//***************************************************************************
	// @brief 세션 종료 사유를 반환합니다.
	// @return Iocp::CloseReason 세션 종료 사유
	//***************************************************************************
	Iocp::CloseReason GetCloseReason() const noexcept { return _closeReason.load(std::memory_order_acquire); }

	//***************************************************************************
	// @brief 고유 세션 ID를 설정합니다.
	// @param sessionId 설정할 고유 세션 ID
	//***************************************************************************
	void SetSessionId(uint64_t sessionId) noexcept { _sessionId = sessionId; }

	//***************************************************************************
	// @brief 고유 세션 ID를 반환합니다.
	// @return uint64_t 고유 세션 ID
	//***************************************************************************
	uint64_t GetSessionId() const noexcept { return _sessionId; }

public:
	void			ProcessConnect();	// CIocpListener의 OnAcceptCallback 등에서 연결 수락 완료 후 호출
	bool			Send(const void* data, uint16_t size) noexcept;

protected:
	// 상위 콘텐츠 레이어(CGameSession 등)에서 오버라이딩할 가상 함수
	virtual void	OnConnected() {}
	virtual void	OnDisconnect() {}
	virtual int32	OnRecv(BYTE* buffer, int32 len) { return len; }
	virtual void	OnSend(int32 len) {}

private:
	void			Send(CSendBufferRef sendBuffer);

	void			RegisterRecv();
	void			RegisterSend();
	void			RegisterDisconnect();

	void			ProcessRecv(int32 numOfBytes);
	void			ProcessSend(int32 numOfBytes);
	void			ProcessDisconnect();

private:
	uint64_t				_sessionId{ 0 };					// 고유 세션 ID
	SOCKET					_socket = INVALID_SOCKET;			// 통신에 사용되는 WinSock 소켓 핸들
	CNetAddress				_netAddress;						// 원격 클라이언트의 IP 주소 및 포트 정보
	
	std::atomic<bool>				_connected = false;							// 원자적(Atomic) 연산을 보장하는 세션 연결/해제 상태 플래그
	std::atomic<Iocp::CloseReason>	_closeReason{ Iocp::CloseReason::None };	// 세션 종료 사유 변수

	std::mutex				_lock;                          // 송신 큐(_sendQueue) 스레드 동기화를 위한 뮤텍스
	CRingBuffer				_recvBuffer;                    // 제로카피 비동기 수신(WSARecv)을 관리하는 수신 링버퍼
	CVector<CSendBufferRef> _sendQueue;                     // 전송 대기 중인 패킷 참조(CSendBufferRef)들을 보관하는 송신 큐
	std::atomic<bool>		_sendRegistered = false;        // WSASend 비동기 요청 중복 호출을 방지하는 원자적 등록 상태 플래그

	RecvEvent				_recvEvent;                     // 비동기 수신(WSARecv) 요청 및 완료 처리를 위한 OVERLAPPED 이벤트 객체
	SendEvent				_sendEvent;                     // 비동기 송신(WSASend) 요청 및 완료 처리를 위한 OVERLAPPED 이벤트 객체
	DisconnectEvent			_disconnectEvent;               // 비동기 해제(DisconnectEx) 요청 및 완료 처리를 위한 OVERLAPPED 이벤트 객체
};

#endif // ndef __IOCPSESSION_H__