
//***************************************************************************
// IocpSession.h : interface for the CIocpSession class.
//
//***************************************************************************

#ifndef UC_IOCPSESSION_H
#define UC_IOCPSESSION_H

#include <Network/IOCP/IocpCore.h>
#include <Network/IOCP/IocpEvent.h>
#include <Network/RingBuffer.h>

#include <atomic>
#include <mutex>

//***************************************************************************
// @class CIocpSession
// @brief CSession을 상속받는 IOCP 네트워크 통신의 핵심인 연결 세션 추상 기반 클래스.
// @details
// 역할:
//      1. 소켓 관리, WSARecv/WSASend/DisconnectEx 호출 및 완료 이벤트 처리
//      2. Scatter-Gather Send 지원 (여러 패킷을 1회 WSASend로 일괄 전송)
//      3. CRingBuffer를 통한 제로카피 비동기 데이터 수신 관리 (WSARecv)
//      4. 상위 응용 레이어(GameSession 등)로 가상 함수 이벤트(OnConnected 등) 전달
//      5. ConnectEx를 통한 클라이언트 측 비동기 연결(ConnectAsync) 지원
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
	void SetSessionId(uint64 sessionId) noexcept { _sessionId = sessionId; }

	//***************************************************************************
	// @brief 고유 세션 ID를 반환합니다.
	// @return uint64 고유 세션 ID
	//***************************************************************************
	uint64 GetSessionId() const noexcept { return _sessionId; }

public:
	void			ProcessConnect();	// CIocpListener의 OnAcceptCallback 등에서 연결 수락 완료 후 호출
	bool			Send(const void* data, uint16 size) noexcept;

	//***************************************************************************
	// @brief ConnectEx로 비동기 연결을 게시합니다 (클라이언트 측 전용).
	// @param remoteAddr 접속할 원격 주소
	// @return bool "게시 시도"가 정상적으로 이뤄졌는지 여부입니다 — 연결 성공
	//         여부가 절대 아닙니다. 세 가지 경로를 구분해야 합니다:
	//         (1) 이 함수가 false를 반환 — 소켓이 무효하거나 bind() 자체가 실패한
	//             경우로, RegisterConnect()까지 가지도 못했지만 이 경우에도
	//             내부적으로 FailConnect()가 호출되어 OnDisconnected() 통지까지
	//             완료된 상태입니다(호출부가 추가로 정리할 것 없음).
	//         (2) 이 함수가 true를 반환했지만 이후 OnDisconnected()가 비동기로
	//             호출됨 — RegisterConnect() 내부에서 ConnectEx 게시 자체가
	//             즉시 실패했거나(WSA_IO_PENDING이 아닌 에러), 또는 게시는
	//             성공했으나 실제 TCP 연결이 실패한 경우(ProcessConnectEx()가
	//             getsockopt(SO_ERROR)로 검출).
	//         (3) 이 함수가 true를 반환하고 이후 OnConnected()가 비동기로 호출됨
	//             — 연결이 실제로 성공한 경우.
	//         즉 이 함수의 반환값만으로는 "연결 성공"을 절대 판단할 수 없고,
	//         호출부는 항상 OnConnected()/OnDisconnected() 오버라이드(또는 그에
	//         준하는 통지 메커니즘)로 최종 결과를 받아야 합니다.
	//***************************************************************************
	bool			ConnectAsync(const CNetAddress& remoteAddr);

