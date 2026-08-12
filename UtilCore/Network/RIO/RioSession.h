
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

class CRioCore;
class CRioBuffer;

//***************************************************************************
// @class CRioSession
// @brief RIO 기반 네트워크 세션의 공통 기반 클래스.
//
// @details
//      CRioObject를 상속하여 RIO 비동기 I/O의 논리적 Owner 역할을
//      수행합니다.
//
//      CRioEvent는 RIO completion이 도착할 때까지 CRioObject의
//      shared_ptr을 보유하므로 Session의 lifetime을 안전하게 유지합니다.
//
//      Session은 다음 lifecycle을 가집니다.
//
//          Create
//             |
//             v
//          Initialize
//             |
//             v
//          Active
//             |
//             +---- Receive / Send
//             |
//             v
//          Closing
//             |
//             v
//          Closed
//
// [I/O Count Policy]
//
//      _ioCount는 CRioObject가 단독으로 관리합니다.
//
//          IncrementIoCount()
//              |
//              v
//          RIO submission
//              |
//              v
//          completion
//              |
//              v
//          DecrementIoCount()
//
//      Session에서는 IoCount를 직접 초기화하지 않습니다.
//      특히 ResetIoCount()와 같은 외부 강제 초기화 API를 사용하지 않습니다.
//
// [Socket Lifetime]
//
//      Session의 SOCKET은 Session의 논리적인 연결 상태를 나타냅니다.
//      RIO completion이 아직 남아 있는 동안 Session 객체 자체가 파괴되지
//      않도록 CRioEvent의 Owner shared_ptr lifetime guarantee를 사용합니다.
//
// [Shutdown Policy]
//
//      Session의 Close는 새로운 I/O 등록을 중단시키는 방향으로 동작하며,
//      실제 객체의 최종 destruction은 outstanding RIO I/O가 모두 완료된
//      이후 외부 lifecycle에서 보장되어야 합니다.
//
// [I/O Submission Policy]
//
//      실제 RIO submission과 IoCount / Owner / Buffer slot lifetime 관리는
//      CRioReceive / CRioSend가 담당합니다.
//
//      CRioSession은 Active 상태인지 확인한 후 해당 Submission utility에
//      작업을 위임합니다.
//
//***************************************************************************
class CRioSession : public CRioObject
{
public:
    using SessionId = uint64_t;

public:
    CRioSession() noexcept;
    ~CRioSession() noexcept override;

    CRioSession(const CRioSession&) = delete;
    CRioSession& operator=(const CRioSession&) = delete;

    CRioSession(CRioSession&&) = delete;
    CRioSession& operator=(CRioSession&&) = delete;

public:
    bool Initialize(CRioCore& core, SOCKET socket, RIO_RQ requestQueue) noexcept;

    void Close() noexcept;

    SessionId GetSessionId() const noexcept;
    bool IsActive() const noexcept;
    bool IsClosing() const noexcept;
    bool IsClosed() const noexcept;
    SOCKET GetSocket() const noexcept;
    RIO_RQ GetRequestQueue() const noexcept;
    CRioCore* GetCore() const noexcept;
    bool HasOutstandingIo() const noexcept;

public:
    bool StartReceive(CRioBuffer* bufferOwner, uint32_t slotIndex, const RIO_BUF& buffer, CRioEvent* rioEvent, DWORD flags = 0) noexcept;
    bool StartReceiveEx(const RIO_BUF* data, ULONG dataBufferCount, const CRioEvent::BufferBinding* dataBinding, const RIO_BUF* localAddress, const RIO_BUF* remoteAddress, const RIO_BUF* control, CRioEvent* rioEvent, DWORD flags = 0) noexcept;
    bool StartSend(CRioBuffer* bufferOwner, uint32_t slotIndex, const RIO_BUF& buffer, CRioEvent* rioEvent, DWORD flags = 0) noexcept;
    bool StartSendEx(const RIO_BUF* data, ULONG dataBufferCount, const CRioEvent::BufferBinding* dataBindings, const RIO_BUF* localAddress, const RIO_BUF* remoteAddress, const RIO_BUF* control, CRioEvent* rioEvent, DWORD flags = 0) noexcept;

public:
    void Dispatch(CRioEvent* rioEvent, ULONG bytesTransferred, LONG status) override;

protected:
    virtual void OnReceiveCompleted(CRioEvent* rioEvent, ULONG bytesTransferred, LONG status) noexcept;
    virtual void OnSendCompleted(CRioEvent* rioEvent, ULONG bytesTransferred, LONG status) noexcept;
    virtual void OnClosing() noexcept;
    virtual void OnClosed() noexcept;

private:
    bool TryChangeState(Rio::SessionState expected, Rio::SessionState desired) noexcept;

private:
    SessionId   _sessionId{ 0 };        // 세션 고유 식별자 (0: 미할당)

    CRioCore*   _core;                  // RIO 커널 API 호출 및 디스패처 구동을 담당하는 Core 시스템의 참조 포인터
    SOCKET      _socket;                // 클라이언트 연결에 할당된 WinSock 소켓 핸들
    RIO_RQ      _requestQueue;          // RIO 비동기 송수신 명령을 제출(Submit)하는 전용 Request Queue 핸들

    std::atomic<Rio::SessionState> _state;  // 원자적(Atomic) 스레드 세이프 상태 추적 변수 (None -> Active -> Closing -> Closed)
};

#endif // __RIOSESSION_H__
