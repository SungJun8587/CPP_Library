
//***************************************************************************
// RioSession.h : interface for the CRioSession class.
//
//***************************************************************************

#ifndef UC_RIOSESSION_H
#define UC_RIOSESSION_H

#include <Network/RIO/RioCommon.h>
#include <Network/RIO/RioObject.h>
#include <Network/RIO/RioEvent.h>
#include <Network/RIO/RioSend.h>
#include <Network/RIO/RioReceive.h>
#include <Network/RIO/RioConnectEvent.h>
#include <Thread/PlatformLock.h>

#include <atomic>
#include <memory>
#include <cstdint>
#include <deque>
#include <vector>
#include <mutex>

class CRioCore;
class CRioBuffer;
class CRioConnectDispatcher;

//***************************************************************************
// @class CRioSession
// @brief RIO(Registered I/O) 및 CRingBuffer 기반의 고성능 네트워크 세션 클래스 (순수 I/O 전용)
//
// @details
//      [송신 버퍼(_sendBuffer)와 RIO 등록의 관계]
//          _sendBuffer는 세션마다 독자적으로 할당되는 메모리(CRingBuffer 생성자에서
//          RawAllocator::Alloc())입니다. RIO는 "등록된(registered) 버퍼" 내의 오프셋만
//          참조할 수 있으므로, _sendBuffer를 RIO_BUF로 넘기려면 이 세션의 _sendBuffer
//          메모리 자체를 RIORegisterBuffer()로 등록해서 얻은 RIO_BUFFERID를 써야 합니다.
//          서버/클라이언트 전역 공용 송신 버퍼(RioService.cpp)에서 발급받은
//          RIO_BUFFERID를 그대로 쓰면 BufferId가 가리키는 영역과 Offset 계산 기준이
//          서로 다른 메모리가 되어 RIOSendEx()가 실패합니다(과거에 실제로 겪은 문제).
//          그래서 Init()에서 이 세션 자신이 _sendBuffer를 등록하고, FinalizeClose()/
//          소멸자에서 해제합니다.
//
//      [송신 흐름 제어 — _sendOverflowQueue]
//          _sendBuffer는 고정 64KB(정확히 65536바이트)라, 큰 body를 보내는 등
//          누적 송신량이 이를 넘으면 예전에는 Send()가 즉시 Close(SendBufferOverflow)로
//          연결을 끊었습니다. 지금은 그 대신 링버퍼가 꽉 찬 만큼을 _sendOverflowQueue
//          (청크 단위 std::deque)에 임시 보관했다가, OnSendCompleted()가 완료
//          통지를 받아 읽기 커서를 옮길 때마다 생기는 여유 공간만큼
//          DrainOverflowIntoSendBufferLocked()로 이어서 채워 넣습니다 — IOCP
//          세션(CVector<CSendBufferRef> 큐, 오브젝트 풀 기반이라 사실상 무제한
//          큐잉)과 "초과분은 메모리에 버퍼링" 동작을 일관되게 맞춘 것입니다.
//          진행 정지(deadlock) 걱정이 없는 이유: 청크 하나는 Send()의 uint16
//          제약상 최대 65535바이트이고 _sendBuffer 용량은 정확히 65536바이트라,
//          링버퍼가 완전히 비면 오버플로 큐의 다음 청크는 반드시 들어갈 수
//          있습니다. 다만 매우 큰 body(예: 수십 MB 업로드)를 CHttpClientCore::
//          BeginRequest()가 한 번에 다 쪼개서 던지면, 그 전량이 실제 전송
//          완료될 때까지 이 큐에 임시로 쌓여 있을 수 있다는 점은 감안해야
//          합니다(요청 크기에 비례하는 일시적 메모리 사용 — 무한 누수 아님).
//
//      [멀티 워커 스레드 안전성]
//          CRioCore가 멀티 워커로 동작해도, 이 클래스는 별도 수정 없이 안전합니다.
//          _ioSubmitLock이 receive/send 제출과 Close/FinalizeClose 간 상호 배제를,
//          _sendLock이 송신 링버퍼 접근을 보호합니다. 세션당 receive는 항상 1개만
//          in-flight이므로 OnReceiveCompleted()끼리는 서로 동시 실행되지 않지만,
//          다른 워커가 처리하는 OnSendCompleted()와는 동시에 실행될 수 있습니다 —
//          다만 그 경우도 위 두 락으로 이미 안전합니다. 단, OnDataReceived()를
//          구현하는 상위 클래스가 세션 밖의 공유 가변 상태(예: 다른 세션과 공유하는
//          게임 로직 상태)를 건드리는 경우엔 그 상태 자체를 상위 계층에서 별도로
//          동기화해야 합니다(이 클래스가 보장하는 범위 밖).
//
//      [클라이언트 전용 비동기 연결(ConnectAsync)]
//          RIO CQ 완료(RIODequeueCompletion 기반)와 ConnectEx 완료(일반 OVERLAPPED/
//          IOCP 기반)는 완전히 다른 통로입니다 — CRioCore::DispatchBatch()에
//          ConnectEx 완료를 섞으면 IsValidCompletionPacket() 검증에 걸려 CRioCore
//          전체가 Faulted로 죽습니다(진단 이력 참고). 그래서 ConnectAsync()는
//          CRioCore와 무관한 별도의 CRioConnectDispatcher(전용 IOCP + 워커 스레드 1개)를
//          통해 완료 통지를 받고, ProcessConnectEx()가 그 완료를 받아 이어서
//          RIOCreateRequestQueue() -> Init() -> PostInitialReceive() 순서로 세션을
//          완성시킵니다. 이 흐름은 호출자(CRioClientService::ConnectOneMoreSession())가
//          오케스트레이션합니다.
//***************************************************************************
class CRioSession : public CSession, public CRioObject
{
public:
	CRioSession();
	virtual ~CRioSession() noexcept override;