protected:
	// 상위 콘텐츠 레이어(CGameSession 등)에서 오버라이딩할 가상 함수
	virtual void	OnConnected() {}
	virtual void	OnDisconnected() {}
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

	//***************************************************************************
	// @brief ConnectEx 비동기 연결을 실제로 게시합니다 (ConnectAsync() 내부에서 호출).
	//***************************************************************************
	void			RegisterConnect(const CNetAddress& remoteAddr);

	//***************************************************************************
	// @brief ConnectEx 완료 통지 처리 (Dispatch가 호출).
	// @details numOfBytes는 Connect 이벤트에서 성공/실패 구분에 쓸 수 없습니다
	//          (성공/실패 둘 다 0바이트로 통지됨 — Recv/Send와 달리 "0바이트=끊김"
	//          이라는 관례가 성립하지 않는 유일한 이벤트 타입). 대신
	//          getsockopt(SO_ERROR)로 실제 연결 성공 여부를 직접 검증합니다.
	//***************************************************************************
	void			ProcessConnectEx();

	//***************************************************************************
	// @brief connect 실패 시 정리 전용 경로.
	// @param reason 실패 사유
	// @details Disconnect(reason)을 재사용하지 않는 이유: 그 함수는
	//          `_connected.exchange(false) == false`면 즉시 return하는 가드가
	//          있는데, connect 실패 시점엔 _connected가 한 번도 true였던 적이
	//          없어(ProcessConnect()가 아직 호출되지 않음) 그 가드에 걸려 소켓도
	//          안 닫히고 OnDisconnected()도 호출되지 않은 채 소켓 핸들만 새는
	//          문제가 있었습니다. 이 함수는 _connected 상태와 무관하게 항상
	//          소켓을 닫고 통지 콜백을 호출합니다.
	//***************************************************************************
	void			FailConnect(Iocp::CloseReason reason);

	//***************************************************************************
	// @brief [수정 — outstanding recv/send 완료를 기다린 뒤에만 OnDisconnected() 통지]
	// @details 예전에는 ProcessDisconnect()(DisconnectEx 자신의 completion)가
	//          도착하는 즉시 OnDisconnected()를 통지했다. 그런데 그 시점에
	//          이미 게시돼 있던 WSARecv/WSASend가 다른 워커 스레드에서 "취소되지
	//          않고 실제 데이터와 함께" 완료될 수 있는 좁은 레이스가 있어서,
	//          "연결 끊김" 통지가 이미 나간 뒤에 OnRecv()가 뒤늦게 호출되는
	//          순서 역전이 가능했다(크래시는 아님 — CIocpEvent의 owner shared_ptr이
	//          객체 lifetime은 보장하므로 — 하지만 상위 프로토콜 레이어 입장에서는
	//          이미 죽었다고 통지받은 세션에서 데이터가 더 오는 논리적 모순).
	//
	//          이제 _pendingIoCount(outstanding recv+send 수)가 0이고
	//          _disconnectCompleted도 true일 때만(둘 다 만족해야 함 — 어느 쪽이
	//          나중에 만족되든 그쪽이 실제로 통지를 트리거함) OnDisconnected()를
	//          호출한다. _disconnectNotified로 이중 통지를 막는다.
	//
	//          [알려진 잔여 레이스 — 의도적으로 미해결] RegisterRecv()/RegisterSend()의
	//          "IsConnected() 체크 후 post" 사이의 극히 좁은 틈에 Disconnect()가
	//          끼어들면, 그 체크 통과 이후 실제 post(및 _pendingIoCount 증가)가
	//          ProcessDisconnect()의 "카운트 0 확인"보다 늦게 반영될 이론적
	//          가능성이 남아있다(개별 원자 변수들은 전부 seq_cst이지만, 서로
	//          다른 두 원자 변수에 걸친 이 특정 인과관계까지 강제하려면
	//          RegisterRecv/RegisterSend의 hot path에 락을 추가해야 해서 비용
	//          대비 실익이 낮다고 판단해 보류함). 이 경우도 그 post는 곧
	//          DisconnectEx에 의해 취소되어 aborted(0바이트)로 안전하게 완료되는
	//          게 거의 항상이라(위 연구에서 확인한 IOCP의 표준 동작), 실질적
	//          발생 확률은 극히 낮다.
	//***************************************************************************
	void			TryFinalizeDisconnect() noexcept;

private:
	uint64				_sessionId{ 0 };					// 고유 세션 ID
	SOCKET					_socket = INVALID_SOCKET;			// 통신에 사용되는 WinSock 소켓 핸들
	CNetAddress				_netAddress;						// 원격 클라이언트의 IP 주소 및 포트 정보

	std::atomic<bool>				_connected = false;							// 원자적(Atomic) 연산을 보장하는 세션 연결/해제 상태 플래그
	std::atomic<Iocp::CloseReason>	_closeReason{ Iocp::CloseReason::None };	// 세션 종료 사유 변수

	// [수정] TryFinalizeDisconnect() 관련 — OnDisconnected() 통지를 outstanding
	// recv/send 완료까지 지연시키기 위한 상태. 셋 다 seq_cst로만 접근한다
	// (TryFinalizeDisconnect()의 주석 참고 — 서로 다른 두 원자 변수에 걸친
	// "어느 쪽이 나중에 0/true가 되든 그쪽이 통지를 트리거한다"는 인과관계를
	// 보장하려면 개별 acquire/release 페어링보다 전역 순차 일관성이 더
	// 단순하고 안전함).
	std::atomic<int32> _pendingIoCount{ 0 };        // 현재 outstanding 상태인 WSARecv+WSASend 완료 대기 수
	std::atomic<bool> _disconnectCompleted{ false }; // DisconnectEx 자신의 completion이 이미 처리됐는지
	std::atomic<bool> _disconnectNotified{ false };  // OnDisconnected() 중복 통지 방지 1회성 CAS 가드

	std::mutex				_lock;                          // 송신 큐(_sendQueue) 스레드 동기화를 위한 뮤텍스
	CRingBuffer				_recvBuffer;                    // 제로카피 비동기 수신(WSARecv)을 관리하는 수신 링버퍼
	CVector<CSendBufferRef> _sendQueue;                     // 전송 대기 중인 패킷 참조(CSendBufferRef)들을 보관하는 송신 큐
	std::atomic<bool>		_sendRegistered = false;        // WSASend 비동기 요청 중복 호출을 방지하는 원자적 등록 상태 플래그

	RecvEvent				_recvEvent;                     // 비동기 수신(WSARecv) 요청 및 완료 처리를 위한 OVERLAPPED 이벤트 객체
	SendEvent				_sendEvent;                     // 비동기 송신(WSASend) 요청 및 완료 처리를 위한 OVERLAPPED 이벤트 객체
	DisconnectEvent			_disconnectEvent;               // 비동기 해제(DisconnectEx) 요청 및 완료 처리를 위한 OVERLAPPED 이벤트 객체
	ConnectEvent			_connectEvent;                   // 비동기 연결(ConnectEx) 요청 및 완료 처리를 위한 OVERLAPPED 이벤트 객체 (클라이언트 전용)
};

#endif // ndef UC_IOCPSESSION_H