
//***************************************************************************
// RioSession.cpp : implementation of the CRioSession class.
//
//***************************************************************************

#include "pch.h"
#include "RioSession.h"

#include <cassert>
#include <limits>

//***************************************************************************
// @brief CRioSession 생성자
//***************************************************************************
CRioSession::CRioSession() = default;

//***************************************************************************
// @brief CRioSession 소멸자
// @note 정상 라이프사이클에서는 FinalizeClose()가 이미 소켓/송신버퍼를 정리했어야
//       합니다. 여기서는 비정상 경로(Init() 실패 후 소멸 등)에 대비한 방어적 정리만
//       수행합니다. UnregisterSendBuffer()/CloseSocketInternal() 둘 다 idempotent라
//       중복 호출해도 안전합니다.
//***************************************************************************
CRioSession::~CRioSession() noexcept
{
    UnregisterSendBuffer();
    CloseSocketInternal();
}

//***************************************************************************
// @brief 새 세션을 초기화합니다 (Created 상태에서만 허용).
// @param sessionId 고유 세션 ID
// @param core RIO Core 객체 포인터
// @param globalRecvBufferPool 전역 수신 버퍼 풀 포인터
// @param socket 클라이언트 소켓 핸들
// @param requestQueue RIO Request Queue 핸들
// @return bool 성공 시 true. false면 이 세션은 Active로 전이하지 않으므로
//         호출자가 socket/requestQueue를 직접 정리해야 합니다.
//***************************************************************************
bool CRioSession::Init(uint64 sessionId, CRioCore* core, CRioBuffer* globalRecvBufferPool, SOCKET socket, RIO_RQ requestQueue) noexcept
{
    assert(_state.load(std::memory_order_acquire) == Rio::SessionState::Created);

    assert(core != nullptr);
    assert(globalRecvBufferPool != nullptr);
    assert(socket != INVALID_SOCKET);
    assert(requestQueue != RIO_INVALID_RQ);

    _sessionId = sessionId;
    _core = core;
    _globalRecvBufferPool = globalRecvBufferPool;
    _socket.store(socket, std::memory_order_release);
    _requestQueue.store(requestQueue, std::memory_order_release);

    _closeReason.store(Rio::CloseReason::None, std::memory_order_release);

    {
        PRWriteLockGuard sendWriteGuard(_sendLock, __FUNCTION__);

        _isSending = false;
        _sendBuffer.Clear();
        _recvBuffer.Clear();
    }

    // 이 세션 소유의 _sendBuffer 메모리를 RIO에 등록합니다. FlushSendInternal()의
    // GetRioSendBuffers()가 이 등록된 영역 기준으로 Offset을 계산하므로, 반드시
    // Active로 전이하기 전에(=첫 Send() 호출보다 먼저) 등록이 끝나 있어야 합니다.
    if( !RegisterSendBufferIfNeeded() )
    {
        assert(false && "CRioSession::Init send buffer registration failed");
        return false;
    }

    _state.store(Rio::SessionState::Active, std::memory_order_release);

    OnConnected();
    return true;
}

//***************************************************************************
// @brief ConnectEx로 비동기 연결을 게시합니다 (클라이언트 측 전용).
// @param dispatcher 완료 통지를 받을 전용 디스패처
// @param sessionId 고유 세션 ID
// @param core RIO Core 객체 포인터
// @param globalRecvBufferPool 전역 수신 버퍼 풀 포인터
// @param remoteAddr 접속할 원격 주소
// @return bool 게시 시도 자체의 성공 여부. 상세 계약은 헤더 주석 참고.
// @details 이 시점에 RIO 전용 소켓을 직접 생성한다(기존 동기 흐름에서는
//          CRioClientService가 CSocketUtils::CreateRioSocket()으로 미리 만들어
//          Init()에 넘겼지만, 비동기 흐름에서는 이 함수가 그 시점을 흡수한다).
//          ConnectEx는 사전에 bind()된 소켓에서만 호출 가능하므로 와일드카드
//          (0.0.0.0:0)로 바인딩한 뒤, 이 소켓을 dispatcher의 전용 IOCP에 연결하고
//          ConnectEx를 게시한다. 와일드카드 바인딩에 기본 생성된 CNetAddress()가
//          아니라 CNetAddress(_T("0.0.0.0"), 0)을 명시적으로 쓰는 이유: 기본
//          생성자는 SOCKADDR_IN을 전부 0으로 두어 sin_family도 0이 되고, 이러면
//          AF_INET 소켓에 bind()가 실패한다(CNetAddress(ip, port) 생성자만
//          sin_family=AF_INET을 명시적으로 세팅함 — NetAddress.h/.cpp 확인 후 발견/수정).
//***************************************************************************
bool CRioSession::ConnectAsync(CRioConnectDispatcher& dispatcher, uint64 sessionId, CRioCore* core,
    CRioBuffer* globalRecvBufferPool, const CNetAddress& remoteAddr)
{
    assert(_state.load(std::memory_order_acquire) == Rio::SessionState::Created);

    if( core == nullptr || globalRecvBufferPool == nullptr )
    {
        FailConnect(Rio::CloseReason::InternalError);
        return false;
    }

    SOCKET socket = CSocketUtils::CreateRioSocket();
    if( socket == INVALID_SOCKET )
    {
        FailConnect(Rio::CloseReason::SocketError);
        return false;
    }

    _socket.store(socket, std::memory_order_release);
    _core = core;
    _globalRecvBufferPool = globalRecvBufferPool;
    _sessionId = sessionId;

    if( !CSocketUtils::Bind(socket, CNetAddress(_T("0.0.0.0"), 0)) )
    {
        FailConnect(Rio::CloseReason::SocketError);
        return false;
    }

    if( !dispatcher.RegisterSocket(socket) )
    {
        FailConnect(Rio::CloseReason::SocketError);
        return false;
    }

    // RegisterConnect() 내부에서 ConnectEx 게시가 즉시 실패하면 자체적으로
    // FailConnect()를 호출합니다 — 그 경우도 이 함수는 true를 반환합니다(게시
    // "시도" 자체는 정상적으로 이뤄졌고, 실패 통지는 OnDisconnected()로 이미
    // 처리됐기 때문). 헤더의 ConnectAsync() 계약 설명 참고.
    RegisterConnect(remoteAddr);
    return true;
}