	CRioSession(const CRioSession&) = delete;
	CRioSession& operator=(const CRioSession&) = delete;

	// CRioObject의 순수 가상 함수 구현
	virtual CRioObjectRef GetRioObjectPtr() override
	{
		// CSession의 shared_from_this()를 CRioObject* 타입의 shared_ptr로 안전하게 변환
		return CRioObjectRef(shared_from_this(), static_cast<CRioObject*>(this));
	}

public:
	// CRioObject 인터페이스 구현
	virtual void Dispatch(CRioEvent* rioEvent, ULONG bytesTransferred, LONG status) override;

private:
	//***************************************************************************
	// @brief CRioObject::OnIoCountReachedZero() 오버라이드. 이 세션의 outstanding
	//        I/O 카운트가 0이 될 때마다(정상 동작 중이든 종료 중이든) 호출됩니다.
	// @details Closing 상태일 때만 의미가 있습니다 — 그 경우 FinalizeClose()를
	//          대신 호출합니다. Active 상태에서 우연히 카운트가 0을 지나가는
	//          경우(예: 마지막 receive completion 처리 후 다음 PostReceiveInternal()
	//          이 아직 다시 카운트를 올리기 전인 찰나)는 아무 의미가 없으므로
	//          무시합니다. Close()의 "outstanding이 있으면 여기서 마무리해줄
	//          것을 기대하고 아무것도 안 함" 경로와 정확히 짝을 이룹니다 —
	//          자세한 설계 배경은 Close()의 주석 참고.
	//***************************************************************************
	virtual void OnIoCountReachedZero() noexcept override;

public:
	// CSession 공통 인터페이스 오버라이드
	virtual void	Disconnect(const TCHAR* cause) override;
	virtual bool	IsConnected() const override { return IsActive(); }
	virtual SOCKET	GetSocket() const noexcept override { return _socket.load(std::memory_order_acquire); }

