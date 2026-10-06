
//***************************************************************************
// IocpSession.h : interface for the CIocpSession class.
//
//***************************************************************************

#ifndef UC_IOCPSESSION_H
#define UC_IOCPSESSION_H

#include <Network/IOCP/IocpCore.h>
#include <Network/IOCP/IocpEvent.h>
#include <Network/RingBuffer.h>
#include <Network/SocketUtils.h>
#include <Util/ScopeExit.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <limits>

//***************************************************************************
// @class CIocpSession
// @brief CSession을 상속받는 IOCP 네트워크 통신의 핵심인 연결 세션 추상 기반 클래스.
// @details
// 역할:
//      1. 소켓 관리, WSARecv/WSASend/DisconnectEx 호출 및 완료 이벤트 처리
//      2. Scatter-Gather Send 지원 (큐에 쌓인 여러 패킷을 1회 WSASend로 일괄 전송).
//         Send() 한 번의 데이터가 SendBuffer 청크(Iocp::SEND_BUFFER_CHUNK_SIZE)보다
//         크면 여러 SendBuffer로 나눠 하나의 단위로 큐에 넣는다(최대 uint16 = 65535바이트).
//         송신 큐 누적 상한은 SetMaxSendQueueBytes()로 선택 지정한다(기본 0 = 무제한).
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
	// @brief 상대가 연결을 정상 종료(FIN)해서 세션이 종료됐는지 반환합니다.
	// @details 수신 완료가 오류 없이 0바이트로 끝난 경우에만 true이며, 리셋/소켓 오류/로컬 종료는 false입니다.
	//          상위 레이어가 "연결 종료로 끝나는 데이터"(HTTP/1.0식 본문 등)의 끝과 오류로 인한 끊김을
	//          구분할 때 OnDisconnected() 안에서 사용합니다.
	//***************************************************************************
	bool WasGracefulClose() const noexcept { return _gracefulClose.load(std::memory_order_acquire); }

	//***************************************************************************
	// @brief 연결이 완료될 때(ProcessConnect, OnConnected() 직전) 소켓에 적용할 TCP 옵션.
	// @details 기본값은 모두 꺼짐(기존 동작). 서버 서비스는 SetSessionSocketOptions()로 접속하는 모든 세션에,
	//          세션 서브클래스는 생성자나 OnConnected() 이전에 SetSocketOptions()로 자기 세션에 지정할 수 있다.
	//          - noDelay: TCP_NODELAY(Nagle 끔). 작은 패킷을 연달아 보내는 채팅/게임처럼 지연에 민감한 서비스용.
	//          - keepAlive: SO_KEEPALIVE. FIN/RST 없이 조용히 끊긴 연결을 유휴 중에도 감지한다.
	//***************************************************************************
	struct SocketOptions
	{
		bool	noDelay = false;
		bool	keepAlive = false;
		uint32	keepAliveIdleMs = 30000;		// 마지막 데이터 이후 첫 keep-alive 확인까지 (keepAlive가 true일 때만 사용)
		uint32	keepAliveIntervalMs = 10000;	// keep-alive 확인 간격
	};

	void			SetSocketOptions(const SocketOptions& options) noexcept { _socketOptions = options; }
	SocketOptions	GetSocketOptions() const noexcept { return _socketOptions; }

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

	//***************************************************************************
	// @brief 바이트 데이터를 송신 큐에 넣고 전송을 요청합니다 (Thread-safe).
	// @param data 전송할 데이터 포인터
	// @param size 전송할 바이트 수 (1 ~ 65535)
	// @return bool 큐잉 성공 여부. 연결되지 않았거나 인자가 잘못된 경우, SendBuffer를
	//         확보하지 못한 경우, 송신 큐 상한을 넘겨 연결이 종료된 경우 false.
	// @details size가 Iocp::SEND_BUFFER_CHUNK_SIZE보다 크면 여러 SendBuffer로 나눠
	//          한 번의 락 구간에서 한꺼번에 큐에 넣으므로, 여러 스레드가 동시에 Send()해도
	//          한 호출의 조각들이 다른 호출의 데이터와 섞이지 않습니다.
	//***************************************************************************
	bool			Send(const void* data, uint16 size) noexcept;

	//***************************************************************************
	// @brief Send() 한 번이 만드는 SendBuffer 묶음.
	// @details 같은 데이터를 여러 세션에 보낼 때(Broadcast) PrepareSend()로 한 번만 만들고
	//          SendPrepared()로 세션마다 공유 전송한다. SendBuffer는 Close() 이후 읽기 전용이라
	//          여러 세션의 송신 큐가 같은 버퍼를 동시에 참조해도 안전하며, 마지막 세션의 전송이
	//          끝나 참조가 모두 풀려야 청크가 풀로 반환된다.
	//***************************************************************************
	struct SendPieces
	{
		CSendBufferRef	buffers[Iocp::kMaxSendPieces];
		size_t			count = 0;
		uint64			totalBytes = 0;
	};

	//***************************************************************************
	// @brief 데이터를 SendBuffer들로 복사해 SendPieces를 만듭니다 (호출 스레드의 thread_local 청크 사용).
	// @param data 전송할 데이터 포인터
	// @param size 전송할 바이트 수 (1 ~ 65535)
	// @param out 결과를 받을 SendPieces
	// @return bool 성공 여부 (인자가 잘못됐거나 SendBuffer를 확보하지 못하면 false)
	//***************************************************************************
	static bool		PrepareSend(const void* data, uint16 size, SendPieces& out) noexcept;

	//***************************************************************************
	// @brief PrepareSend()로 만든 SendBuffer 묶음을 이 세션의 송신 큐에 넣습니다 (Thread-safe).
	// @param pieces 전송할 묶음 (호출 후에도 그대로 유지되어 다른 세션에 다시 쓸 수 있다)
	// @return bool 큐잉 성공 여부. Send()와 같은 조건에서 false.
	//***************************************************************************
	bool			SendPrepared(const SendPieces& pieces) noexcept;

	//***************************************************************************
	// @brief 송신 큐(아직 WSASend에 넘기지 않은 데이터)의 누적 바이트 상한을 설정합니다.
	// @param maxBytes 상한 바이트 수. 0이면 무제한(기본값).
	// @details 상한을 넘기게 되는 Send()는 데이터를 큐에 넣지 않고
	//          Disconnect(Iocp::CloseReason::SendBufferOverflow)를 호출한 뒤 false를 반환합니다.
	//          대용량 body를 큐잉하는 HTTP 클라이언트 세션 등은 무제한(0)으로 두고,
	//          수신 속도가 느린 클라이언트로부터 서버 메모리를 보호해야 하는
	//          게임/채팅 세션에서만 지정하는 용도입니다. 이미 WSASend에 넘어간
	//          in-flight 데이터는 이 상한에 포함되지 않습니다.
	//***************************************************************************
	void			SetMaxSendQueueBytes(uint64 maxBytes) noexcept { _maxSendQueueBytes.store(maxBytes, std::memory_order_relaxed); }
	uint64			GetMaxSendQueueBytes() const noexcept { return _maxSendQueueBytes.load(std::memory_order_relaxed); }

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
	//***************************************************************************
	// @brief 이미 Close()된 SendBuffer들을 하나의 단위로 송신 큐에 넣고, 진행 중인
	//        WSASend가 없으면 RegisterSend()를 시작합니다.
	// @param buffers SendBuffer 배열 (큐로 이동됨)
	// @param count 배열 원소 수
	// @param totalBytes 배열이 담고 있는 데이터의 총 바이트 수
	// @return bool 큐잉 성공 여부 (미연결 또는 상한 초과 시 false)
	//***************************************************************************
	bool			EnqueueSend(CSendBufferRef* buffers, size_t count, uint64 totalBytes);

	void			ApplySocketOptions() noexcept;	// _socketOptions를 소켓에 적용 (ProcessConnect()에서 OnConnected() 직전에 호출)

	// 이 completion 하나에 대응하는 outstanding I/O 카운트를 내리고 종료 통지 조건을 재확인합니다.
	void			ReleaseIo() noexcept;

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
	// @details _connected 상태와 무관하게 소켓을 닫고 OnDisconnected() 통지를 완료합니다.
	//          Disconnect(Iocp::CloseReason)의 "미연결 상태 강제 종료" 경로와
	//          _disconnectNotified CAS 가드를 공유하므로 어느 쪽이 먼저 실행되든 통지는
	//          1회만 나갑니다(취소된 ConnectEx의 완료가 뒤늦게 도착해도 중복 통지 없음).
	//***************************************************************************
	void			FailConnect(Iocp::CloseReason reason);

	//***************************************************************************
	// @brief outstanding recv/send가 모두 완료되고 DisconnectEx 자신의 completion도
	//        처리됐을 때에만 OnDisconnected()를 1회 통지합니다.
	// @details 조건은 _pendingIoCount == 0 && _disconnectCompleted 입니다. 어느 쪽이 나중에
	//          만족되든(마지막 recv/send completion, 또는 DisconnectEx completion) 그쪽 호출부가
	//          이 함수를 통해 통지를 트리거하며, _disconnectNotified CAS로 이중 통지를 막습니다.
	//          세 상태 변수는 seq_cst로만 접근합니다(서로 다른 두 원자 변수에 걸친
	//          "나중에 만족시킨 쪽이 통지한다"는 인과관계를 단순하게 보장하기 위함).
	//
	//          [Process* 와의 순서] ProcessRecv/ProcessSend는 자신의 completion에 대응하는
	//          _pendingIoCount 감소와 이 함수 호출을 함수가 끝날 때(OnRecv()/OnSend()와 다음 I/O
	//          게시까지 마친 뒤) 수행합니다. 따라서 OnRecv()/OnSend()가 실행되는 동안에는 다른
	//          스레드가 OnDisconnected()를 통지하지 못하고, 통지 이후에는 이 세션의 어떤
	//          Process*도 콜백을 호출하지 않습니다(종료가 시작된 세션, 즉 IsConnected() == false이면
	//          콜백 자체를 건너뜁니다). 이 보장의 예외는 Disconnect()의 강제 정리 경로입니다 —
	//          미연결 세션, CNetService::Close(), Register* 실패 롤백, 사용자 코드의 중복
	//          Disconnect() 호출은 이 카운트를 기다리지 않고 OnDisconnected()를 통지합니다
	//          (연결 상태를 명시적 상태 머신으로 분리하는 개선은 보류).
	//
	//          [알려진 잔여 레이스 — 의도적으로 미해결] RegisterRecv()/RegisterSend()의
	//          "IsConnected() 체크 후 post" 사이의 극히 좁은 틈에 Disconnect()가 끼어들면,
	//          실제 post(및 _pendingIoCount 증가)가 ProcessDisconnect()의 "카운트 0 확인"보다
	//          늦게 반영될 이론적 가능성이 남아있다. 이를 강제하려면 RegisterRecv/RegisterSend의
	//          hot path에 락을 추가해야 해서 비용 대비 실익이 낮다고 보아 보류했다. 이 경우에도
	//          그 post는 곧 DisconnectEx에 의해 취소되어 aborted(0바이트)로 완료되는 것이 일반적이다.
	//***************************************************************************
	void			TryFinalizeDisconnect() noexcept;