//***************************************************************************
// @brief ConnectEx 비동기 연결을 실제로 게시합니다.
//***************************************************************************
void CRioSession::RegisterConnect(const CNetAddress& remoteAddr)
{
    _connectEvent.Init();

    // 완료 통지까지 세션 수명을 보장 (IOCP RecvEvent/SendEvent의 owner와 동일한
    // 역할). CSession::shared_from_this()가 shared_ptr<CSession>을 반환하므로,
    // GetRioObjectPtr()와 동일한 aliasing 패턴으로 CRioSessionRef로 변환한다.
    _connectEvent.owner = CRioSessionRef(shared_from_this(), this);

    SOCKADDR_IN sockAddr = remoteAddr.GetSockAddr();
    DWORD bytesSent = 0;
    SOCKET socket = GetSocket();

    if( CSocketUtils::ConnectEx(socket, reinterpret_cast<SOCKADDR*>(&sockAddr), sizeof(sockAddr),
        nullptr, 0, &bytesSent, static_cast<LPOVERLAPPED>(&_connectEvent)) == FALSE )
    {
        int32 errorCode = ::WSAGetLastError();
        if( errorCode != WSA_IO_PENDING )
        {
            // 게시 자체가 즉시 실패 — IOCP 완료 통지가 오지 않으므로 여기서 직접 정리.
            _connectEvent.owner = nullptr;
            FailConnect(Rio::CloseReason::SocketError);
        }
    }
}

//***************************************************************************
// @brief ConnectEx 완료 통지 처리. CRioConnectDispatcher의 워커 스레드가 호출합니다.
// @details 성공 확인 후 SO_UPDATE_CONNECT_CONTEXT -> RIOCreateRequestQueue() ->
//          Init() -> PostInitialReceive() 순서로 세션을 완성시킵니다. Init()이
//          내부적으로 OnConnected()를 호출하므로 여기서 별도로 부를 필요는 없습니다.
//***************************************************************************
void CRioSession::ProcessConnectEx()
{
    SOCKET socket = GetSocket();

    int32 sockError = 0;
    bool getOptOk = CSocketUtils::GetSocketError(socket, sockError);

    if( !getOptOk || sockError != 0 )
    {
        FailConnect(Rio::CloseReason::SocketError);
        return;
    }

    // ConnectEx로 연결된 소켓은 SO_UPDATE_CONNECT_CONTEXT를 걸어야
    // getpeername/setsockopt/RIOCreateRequestQueue 등이 정상 동작합니다.
    if( !CSocketUtils::SetUpdateConnectContext(socket) )
    {
        FailConnect(Rio::CloseReason::SocketError);
        return;
    }

    if( _core == nullptr )
    {
        FailConnect(Rio::CloseReason::InternalError);
        return;
    }

    const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _core->GetRioTable();

    RIO_RQ requestQueue = rioTable.RIOCreateRequestQueue(
        socket,
        Rio::kRequestQueueMaxReceiveOutstanding,
        Rio::kRequestQueueMaxReceiveDataBuffers,
        Rio::kRequestQueueMaxSendOutstanding,
        Rio::kRequestQueueMaxSendDataBuffers,
        _core->GetReceiveQueue(),
        _core->GetSendQueue(),
        nullptr
    );

    if( requestQueue == RIO_INVALID_RQ )
    {
        FailConnect(Rio::CloseReason::InternalError);
        return;
    }

    if( !Init(_sessionId, _core, _globalRecvBufferPool, socket, requestQueue) )
    {
        FailConnect(Rio::CloseReason::InternalError);
        return;
    }

    PostInitialReceive();
}