	//***************************************************************************
	// @brief 세션을 초기화하고 이 세션 소유의 송신 버퍼를 RIO에 등록합니다.
	// @param sessionId 고유 세션 ID
	// @param core RIO Core 객체 포인터
	// @param globalRecvBufferPool 전역 수신 버퍼 풀 포인터
	// @param socket 클라이언트 소켓 핸들
	// @param requestQueue RIO Request Queue 핸들
	// @return bool 초기화(및 송신 버퍼 등록) 성공 시 true.
	//         false를 반환하면 세션은 Active로 전이하지 않으며, 호출자가
	//         clientSocket/requestQueue를 직접 정리해야 합니다(세션이 아직
	//         Active가 아니므로 Close()로 자기 자신을 정리시킬 수 없음).
	//***************************************************************************
	bool Init(uint64 sessionId, CRioCore* core, CRioBuffer* globalRecvBufferPool, SOCKET socket, RIO_RQ requestQueue) noexcept;

	//***************************************************************************
	// @brief ConnectEx로 비동기 연결을 게시합니다 (클라이언트 측 전용).
	// @param dispatcher 완료 통지를 받을 전용 디스패처(CRioCore와 무관, 호출자가
	//        소유하며 이 세션 수명보다 오래 살아있어야 함 — 보통 서비스가 소유)
	// @param sessionId 고유 세션 ID (연결 성공 시 Init()에 그대로 전달됨)
	// @param core RIO Core 객체 포인터 (연결 성공 시 RIOCreateRequestQueue와 Init에 사용)
	// @param globalRecvBufferPool 전역 수신 버퍼 풀 포인터
	// @param remoteAddr 접속할 원격 주소
	// @return bool "게시 시도"가 정상적으로 이뤄졌는지 여부입니다 — 연결 성공
	//         여부가 절대 아닙니다(IOCP의 CIocpSession::ConnectAsync()와 동일한
	//         계약). 이 함수의 모든 실패 경로는 FailConnect()를 호출해
	//         OnDisconnected()까지 통지를 완료하므로, 호출부는 반환값이 false여도
	//         별도로 정리할 것이 없습니다. 최종 연결 성공/실패는 항상
	//         OnConnected()/OnDisconnected(reason) 오버라이드로 비동기 통지됩니다.
	//***************************************************************************
	bool ConnectAsync(CRioConnectDispatcher& dispatcher, uint64 sessionId, CRioCore* core,
		CRioBuffer* globalRecvBufferPool, const CNetAddress& remoteAddr);

	//***************************************************************************
	// @brief ConnectEx 완료 통지 처리. CRioConnectDispatcher의 워커 스레드가 호출합니다.
	// @details 일반 사용자 코드에서 직접 호출할 일은 없지만, CRioConnectDispatcher가
	//          CRioSession의 내부 구현 세부사항(RIOCreateRequestQueue 순서 등)을
	//          몰라도 되도록 이 함수만 public으로 노출합니다.
	//***************************************************************************
	void ProcessConnectEx();

	//***************************************************************************
	// @brief 지정된 사유로 세션 종료를 요청합니다.
	// @param reason 세션 종료 사유
	//***************************************************************************
	void Close(Rio::CloseReason reason) noexcept;

	//***************************************************************************
	// @brief 강제 종료 사유로 세션을 종료합니다.
	//***************************************************************************
	void Close() noexcept { Close(Rio::CloseReason::ForcedClose); }

	//***************************************************************************
	// @brief 최초 비동기 수신(Receive) 요청을 게시합니다.
	// @return 게시 성공 시 true, 실패 시 false
	//***************************************************************************
	bool PostInitialReceive() noexcept;

	//***************************************************************************
	// @brief 데이터를 송신 버퍼에 큐잉하고 RIO 전송을 진행합니다.
	// @param data 전송할 데이터 버퍼 포인터
	// @param size 전송할 데이터 크기 (바이트)
	// @return 전송 큐잉 및 처리 성공 시 true, 실패 시 false
	//***************************************************************************
	bool Send(const void* data, uint16 size) noexcept;

