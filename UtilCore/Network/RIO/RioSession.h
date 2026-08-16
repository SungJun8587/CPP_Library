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

	void Init(uint64_t sessionId, CRioCore* core, CRioBuffer* globalRecvBufferPool, SOCKET socket, RIO_RQ requestQueue, RIO_BUFFERID sendBufferId) noexcept;
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

	void OnReceiveCompleted(CRioEvent* rioEvent, DWORD bytesTransferred) noexcept;
	void OnSendCompleted(CRioEvent* rioEvent, DWORD bytesTransferred) noexcept;

private:
	uint64_t _sessionId{ 0 };                           // 고유 세션 ID

	CRioCore* _core{ nullptr };                         // RIO Core 객체 포인터
	CRioBuffer* _globalRecvBufferPool{ nullptr };       // 전역 수신 버퍼 풀 포인터

	// Close/Finalize와 RIO submit 사이의 concurrent read/write 보호
	std::atomic<SOCKET> _socket{ INVALID_SOCKET };      // 세션 바인딩 소켓
	std::atomic<RIO_RQ> _requestQueue{ RIO_INVALID_RQ }; // RIO Request Queue 핸들

	RIO_BUFFERID _sendBufferId{ RIO_INVALID_BUFFERID }; // 송신 버퍼 RIO ID

	CNetAddress _netAddress;                            // 원격 클라이언트 네트워크 주소 (IP/Port)

	// I/O submit과 Close/Finalize의 상호 배제를 위한 게이트
	mutable PLock _ioSubmitLock;                        // I/O 제출 및 상태 전이 동기화 게이트 락

	std::atomic<Rio::SessionState> _state{ Rio::SessionState::Created };        // 세션 상태 머신 변수
	std::atomic<Rio::CloseReason> _closeReason{ Rio::CloseReason::None };       // 세션 종료 사유

	PRWLock _sendLock;                                  // 송신 동기화 RW 락
	bool _isSending{ false };                           // 전송 진행 여부 플래그

	CRingBuffer _sendBuffer{ Rio::kSendRingBufferSize };    // 64KB 송신 링버퍼
	CRingBuffer _recvBuffer{ Rio::kRecvRingBufferSize };    // 64KB 수신 링버퍼
};

#endif // ndef __RIOSESSION_H__