//***************************************************************************
// @brief connect 실패(또는 그 이전 단계 실패) 시 정리 전용 경로.
// @param reason 실패 사유
//***************************************************************************
void CRioSession::FailConnect(Rio::CloseReason reason) noexcept
{
    _closeReason.store(reason, std::memory_order_release);

    // Active를 거친 적 없는 예외 경로이므로 Close()의 Active->Closing CAS를
    // 우회하고 직접 Closed로 확정한다 (헤더의 FailConnect() 주석 참고).
    _state.store(Rio::SessionState::Closed, std::memory_order_release);

    CloseSocketInternal();  // idempotent
    UnregisterSendBuffer(); // 보통 미등록 상태지만(Init 전) idempotent라 방어적으로 호출

    OnDisconnected(reason);      // 상위 콘텐츠 레이어 훅 (protected virtual)
    CSession::OnDisconnected();  // 서비스의 ReleaseSession 콜백 연동
}

//***************************************************************************
// @brief 이 세션 소유의 _sendBuffer 메모리를 RIORegisterBuffer()로 등록합니다.
// @return 이미 등록됐거나 새로 등록 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSession::RegisterSendBufferIfNeeded() noexcept
{
    // 이미 등록되어 있다면(예: 방어적 재호출) 그대로 성공 처리
    if( _sendBufferId != RIO_INVALID_BUFFERID ) return true;

    if( _core == nullptr ) return false;

    const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _core->GetRioTable();
    if( rioTable.RIORegisterBuffer == nullptr ) return false;

    char* bufferBegin = _sendBuffer.GetBufferBegin();
    char* bufferEnd = _sendBuffer.GetBufferEnd();

    if( bufferBegin == nullptr || bufferEnd <= bufferBegin ) return false;

    const size_t totalSize = static_cast<size_t>(bufferEnd - bufferBegin);

    // RIORegisterBuffer()의 DataLength 파라미터는 DWORD입니다.
    if( totalSize > static_cast<size_t>((std::numeric_limits<DWORD>::max)()) ) return false;

    const RIO_BUFFERID bufferId = rioTable.RIORegisterBuffer(bufferBegin, static_cast<DWORD>(totalSize));
    if( bufferId == RIO_INVALID_BUFFERID ) return false;

    _sendBufferId = bufferId;
    return true;
}

//***************************************************************************
// @brief 등록했던 _sendBuffer의 RIO 버퍼 ID를 해제합니다.
//***************************************************************************
void CRioSession::UnregisterSendBuffer() noexcept
{
    if( _sendBufferId == RIO_INVALID_BUFFERID ) return;

    if( _core != nullptr )
    {
        const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _core->GetRioTable();

        if( rioTable.RIODeregisterBuffer != nullptr )
            rioTable.RIODeregisterBuffer(_sendBufferId);
    }

    _sendBufferId = RIO_INVALID_BUFFERID;
}

//***************************************************************************
// @brief 외부(NetService 등)에서 요청한 세션 강제 종료 처리
// @param cause 종료 사유 메시지
//***************************************************************************
void CRioSession::Disconnect(const TCHAR* cause)
{
    Close(Rio::CloseReason::ForcedClose);
}