	//***************************************************************************
	// @brief 소켓 핸들이 유효한 상태인지 확인합니다.
	// @return bool _socket이 INVALID_SOCKET이 아니면 true, 아니면 false
	//***************************************************************************
	bool IsValid() const noexcept { return GetSocket() != INVALID_SOCKET; }

	//***************************************************************************
	// @brief 원격 클라이언트의 네트워크 주소(IP/Port)를 설정합니다.
	// @param netAddr 설정할 CNetAddress 객체
	//***************************************************************************
	void SetNetAddress(CNetAddress netAddr) { _netAddress = netAddr; }

	//***************************************************************************
	// @brief 원격 클라이언트의 네트워크 주소(IP/Port)를 반환합니다.
	// @return CNetAddress 네트워크 주소 객체
	//***************************************************************************
	CNetAddress GetNetAddress() const { return _netAddress; }

	//***************************************************************************
	// @brief 세션 종료 사유를 반환합니다.
	// @return CloseReason 세션 종료 사유
	//***************************************************************************
	Rio::CloseReason GetCloseReason() const noexcept { return _closeReason.load(std::memory_order_acquire); }

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

	//***************************************************************************
	// @brief RIO Request Queue 핸들을 반환합니다.
	// @return RIO_RQ RIO Request Queue 핸들
	//***************************************************************************
	RIO_RQ GetRequestQueue() const noexcept { return _requestQueue.load(std::memory_order_acquire); }

	//***************************************************************************
	// @brief RIO Core 엔진 객체 포인터를 반환합니다.
	// @return CRioCore* RIO Core 객체 포인터
	//***************************************************************************
	CRioCore* GetCore() const noexcept { return _core; }

	//***************************************************************************
	// @brief 세션 객체가 생성되어 메모리에 할당되었으나 아직 활성화되지 않은 상태인지 여부를 반환합니다.
	// @return bool 생성 상태인 경우 true, 아니면 false
	//***************************************************************************
	bool IsCreated() const noexcept { return _state.load(std::memory_order_acquire) == Rio::SessionState::Created; }

	//***************************************************************************
	// @brief 세션 연결이 활성화되어 I/O 요청 및 송수신 처리가 가능한 상태인지 여부를 반환합니다.
	// @return bool 활성 상태인 경우 true, 아니면 false
	//***************************************************************************
	bool IsActive() const noexcept { return _state.load(std::memory_order_acquire) == Rio::SessionState::Active; }

	//***************************************************************************
	// @brief 세션이 종료 진행 중인지 여부를 반환합니다.
	// @return bool 종료 진행 중인 경우 true, 아니면 false
	//***************************************************************************
	bool IsClosing() const noexcept { return _state.load(std::memory_order_acquire) == Rio::SessionState::Closing; }

	//***************************************************************************
	// @brief 세션이 완전히 닫혔는지 여부를 반환합니다.
	// @return bool 닫힌 상태인 경우 true, 아니면 false
	//***************************************************************************
	bool IsClosed() const noexcept { return _state.load(std::memory_order_acquire) == Rio::SessionState::Closed; }

	//***************************************************************************
	// @brief 수신 링버퍼 참조를 반환합니다 (상위 클래스에서 패킷 파싱 시 사용).
	// @return CRingBuffer& 수신 링버퍼
	//***************************************************************************
	CRingBuffer& GetRecvBuffer() noexcept { return _recvBuffer; }

protected:
	virtual void OnConnected() {}
	virtual void OnDisconnected(Rio::CloseReason reason) {}

	// 패킷 파싱 책임은 상위 클래스로 위임
	virtual void OnDataReceived() = 0;

private:
	//***************************************************************************
	// @brief 세션의 모든 리소스(RIO_RQ, 소켓, 송신 버퍼 등록 등)를 안전하게 해제하고
	//        연결 해제 콜백을 호출합니다.
	//***************************************************************************
	void FinalizeClose() noexcept;

