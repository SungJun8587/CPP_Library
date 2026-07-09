#pragma once

class Session;

/*==============================================================================
    EventType
    
    IocpEvent가 어떤 종류의 비동기 I/O인지 식별하는 열거형.
    Session::Dispatch에서 switch문으로 분기해 적절한 Process 함수를 호출한다.
==============================================================================*/
enum class EventType : uint8
{
    Connect,        // ConnectEx 완료 (클라이언트 → 서버 연결 성공)
    Disconnect,     // DisconnectEx 완료 (연결 종료 및 소켓 초기화 완료)
    Accept,         // AcceptEx 완료 (서버 → 클라이언트 연결 수락)
    Recv,           // WSARecv 완료 (데이터 수신)
    Send,           // WSASend 완료 (데이터 전송)
};

/*==============================================================================
    IocpEvent
    
    역할:
        Windows IOCP의 OVERLAPPED 구조체를 상속한 이벤트 기반 클래스.
        
    핵심 설계: OVERLAPPED 상속
        WinSock 비동기 API(WSARecv, WSASend, AcceptEx 등)는 OVERLAPPED* 를 받는다.
        IocpEvent가 OVERLAPPED를 상속하므로 IocpEvent* 를 OVERLAPPED* 로 캐스팅해
        직접 전달할 수 있다.
        
        IOCP 완료 통지 시:
            GetQueuedCompletionStatus → OVERLAPPED* 반환
            reinterpret_cast<IocpEvent*>(overlapped) 로 캐스팅
            → eventType, owner에 즉시 접근 가능
        
        이 덕분에 Completion Key를 사용하지 않아도 이벤트 식별 가능.

    owner의 역할 — Session 소멸 안전성:
        IocpEvent::owner 는 shared_ptr<IocpObject>.
        I/O 등록(Register*) 시: owner = shared_from_this()  → Session ref +1
        I/O 완료(Process*)  시: owner = nullptr              → Session ref -1
        
        소켓이 닫혀도 IOCP 큐에 pending된 이벤트가 완료 통지될 때까지
        owner가 Session을 살려두므로 Use-After-Free 없음.
        
        예시:
            RegisterRecv() → _recvEvent.owner = shared_from_this() (ref = 2)
            Session이 다른 곳에서 ref 해제되어도 (ref = 1)
            ProcessRecv() 완료 후 owner = nullptr 에서 마지막 소멸 (ref = 0)
==============================================================================*/
class IocpEvent : public OVERLAPPED
{
public:
    IocpEvent(EventType type);

    /*--------------------------------------------------------------------------
        Init
        
        OVERLAPPED 구조체의 모든 필드를 0으로 초기화한다.
        WSARecv/WSASend/AcceptEx 재사용 전에 반드시 호출해야 한다.
        
        특히 Internal 필드가 중요:
            Internal: 완료된 I/O의 NTSTATUS 코드
            DispatchBatch에서 entries[i].lpOverlapped->Internal == 0 이면 성공으로 판단
            이전 완료의 값이 남아있으면 성공/실패를 잘못 판단할 수 있음
    --------------------------------------------------------------------------*/
    void            Init();

public:
    EventType       eventType;  // I/O 종류 식별자
    IocpObjectRef   owner;      // 이 이벤트를 소유한 Session/Listener (shared_ptr)
};

/*==============================================================================
    ConnectEvent
    
    사용: Session::RegisterConnect() → ConnectEx 호출 시
    완료: Session::ProcessConnect()
    
    추가 멤버 없음. eventType만으로 충분.
==============================================================================*/
class ConnectEvent : public IocpEvent
{
public:
    ConnectEvent() : IocpEvent(EventType::Connect) { }
};

/*==============================================================================
    DisconnectEvent
    
    사용: Session::RegisterDisconnect() → DisconnectEx(TF_REUSE_SOCKET) 호출 시
    완료: Session::ProcessDisconnect()
    
    TF_REUSE_SOCKET:
        소켓을 닫지 않고 연결만 끊은 후 재사용 가능 상태로 초기화.
        AcceptEx 풀(Listener)에서 소켓을 재사용하는 구조에 필수.
==============================================================================*/
class DisconnectEvent : public IocpEvent
{
public:
    DisconnectEvent() : IocpEvent(EventType::Disconnect) { }
};

/*==============================================================================
    AcceptEvent
    
    사용: Listener::RegisterAccept() → AcceptEx 호출 시
    완료: Listener::ProcessAccept()
    
    session 멤버:
        AcceptEx는 클라이언트 소켓을 미리 만들어 전달해야 한다.
        RegisterAccept()에서 CreateSession()으로 미리 생성한 세션을 보관하고,
        ProcessAccept()에서 꺼내 연결 완료 처리에 사용한다.
        
        이 구조 덕분에 연결 완료 즉시 세션 객체가 준비되어 있어
        accept 지연 없이 즉시 처리 가능.
==============================================================================*/
class AcceptEvent : public IocpEvent
{
public:
    AcceptEvent() : IocpEvent(EventType::Accept) { }
public:
    SessionRef  session = nullptr;  // 연결 수락을 위해 미리 준비된 세션
};

/*==============================================================================
    RecvEvent
    
    사용: Session::RegisterRecv() → WSARecv 호출 시
    완료: Session::ProcessRecv(numOfBytes)
    
    추가 멤버 없음.
    수신 버퍼는 Session의 _recvBuffer가 담당하므로 여기서 따로 보관하지 않는다.
==============================================================================*/
class RecvEvent : public IocpEvent
{
public:
    RecvEvent() : IocpEvent(EventType::Recv) { }
};

/*==============================================================================
    SendEvent
    
    사용: Session::RegisterSend() → WSASend(Scatter-Gather) 호출 시
    완료: Session::ProcessSend(numOfBytes)
    
    sendBuffers:
        WSASend의 Scatter-Gather 전송을 위해 WSABUF 배열을 구성할 때
        원본 SendBuffer들의 수명을 유지하기 위한 ref count 보유자.
        
        흐름:
        RegisterSend() → _sendQueue에서 전부 꺼내 sendBuffers에 보관
                       → WSABUF 배열 구성 후 WSASend 호출
        ProcessSend()  → sendBuffers.clear() → SendBuffer ref 해제
                       → SendBufferChunk ref count 감소 → 풀 반환
        
        이 벡터가 없으면:
            WSASend pending 중에 SendBuffer가 소멸 → WSABUF의 buf 포인터가 댕글링
==============================================================================*/
class SendEvent : public IocpEvent
{
public:
    SendEvent() : IocpEvent(EventType::Send) { }
public:
    // WSASend pending 중 SendBuffer 수명 보장. ProcessSend 완료 후 clear()
    Vector<SendBufferRef>   sendBuffers;
};