//***************************************************************************
// @brief 지정된 사유로 세션 종료를 요청합니다 (락 내부에서 상태 전이 후 필요시 락 밖에서 실행).
// @param reason 세션 종료 사유
// @note [수정 — outstanding I/O drain 후 소켓을 닫도록 변경]
//       예전 버전은 진행 중인 RIO 요청(outstanding I/O)을 기다리지 않고 즉시
//       FinalizeClose()로 넘어가 소켓을 닫았다. RIO 공식 문서(RIOCloseCompletionQueue,
//       LPFN_RIOCREATEREQUESTQUEUE)와 실제 사례(MS Q&A에 보고된, 이미 진행 중이던
//       요청의 RQ가 담긴 소켓이 다른 스레드에서 closesocket()될 때 mswsock.dll이
//       크래시하는 사례)를 확인한 결과 — "이미 게시되어 진행 중이던 요청이 소켓
//       close 이후에도 CQ로 에러 completion을 정상 반환한다"는 보장은 문서 어디에도
//       없었다. CRioCore::Shutdown() 자체는 이미 "outstanding이 0이 될 때까지
//       CQ/IOCP를 안 닫는" 훨씬 보수적인 패턴을 쓰는데, 개별 세션 레벨(Close())만
//       그 원칙을 안 지키고 있던 것 — 이제 동일한 원칙을 세션 단위로도 적용한다.
//
//       [비블로킹 구현] outstanding I/O가 없으면(가장 흔한 경로 — 유휴 세션 종료)
//       즉시 FinalizeClose()로 넘어간다. outstanding이 있으면 여기서는 소켓
//       shutdown()만 해두고 아무것도 기다리지 않은 채 반환한다 — 남은 completion들이
//       도착할 때마다 CRioCore::ProcessRioResult()의 ObjectIoCountGuard가
//       DecrementIoCount() 이후 OnIoCountReachedZero()를 호출해주고, 그 훅이
//       Closing 상태를 보고 대신 FinalizeClose()를 호출한다(아래 OnIoCountReachedZero()
//       참고). 이 방식이면 RIO 워커 스레드가 다른 세션의 completion 처리를
//       기다리며 블로킹되는 일이 없다 — CRioCore::Shutdown()처럼 poll 루프를
//       세션 레벨에 두면 그 폴링을 처리할 워커 자신이 다른 completion을 기다리며
//       멈춰버리는 데드락 위험이 있어 그 방식은 채택하지 않았다.
//***************************************************************************
void CRioSession::Close(Rio::CloseReason reason) noexcept
{
    bool shouldFinalizeNow = false;

    {
        PLockGuard guard(_ioSubmitLock, __FUNCTION__);

        Rio::SessionState expected = Rio::SessionState::Active;

        if( !_state.compare_exchange_strong(expected, Rio::SessionState::Closing, std::memory_order_acq_rel, std::memory_order_acquire) )
        {
            return;
        }

        _closeReason.store(reason, std::memory_order_release);

        // 소켓 셧다운 먼저 수행 (신규 데이터 송수신 차단, 이미 게시된 요청은
        // 그대로 진행 중 — closesocket()은 여기서 하지 않는다)
        ShutdownSocketInternal();

        // outstanding I/O가 이미 없으면(가장 흔함) 곧바로 최종 정리해도 안전하다.
        // 있으면 여기서 아무것도 하지 않는다 — PostReceiveInternal()/
        // FlushSendInternal()도 이 함수와 동일하게 _ioSubmitLock을 잡고 나서야
        // 신규 제출 전 상태를 확인하므로(위 CAS로 이미 Closing이 된 이후에는
        // 그쪽에서 새로 IncrementIoCount()가 절대 끼어들 수 없다 — 락으로
        // 직렬화됨), 지금 관측한 이 카운트가 앞으로 남은 completion 수의
        // 정확한 상한이다.
        shouldFinalizeNow = !HasOutstandingIo();
    }

    if( shouldFinalizeNow )
    {
        FinalizeClose();
    }
    // else: 마지막 남은 completion의 OnIoCountReachedZero() 훅이 대신 정리한다.
}

//***************************************************************************
// @brief CRioObject::OnIoCountReachedZero() 오버라이드.
// @details Close()가 outstanding I/O를 남겨둔 채 반환했을 경우, 그 마지막
//          completion의 처리 결과로 이 함수가 호출된다(호출부: CRioCore::
//          ProcessRioResult()의 ObjectIoCountGuard, 이미 RIO 워커 스레드
//          위에서 실행 중 — 여기서 블로킹 대기 절대 금지). Closing 상태가
//          아니면(Active 상태에서 카운트가 우연히 0을 지나가는 경우 등)
//          아무 의미가 없으므로 무시한다.
//***************************************************************************
void CRioSession::OnIoCountReachedZero() noexcept
{
    if( _state.load(std::memory_order_acquire) == Rio::SessionState::Closing )
    {
        FinalizeClose();
    }
}

//***************************************************************************
// @brief 세션의 모든 리소스(RIO_RQ, 소켓, 송신 버퍼 등록 등)를 안전하게 해제하고
//        연결 해제 콜백을 호출합니다.
//***************************************************************************
void CRioSession::FinalizeClose() noexcept
{
    Rio::CloseReason closeReason = Rio::CloseReason::InternalError;

    {
        PLockGuard guard(_ioSubmitLock, __FUNCTION__);

        Rio::SessionState expected = Rio::SessionState::Closing;

        if( !_state.compare_exchange_strong(expected, Rio::SessionState::Closed, std::memory_order_acq_rel, std::memory_order_acquire) )
        {
            return;
        }

        closeReason = _closeReason.load(std::memory_order_acquire);

        // 1. RIO Request Queue 핸들 초기화 (소켓 닫힐 때 RIO 서브시스템이 함께 정리함)
        _requestQueue.store(RIO_INVALID_RQ, std::memory_order_release);

        // 2. 소켓 핸들 완전 정리
        CloseSocketInternal();

        // 3. 이 세션이 등록했던 송신 버퍼 해제. Microsoft 문서상 outstanding
        //    send/receive가 남아있는 동안 deregister하지 말라는 권고가 있으나,
        //    바로 위 CloseSocketInternal()과 같은 이유로 여기서는 즉시 처리합니다
        //    (Close() 주석 참고 — 아직 확인이 필요한 부분).
        UnregisterSendBuffer();
    }

    // 4. 외부 락 범위 밖에서 안전하게 사용자 콜백 호출
    OnDisconnected(closeReason);

    // CSession에 정의된 상위 통지 함수 호출 -> CNetService의 ReleaseSession 자동 연동!
    CSession::OnDisconnected();
}