	//***************************************************************************
	// @brief 내부 RIO 비동기 수신 요청을 게시합니다.
	// @return 게시 성공 시 true, 실패 시 false
	//***************************************************************************
	bool PostReceiveInternal() noexcept;

	//***************************************************************************
	// @brief 송신 링버퍼 데이터를 가져와 RIO 전송을 요청합니다.
	// @return 전송 요청 성공 시 true, 실패 시 false
	//***************************************************************************
	bool FlushSendInternal() noexcept;

	//***************************************************************************
	// @brief 오버플로 큐(_sendOverflowQueue)에 쌓인 청크를 _sendBuffer에 여유
	//        공간이 생긴 만큼 옮겨 담습니다.
	// @details _sendLock을 write로 보유한 상태에서만 호출해야 합니다(호출부:
	//          OnSendCompleted(), MoveReadBuffer() 직후). 청크 하나를 통째로
	//          넣을 공간이 없으면 그 청크는 큐에 그대로 남겨두고 중단합니다
	//          (Enqueue(..., exact=false)의 "전량 아니면 실패" 시맨틱과
	//          일관성을 맞추기 위해 청크를 쪼개서 일부만 옮기지 않음). 청크는
	//          항상 65535바이트 이하(Send()의 uint16 제약)이고 _sendBuffer
	//          용량은 정확히 65536바이트라, 링버퍼가 완전히 빈 상태라면 다음
	//          청크 하나는 반드시 들어갈 수 있음이 보장됩니다 — 즉 이 드레인이
	//          영원히 진행 못 하고 멈추는 경우는 없습니다.
	//***************************************************************************
	void DrainOverflowIntoSendBufferLocked() noexcept;

	void ShutdownSocketInternal() noexcept;
	void CloseSocketInternal() noexcept;

	//***************************************************************************
	// @brief 이 세션 소유의 _sendBuffer 메모리를 RIORegisterBuffer()로 등록합니다.
	// @details Init() 1회 호출에서만 실질적으로 등록이 일어납니다(세션은 재사용되지
	//          않으므로 _sendBufferId가 이미 유효하면 그대로 true 반환).
	// @return 이미 등록됐거나 새로 등록 성공 시 true, 실패 시 false
	//***************************************************************************
	bool RegisterSendBufferIfNeeded() noexcept;

	//***************************************************************************
	// @brief 등록했던 _sendBuffer의 RIO 버퍼 ID를 RIODeregisterBuffer()로 해제합니다.
	// @details 여러 번 호출해도 안전합니다(idempotent) — _sendBufferId를
	//          RIO_INVALID_BUFFERID로 되돌리므로 두 번째 호출은 즉시 반환됩니다.
	//***************************************************************************
	void UnregisterSendBuffer() noexcept;

	//***************************************************************************
	// @brief 수신 완료 비동기 이벤트를 처리합니다.
	// @param rioEvent 완료된 RIO 이벤트 포인터
	// @param bytesTransferred 수신된 바이트 수
	//***************************************************************************
	void OnReceiveCompleted(CRioEvent* rioEvent, DWORD bytesTransferred) noexcept;

	//***************************************************************************
	// @brief 송신 완료 비동기 이벤트를 처리합니다.
	// @param rioEvent 완료된 송신 RIO 이벤트 포인터
	// @param bytesTransferred 전송된 바이트 수
	//***************************************************************************
	void OnSendCompleted(CRioEvent* rioEvent, DWORD bytesTransferred) noexcept;

	//***************************************************************************
	// @brief ConnectEx 비동기 연결을 실제로 게시합니다 (ConnectAsync() 내부에서 호출).
	//***************************************************************************
	void RegisterConnect(const CNetAddress& remoteAddr);

