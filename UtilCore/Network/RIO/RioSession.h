
//***************************************************************************
// RioSession.h : interface for the CRioSession class.
//
//***************************************************************************

#ifndef __RIOSESSION_H__
#define __RIOSESSION_H__

#ifndef __RIOCOMMON_H__
#include <Network/Rio/RioCommon.h>
#endif

#ifndef __RIOOBJECT_H__
#include <Network/Rio/RioObject.h>
#endif

#ifndef __RIOEVENT_H__
#include <Network/Rio/RioEvent.h>
#endif

#ifndef __RIOSEND_H__
#include <Network/Rio/RioSend.h>
#endif

#ifndef __RIORECEIVE_H__
#include <Network/Rio/RioReceive.h>
#endif

#ifndef __PLATFORMLOCK_H__
#include <Thread/PlatformLock.h>
#endif

#include <atomic>
#include <memory>
#include <cstdint>

class CRioCore;
class CRioBuffer;

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

public:
	// CSession 공통 인터페이스 오버라이드
	virtual void	Disconnect(const TCHAR* cause) override;
	virtual bool	IsConnected() const override { return IsActive(); }
	virtual SOCKET	GetSocket() const noexcept override { return _socket.load(std::memory_order_acquire); }

	//***************************************************************************
	// @brief 세션을 초기화하고 이 세션 소유의 송신 버퍼를 RIO에 등록합니다.
	// @return bool 초기화(및 송신 버퍼 등록) 성공 시 true.
	//         false를 반환하면 세션은 Active로 전이하지 않으며, 호출자가
	//         clientSocket/requestQueue를 직접 정리해야 합니다(세션이 아직
	//         Active가 아니므로 Close()로 자기 자신을 정리시킬 수 없음).
	//***************************************************************************
	bool Init(uint64_t sessionId, CRioCore* core, CRioBuffer* globalRecvBufferPool, SOCKET socket, RIO_RQ requestQueue) noexcept;
	void Close(Rio::CloseReason reason) noexcept;

	//***************************************************************************
	// @brief 강제 종료 사유로 세션을 종료합니다.
	//***************************************************************************
	void Close() noexcept { Close(Rio::CloseReason::ForcedClose); }

	bool PostInitialReceive() noexcept;
	bool Send(const void* data, uint16_t size) noexcept;

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
	void SetSessionId(uint64_t sessionId) noexcept { _sessionId = sessionId; }

	//***************************************************************************
	// @brief 고유 세션 ID를 반환합니다.
	// @return uint64_t 고유 세션 ID
	//***************************************************************************
	uint64_t GetSessionId() const noexcept { return _sessionId; }

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

	void OnReceiveCompleted(CRioEvent* rioEvent, DWORD bytesTransferred) noexcept;
	void OnSendCompleted(CRioEvent* rioEvent, DWORD bytesTransferred) noexcept;

private:
	uint64_t _sessionId{ 0 };                           // 고유 세션 ID

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

	CRingBuffer _sendBuffer{ Rio::kSendRingBufferSize };    // 64KB 송신 링버퍼 (세션 독자 소유 메모리)
	CRingBuffer _recvBuffer{ Rio::kRecvRingBufferSize };    // 64KB 수신 링버퍼
};

#endif // ndef __RIOSESSION_H__