//***************************************************************************
// @brief 최초 비동기 수신(Receive) 요청을 게시합니다.
// @return 게시 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSession::PostInitialReceive() noexcept
{
    return PostReceiveInternal();
}

//***************************************************************************
// @brief 내부 RIO 비동기 수신 요청을 게시합니다.
// @return 게시 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSession::PostReceiveInternal() noexcept
{
    Rio::CloseReason failureReason = Rio::CloseReason::None;

    uint32 slotIndex = Rio::kInvalidSlotIndex;
    CRioEvent* rioEvent = nullptr;

    RIO_BUF rioBuf{};
    CRioCore* core = nullptr;
    CRioBuffer* bufferPool = nullptr;
    RIO_RQ requestQueue = RIO_INVALID_RQ;

    {
        PLockGuard guard(_ioSubmitLock, __FUNCTION__);

        // 1. 세션 상태 확인: 현재 세션이 Active 상태가 아니라면 수신 요청을 중단하고 실패 반환
        if( _state.load(std::memory_order_acquire) != Rio::SessionState::Active ) return false;

        // 2. 핵심 세션 멤버 변수 캐싱 (Core, 수신 버퍼 풀, 요청 큐 핸들)
        core = _core;
        bufferPool = _globalRecvBufferPool;
        requestQueue = _requestQueue.load(std::memory_order_acquire);

        // 3. 필수 리소스 포인터 및 큐 유효성 검사
        if( bufferPool == nullptr || core == nullptr || requestQueue == RIO_INVALID_RQ ) return false;

        // 4. 수신 버퍼 풀에서 사용할 슬롯(Slot) 할당 시도
        if( !bufferPool->AllocSlot(slotIndex) )
        {
            failureReason = Rio::CloseReason::BufferAllocationFailed;
        }
        // 5. 할당받은 슬롯의 실제 RIO 버퍼 정보(RIO_BUF) 획득 시도
        else if( !bufferPool->GetRioBuffer(slotIndex, rioBuf) )
        {
            // 정보 획득 실패 시, 앞서 할당받았던 슬롯을 곧바로 반납하고 상태 초기화
            bufferPool->FreeSlot(slotIndex);
            slotIndex = Rio::kInvalidSlotIndex;
            failureReason = Rio::CloseReason::InternalError;
        }
        else
        {
            // 6. 코어 객체로부터 RIO 이벤트 풀(Event Pool) 가져오기
            CRioEventPool* eventPool = core->GetEventPool();

            if( eventPool == nullptr )
            {
                // 이벤트 풀이 존재하지 않으면, 슬롯 반납 및 내부 에러 처리
                bufferPool->FreeSlot(slotIndex);
                slotIndex = Rio::kInvalidSlotIndex;
                failureReason = Rio::CloseReason::InternalError;
            }
            else
            {
                // 7. 이벤트 풀에서 비동기 I/O 처리를 위한 이벤트 객체 할당 받기
                rioEvent = eventPool->Alloc();
                if( rioEvent == nullptr )
                {
                    // 이벤트 할당 실패(풀 고갈) 시, 슬롯 반납 및 사유 기록
                    bufferPool->FreeSlot(slotIndex);
                    slotIndex = Rio::kInvalidSlotIndex;
                    failureReason = Rio::CloseReason::EventPoolExhausted;
                }
            }
        }

        // 8. 사전 자원 할당 및 준비 과정에 문제가 없다면 실제 수신(Receive) 요청 수행.
        //    Initialize()/BindBufferSlot()은 여기서 직접 하지 않습니다 —
        //    CRioReceive::Receive() 내부가 전적으로 책임집니다.
        if( failureReason == Rio::CloseReason::None )
        {
            const bool success = CRioReceive::Receive(
                *core,
                requestQueue,
                rioBuf,
                bufferPool,
                slotIndex,
                rioEvent,
                this,
                0);

            // 9. RIO Receive 요청 제출 실패 시 실패 사유 기록.
            //    실패했다면 CRioReceive::Receive() 내부에서 이미 slot/event/IoCount
            //    롤백까지 전부 처리했으므로 여기서 추가로 정리할 것은 없습니다.
            if( !success )
            {
                failureReason = Rio::CloseReason::ReceivePostFailed;
            }
        }
    }

    // 10. 과정 중 발생한 실패 사유가 존재한다면 세션을 지정된 사유로 종료하고 false 반환
    if( failureReason != Rio::CloseReason::None )
    {
        Close(failureReason);
        return false;
    }

    // 11. 모든 수신 포스트 과정 성공
    return true;
}