	//***************************************************************************
	// @brief connect 실패(또는 그 이전 단계인 소켓 생성/bind/RQ 생성 실패) 시
	//        정리 전용 경로.
	// @param reason 실패 사유
	// @details Close(reason)을 재사용하지 않는 이유: 그 함수는 Active 상태에서
	//          Closing으로의 CAS 전이를 전제로 하는데, connect 실패 시점엔
	//          _state가 한 번도 Active였던 적이 없어(Init()이 아직 성공적으로
	//          끝나지 않음) 그 전이가 실패해 조용히 return하고 아무 정리도 안
	//          됩니다(소켓 leak). 이 함수는 상태와 무관하게 항상 소켓을 닫고
	//          통지 콜백을 호출한 뒤 _state를 직접 Closed로 확정합니다(Active->
	//          Closing->Closed의 정상 파이프라인을 거치지 않는 예외 경로임을
	//          명시적으로 표시).
	//***************************************************************************
	void FailConnect(Rio::CloseReason reason) noexcept;

private:
	uint64 _sessionId{ 0 };                           // 고유 세션 ID

	CRioCore* _core{ nullptr };                         // RIO Core 객체 포인터
	CRioBuffer* _globalRecvBufferPool{ nullptr };       // 전역 수신 버퍼 풀 포인터

	// Close/Finalize와 RIO submit 사이의 concurrent read/write 보호
	std::atomic<SOCKET> _socket{ INVALID_SOCKET };      // 세션 바인딩 소켓
	std::atomic<RIO_RQ> _requestQueue{ RIO_INVALID_RQ }; // RIO Request Queue 핸들

	// _sendBuffer(아래) 메모리를 RIORegisterBuffer()로 등록한 결과 ID.
	// Init()에서 등록, FinalizeClose()/소멸자에서 해제. 외부(RioService 등)에서
	// 주입받지 않고 이 세션이 직접 소유/관리합니다 — 서버 전역 공용 버퍼 ID를
	// 쓰면 _sendBuffer의 실제 메모리 영역과 불일치해 RIOSendEx()가 실패합니다.
	RIO_BUFFERID _sendBufferId{ RIO_INVALID_BUFFERID };

	CNetAddress _netAddress;                            // 원격 클라이언트 네트워크 주소 (IP/Port)

	// I/O submit과 Close/Finalize의 상호 배제를 위한 게이트
	mutable PLock _ioSubmitLock;                        // I/O 제출 및 상태 전이 동기화 게이트 락

	std::atomic<Rio::SessionState> _state{ Rio::SessionState::Created };        // 세션 상태 머신 변수
	std::atomic<Rio::CloseReason> _closeReason{ Rio::CloseReason::None };       // 세션 종료 사유

	PRWLock _sendLock;                                  // 송신 동기화 RW 락
	bool _isSending{ false };                           // 전송 진행 여부 플래그

	// _sendBuffer(64KB)가 꽉 찼을 때 Close()로 연결을 죽이지 않고 여기 임시
	// 보관했다가, OnSendCompleted()가 공간을 비울 때마다
	// DrainOverflowIntoSendBufferLocked()로 이어서 채워 넣는다. 청크 단위(각
	// Send() 호출 1회 = 최대 65535바이트)로 보관하며, _sendLock(write)으로
	// 함께 보호한다(별도 락을 두지 않고 기존 _sendLock에 편입 — 오버플로
	// 큐도 결국 _sendBuffer와 같은 "송신 대기열"의 연장이라 별도 락으로
	// 나누면 두 락 사이의 원자성을 새로 신경 써야 해서 오히려 복잡해짐).
	std::deque<std::vector<char>> _sendOverflowQueue;

	CRingBuffer _sendBuffer{ Rio::kSendRingBufferSize };    // 64KB 송신 링버퍼 (세션 독자 소유 메모리, 꽉 차면 초과분은 _sendOverflowQueue로)
	CRingBuffer _recvBuffer{ Rio::kRecvRingBufferSize };    // 64KB 수신 링버퍼

	RioConnectEvent _connectEvent;                      // ConnectEx 요청 및 완료 처리를 위한 OVERLAPPED 이벤트 객체 (클라이언트 전용)
};

#endif // ndef UC_RIOSESSION_H