private:
	uint64					_sessionId{ 0 };					// 고유 세션 ID
	SOCKET					_socket = INVALID_SOCKET;			// 통신에 사용되는 WinSock 소켓 핸들
	CNetAddress				_netAddress;						// 원격 클라이언트의 IP 주소 및 포트 정보

	std::atomic<bool>				_connected = false;							// 원자적(Atomic) 연산을 보장하는 세션 연결/해제 상태 플래그
	std::atomic<Iocp::CloseReason>	_closeReason{ Iocp::CloseReason::None };	// 세션 종료 사유 변수
	SocketOptions					_socketOptions;								// 연결 완료 시 적용할 TCP 옵션 (SetSocketOptions() 참고)
	std::atomic<bool>				_gracefulClose = false;						// 상대의 정상 종료(FIN)로 끝났는지 (WasGracefulClose() 참고)

	// TryFinalizeDisconnect() 관련 — OnDisconnected() 통지를 outstanding
	// recv/send 완료까지 지연시키기 위한 상태. 셋 다 seq_cst로만 접근한다
	// (TryFinalizeDisconnect()의 주석 참고 — 서로 다른 두 원자 변수에 걸친
	// "어느 쪽이 나중에 0/true가 되든 그쪽이 통지를 트리거한다"는 인과관계를
	// 보장하려면 개별 acquire/release 페어링보다 전역 순차 일관성이 더
	// 단순하고 안전함).
	std::atomic<int32> _pendingIoCount{ 0 };				// 현재 outstanding 상태인 WSARecv+WSASend 완료 대기 수
	std::atomic<bool> _disconnectCompleted{ false };		// DisconnectEx 자신의 completion이 이미 처리됐는지
	std::atomic<bool> _disconnectNotified{ false };			// OnDisconnected() 중복 통지 방지 1회성 CAS 가드

	std::mutex				_lock;                          // 송신 큐(_sendQueue) 스레드 동기화를 위한 뮤텍스
	CRingBuffer				_recvBuffer;                    // 제로카피 비동기 수신(WSARecv)을 관리하는 수신 링버퍼
	CVector<CSendBufferRef> _sendQueue;                     // 전송 대기 중인 패킷 참조(CSendBufferRef)들을 보관하는 송신 큐
	uint64					_sendQueueBytes = 0;            // _sendQueue에 쌓인 데이터의 누적 바이트 수 (_lock으로 보호)
	std::atomic<uint64>		_maxSendQueueBytes{ 0 };         // 송신 큐 누적 상한 (0 = 무제한)
	std::atomic<bool>		_sendRegistered = false;        // WSASend 비동기 요청 중복 호출을 방지하는 원자적 등록 상태 플래그

	RecvEvent				_recvEvent;                     // 비동기 수신(WSARecv) 요청 및 완료 처리를 위한 OVERLAPPED 이벤트 객체
	SendEvent				_sendEvent;                     // 비동기 송신(WSASend) 요청 및 완료 처리를 위한 OVERLAPPED 이벤트 객체 (sendBuffers/wsaBufs 보유)
	DisconnectEvent			_disconnectEvent;               // 비동기 해제(DisconnectEx) 요청 및 완료 처리를 위한 OVERLAPPED 이벤트 객체
	ConnectEvent			_connectEvent;                  // 비동기 연결(ConnectEx) 요청 및 완료 처리를 위한 OVERLAPPED 이벤트 객체 (클라이언트 전용)
};

#endif // ndef UC_IOCPSESSION_H