//***************************************************************************
// @brief CRioObject 완료 이벤트 디스패치 가상 함수 구현
// @param rioEvent 완료된 RIO 이벤트 포인터
// @param bytesTransferred 전송된 바이트 수
// @param status 연산 결과 상태 코드
//***************************************************************************
void CRioSession::Dispatch(CRioEvent* rioEvent, ULONG bytesTransferred, LONG status)
{
    // 세션이 활성 상태가 아니라면(이미 닫혔거나 종료 진행 중이라면) 뒤늦게 도착한
    // 완료 이벤트를 안전하게 차단합니다. IoCount 자체는 CRioCore::ProcessRioResult()의
    // ObjectIoCountGuard가 이 함수 반환 이후 자동으로 감소시키므로 여기서 별도로
    // 손댈 필요가 없습니다.
    if( !IsActive() )
    {
        return;
    }

    if( rioEvent == nullptr )
    {
        Close(Rio::CloseReason::InternalError);
        return;
    }

    if( status != 0 )
    {
        Close(Rio::CloseReason::SocketError);
        return;
    }

    // [수정] rioEvent->GetBufferBindings().empty()로 Send/Receive를 구분하던
    // 방식은, CRioSend::Send()(단일 버퍼 버전)가 BindBufferSlot()을 호출해
    // Send 이벤트에도 바인딩이 생길 수 있는 경로가 열려 있어 그 경로가 실제로
    // 쓰이면 Send를 Receive로 오분류하는 잠재 버그였다. CRioEvent가 Initialize()
    // 시점에 Rio::EventType(Send/Receive)을 명시적으로 기록해두므로, 그 값을
    // 직접 조회하는 GetEventType()으로 교체해 바인딩 유무와 무관하게 항상
    // 정확히 구분한다.
    switch( rioEvent->GetEventType() )
    {
    case Rio::EventType::Receive:
        OnReceiveCompleted(rioEvent, bytesTransferred);
        break;

    case Rio::EventType::Send:
        OnSendCompleted(rioEvent, bytesTransferred);
        break;

    default:
        assert(false && "CRioSession::Dispatch: unknown Rio::EventType on completed rioEvent");
        Close(Rio::CloseReason::InternalError);
        break;
    }
}

//***************************************************************************
// @brief 수신 완료 비동기 이벤트를 처리합니다.
// @param rioEvent 완료된 RIO 이벤트 포인터
// @param bytesTransferred 수신된 바이트 수
//***************************************************************************
void CRioSession::OnReceiveCompleted(CRioEvent* rioEvent, DWORD bytesTransferred) noexcept
{
    if( rioEvent == nullptr )
    {
        Close(Rio::CloseReason::InternalError);
        return;
    }

    const auto& bindings = rioEvent->GetBufferBindings();

    if( bindings.empty() )
    {
        Close(Rio::CloseReason::InternalError);
        return;
    }

    CRioBuffer* bufferPool = bindings[0].buffer;
    const uint32 slotIndex = bindings[0].slotIndex;

    if( bufferPool == nullptr )
    {
        Close(Rio::CloseReason::InternalError);
        return;
    }

    if( bytesTransferred == 0 )
    {
        // 상대방이 정상 종료(FIN)했을 때. BufferSlot/EventPool 반환은
        // CRioCore::ProcessRioResult()가 completion 처리 후 수행합니다.
        Close(Rio::CloseReason::RemoteClosed);
        return;
    }

    void* slotDataPtr = bufferPool->GetSlotAddress(slotIndex);

    if( slotDataPtr == nullptr )
    {
        Close(Rio::CloseReason::InternalError);
        return;
    }

    int64 enqueuedBytes = 0;

    const bool enqueueSuccess = _recvBuffer.Enqueue(
        static_cast<const char*>(slotDataPtr),
        static_cast<int64>(bytesTransferred),
        &enqueuedBytes,
        false);

    if( !enqueueSuccess || enqueuedBytes != static_cast<int64>(bytesTransferred) )
    {
        Close(Rio::CloseReason::RingBufferOverflow);
        return;
    }

    OnDataReceived();

    if( IsActive() && !PostReceiveInternal() )
    {
        // PostReceiveInternal() 실패 시 내부에서 이미 Close()가 호출됩니다.
        return;
    }
}

