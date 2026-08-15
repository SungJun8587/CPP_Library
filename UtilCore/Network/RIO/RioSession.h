
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

#ifndef __RIORECEIVE_H__
#include <Network/Rio/RioReceive.h>
#endif

#ifndef __RIOSEND_H__
#include <Network/Rio/RioSend.h>
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
// @struct PacketHeader
// @brief 네트워크 패킷 기본 헤더 (4바이트)
//***************************************************************************
#pragma pack(push, 1)
struct PacketHeader
{
    uint16_t size; // 헤더 포함 패킷 전체 크기
    uint16_t id;   // 패킷 프로토콜 ID
};
#pragma pack(pop)

//***************************************************************************
// @class CRioSession
// @brief RIO(Registered I/O) 및 CRingBuffer 기반의 고성능 네트워크 세션 클래스
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
    void Init(uint64_t sessionId, CRioCore* core, CRioBuffer* globalRecvBufferPool, SOCKET socket, RIO_RQ requestQueue, RIO_BUFFERID sendBufferId) noexcept;
    void Close(Rio::CloseReason reason) noexcept;

    //***************************************************************************
    // @brief 강제 종료 사유로 세션을 종료합니다.
    //***************************************************************************
    void Close() noexcept { Close(Rio::CloseReason::ForcedClose); }

    bool PostInitialReceive() noexcept;
    bool Send(const void* data, uint16_t size) noexcept;

    void Disconnect(const TCHAR* cause);

    virtual void Dispatch(CRioEvent* rioEvent, ULONG bytesTransferred, LONG status) override;

    //***************************************************************************
    // @brief 비동기 I/O 카운트를 안전하게 증가시킵니다.
    //***************************************************************************
    void IncrementIoCountNoLock() noexcept { _outstandingIo.fetch_add(1, std::memory_order_relaxed); }

    void DecrementIoCountNoLock() noexcept;
    void TryFinalizeClose() noexcept;

    //***************************************************************************
    // @brief 고유 세션 ID를 반환합니다.
    // @return uint64_t 고유 세션 ID
    //***************************************************************************
    uint64_t GetSessionId() const noexcept { return _sessionId; }

    //***************************************************************************
    // @brief 바인딩된 소켓 핸들을 반환합니다.
    // @return SOCKET 소켓 핸들
    //***************************************************************************
    SOCKET GetSocket() const noexcept { return _socket.load(std::memory_order_acquire); }

    //***************************************************************************
    // @brief RIO Request Queue 핸들을 반환합니다.
    // @return RIO_RQ RIO Request Queue 핸들
    //***************************************************************************
    RIO_RQ GetRequestQueue() const noexcept { return _requestQueue.load(std::memory_order_acquire); }

    //***************************************************************************
    // @brief 세션 종료 사유를 반환합니다.
    // @return CloseReason 세션 종료 사유
    //***************************************************************************
    Rio::CloseReason GetCloseReason() const noexcept { return _closeReason.load(std::memory_order_acquire); }

    //***************************************************************************
    // @brief RIO Core 엔진 객체 포인터를 반환합니다.
    // @return CRioCore* RIO Core 객체 포인터
    //***************************************************************************
    CRioCore* GetCore() const noexcept { return _core; }

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
    // @brief 세션이 정상적으로 활성화되어 통신 가능한 상태인지 여부를 반환합니다.
    // @return bool 활성 상태인 경우 true, 아니면 false
    //***************************************************************************
    bool IsActive() const noexcept { return _state.load(std::memory_order_acquire) == Rio::SessionState::Active; }

protected:
    virtual void OnConnected() {}
    virtual void OnDisconnected(Rio::CloseReason reason) {}
    virtual void OnPacketReceived(const PacketHeader& header, const char* payload, uint16_t payloadSize) = 0;

private:
    void FinalizeClose() noexcept;
    bool PostReceiveInternal() noexcept;
    bool FlushSendInternal() noexcept;
    void ProcessPackets() noexcept;
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

    // I/O submit과 Close/Finalize의 상호 배제를 위한 게이트
    // Active 상태에서 RIO submit을 시작한 경우 실제 submit 완료까지 유지됩니다.
    mutable PLock _ioSubmitLock;                        // I/O 제출 및 상태 전이 동기화 게이트 락

    std::atomic<Rio::SessionState> _state{ Rio::SessionState::Created };        // 세션 상태 머신 변수
    std::atomic<uint32_t> _outstandingIo{ 0 };                                  // 진행 중인 Outstanding I/O 카운터
    std::atomic<Rio::CloseReason> _closeReason{ Rio::CloseReason::None };       // 세션 종료 사유

    PRWLock _sendLock;                                      // 송신 동기화 RW 락
    bool _isSending{ false };                               // 전송 진행 여부 플래그

    CRingBuffer _sendBuffer{ Rio::kSendRingBufferSize };    // 64KB 송신 링버퍼
    CRingBuffer _recvBuffer{ Rio::kRecvRingBufferSize };    // 64KB 수신 링버퍼
    
    char _packetPayloadBuffer[Rio::kMaxPacketSize]{};       // 패킷 페이로드 임시 추출 버퍼
};

#endif // ndef __RIOSESSION_H__