//***************************************************************************
// @brief 데이터를 송신 버퍼에 큐잉하고 RIO 전송을 진행합니다.
// @param data 전송할 데이터 버퍼 포인터
// @param size 전송할 데이터 크기 (바이트)
// @return 전송 큐잉 및 처리 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSession::Send(const void* data, uint16 size) noexcept
{
    if( data == nullptr || size == 0 ) return false;
    if( !IsActive() ) return false;

    bool needStartSend = false;

    {
        PRWriteLockGuard lockGuard(_sendLock, __FUNCTION__);

        // 락 획득 직후 세션 상태를 재확인 (IsActive() 체크와의 사이에 Close()가 끼어들 수 있음)
        if( _state.load(std::memory_order_acquire) != Rio::SessionState::Active ) return false;

        int64 enqueuedBytes = 0;

        const bool enqueueSuccess = _sendBuffer.Enqueue(
            static_cast<const char*>(data),
            static_cast<int64>(size),
            &enqueuedBytes,
            false);

        if( !enqueueSuccess || enqueuedBytes != static_cast<int64>(size) )
        {
            // 링버퍼(64KB)가 꽉 찼다. 과거에는 여기서 곧바로 Close(SendBufferOverflow)
            // 했으나, 그러면 body가 조금만 커도(예: 대량 POST) 연결이 끊겨버린다.
            // IOCP 세션(CVector<CSendBufferRef> 큐, 오브젝트 풀 기반이라 사실상
            // 무제한 큐잉)과 동작을 맞추기 위해, 여기서는 연결을 죽이지 않고
            // 오버플로 큐에 보관한다 — OnSendCompleted()가 공간을 비울 때마다
            // DrainOverflowIntoSendBufferLocked()로 이어서 채운다.
            const char* bytes = static_cast<const char*>(data);
            _sendOverflowQueue.emplace_back(bytes, bytes + size);
        }

        if( _isSending )
        {
            return true; // 이미 전송 루프 진행 중이므로 큐잉만 완료 (링버퍼든 오버플로든)
        }

        _isSending = true;
        needStartSend = true;
    }

    if( !needStartSend ) return true;

    if( !FlushSendInternal() )
    {
        PRWriteLockGuard lockGuard(_sendLock, __FUNCTION__);
        _isSending = false;
        return false;
    }

    return true;
}

//***************************************************************************
// @brief 송신 링버퍼 데이터를 가져와 RIO 전송을 요청합니다.
// @return 전송 요청 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSession::FlushSendInternal() noexcept
{
    Rio::CloseReason failureReason = Rio::CloseReason::None;

    CRioEvent* rioEvent = nullptr;

    // 링버퍼가 wrap되면 최대 2개 세그먼트가 나올 수 있습니다.
    RIO_BUF rioBufs[2]{};
    int bufferCount = 0;

    CRioCore* core = nullptr;
    RIO_RQ requestQueue = RIO_INVALID_RQ;

    {
        PLockGuard guard(_ioSubmitLock, __FUNCTION__);

        if( _state.load(std::memory_order_acquire) != Rio::SessionState::Active ) return false;

        core = _core;
        requestQueue = _requestQueue.load(std::memory_order_acquire);

        if( core == nullptr || requestQueue == RIO_INVALID_RQ ) return false;

        {
            PRReadLockGuard sendReadGuard(_sendLock, __FUNCTION__);
            bufferCount = _sendBuffer.GetRioSendBuffers(rioBufs, _sendBufferId);
        }

        if( bufferCount <= 0 )
        {
            PRWriteLockGuard sendWriteGuard(_sendLock, __FUNCTION__);
            _isSending = false;
            return true;
        }

        CRioEventPool* eventPool = core->GetEventPool();

        if( eventPool == nullptr )
        {
            failureReason = Rio::CloseReason::InternalError;
        }
        else
        {
            rioEvent = eventPool->Alloc();
            if( rioEvent == nullptr )
            {
                failureReason = Rio::CloseReason::EventPoolExhausted;
            }
        }

        if( failureReason == Rio::CloseReason::None )
        {
            // 중요: RIOSend/RIOSendEx/RIOReceive/RIOReceiveEx는 데이터 버퍼 쪽
            // scatter-gather를 지원하지 않습니다(MS 문서: DataBufferCount는
            // pData가 NULL이 아니면 반드시 1). 링버퍼 wrap으로 bufferCount==2가
            // 나오더라도 이번 호출에서는 첫 세그먼트(rioBufs[0])만 1개로 보냅니다.
            // 두 번째 세그먼트는 이 completion 이후 OnSendCompleted()가
            // FlushSendInternal()을 다시 부를 때(그 시점엔 read 커서가 이동해
            // 있어 wrap이 풀린 상태) 자연스럽게 처리됩니다.
            const ULONG sendBufferCount = 1;

            const bool success = CRioSend::SendEx(
                *core,
                requestQueue,
                &rioBufs[0],
                sendBufferCount,
                nullptr,   // dataBindings: 사전등록 버퍼라 slot ownership 이전 불필요
                nullptr,
                nullptr,
                nullptr,
                rioEvent,
                this,
                0);

            if( !success )
            {
                failureReason = Rio::CloseReason::SendPostFailed;
            }
        }
    }

    if( failureReason != Rio::CloseReason::None )
    {
        Close(failureReason);
        return false;
    }

    return true;
}

//***************************************************************************
// @brief 송신 완료 비동기 이벤트를 처리합니다.
// @param rioEvent 완료된 송신 RIO 이벤트 포인터
// @param bytesTransferred 전송된 바이트 수
//***************************************************************************
void CRioSession::OnSendCompleted(CRioEvent* rioEvent, DWORD bytesTransferred) noexcept
{
    if( rioEvent == nullptr )
    {
        Close(Rio::CloseReason::InternalError);
        return;
    }

    bool needFlush = false;
    bool invalidCompletion = false;

    {
        PRWriteLockGuard lockGuard(_sendLock, __FUNCTION__);

        const int64 currentQueuedBytes = _sendBuffer.GetSizeUsed();

        if( bytesTransferred == 0 && currentQueuedBytes > 0 )
        {
            invalidCompletion = true;
        }
        else if( bytesTransferred > static_cast<DWORD>(currentQueuedBytes) )
        {
            invalidCompletion = true;
        }
        else if( bytesTransferred > 0 )
        {
            if( !_sendBuffer.MoveReadBuffer(bytesTransferred) )
            {
                invalidCompletion = true;
            }
        }

        if( !invalidCompletion )
        {
            // 읽기 커서가 이동해 방금 생긴 여유 공간만큼 오버플로 큐를 채워
            // 넣는다 — _sendBuffer.GetSizeUsed()가 아래에서 최신 상태를
            // 반영하도록 needFlush 판단보다 먼저 수행해야 한다.
            DrainOverflowIntoSendBufferLocked();

            // 이번 호출은 항상 1세그먼트만 보냈으므로, 남은 데이터(wrap의 나머지
            // 세그먼트, 방금 오버플로에서 옮겨진 데이터 포함)가 있으면 다시
            // FlushSendInternal()을 호출해 이어서 보냅니다. 링버퍼가 비어있어도
            // 오버플로 큐에 아직 못 옮긴 청크가 남아있을 수 있으므로(그 청크가
            // 이번에 생긴 여유 공간보다 컸던 경우) 함께 확인한다.
            const bool moreInBuffer = _sendBuffer.GetSizeUsed() > 0;
            const bool moreInOverflow = !_sendOverflowQueue.empty();

            if( IsActive() && (moreInBuffer || moreInOverflow) )
            {
                needFlush = true;
            }
            else
            {
                _isSending = false;
            }
        }
        else
        {
            _isSending = false;
        }
    }

    if( invalidCompletion )
    {
        Close(Rio::CloseReason::InternalError);
        return;
    }

    if( needFlush && !FlushSendInternal() )
    {
        PRWriteLockGuard lockGuard(_sendLock, __FUNCTION__);
        _isSending = false;
    }
}

//***************************************************************************
// @brief 오버플로 큐에 쌓인 청크를 _sendBuffer에 여유 공간이 생긴 만큼 옮겨 담습니다.
// @details _sendLock을 write로 보유한 상태에서만 호출해야 합니다.
//***************************************************************************
void CRioSession::DrainOverflowIntoSendBufferLocked() noexcept
{
    while( !_sendOverflowQueue.empty() )
    {
        const std::vector<char>& chunk = _sendOverflowQueue.front();

        int64 enqueuedBytes = 0;
        const bool enqueueSuccess = _sendBuffer.Enqueue(
            chunk.data(),
            static_cast<int64>(chunk.size()),
            &enqueuedBytes,
            false);

        if( !enqueueSuccess || enqueuedBytes != static_cast<int64>(chunk.size()) )
        {
            // 아직 이 청크를 통째로 넣을 공간이 없음 — 쪼개지 않고 큐에 남겨둔 채
            // 중단한다. 다음 OnSendCompleted()가 다시 시도한다.
            break;
        }

        _sendOverflowQueue.pop_front();
    }
}

//***************************************************************************
// @brief 소켓 셧다운 내부 처리
//***************************************************************************
void CRioSession::ShutdownSocketInternal() noexcept
{
    const SOCKET socket = _socket.load(std::memory_order_acquire);

    if( socket != INVALID_SOCKET ) ::shutdown(socket, SD_BOTH);
}

//***************************************************************************
// @brief 소켓 닫기 내부 처리
//***************************************************************************
void CRioSession::CloseSocketInternal() noexcept
{
    const SOCKET socketToClose = _socket.exchange(INVALID_SOCKET, std::memory_order_acq_rel);

    CSocketUtils::Close(socketToClose);
}