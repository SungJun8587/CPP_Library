
//***************************************************************************
// IocpEvent.h : interface for the CIocpEvent class.
//
//***************************************************************************

#ifndef UC_IOCPEVENT_H
#define UC_IOCPEVENT_H

#include <Network/IOCP/IocpCommon.h>
#include <Network/NetworkRedefineDataType.h>
#include <Memory/Containers.h>

//***************************************************************************
// @class IocpEvent
// @brief Windows IOCP의 OVERLAPPED 구조체를 상속한 이벤트 기반 클래스.
//
// @details
// 핵심 설계: OVERLAPPED 상속
// WinSock 비동기 API(WSARecv, WSASend, AcceptEx 등)는 OVERLAPPED* 를 받는다.
// IocpEvent가 OVERLAPPED를 상속하므로 IocpEvent* 를 OVERLAPPED* 로 캐스팅해
// 직접 전달할 수 있다.
// 
// IOCP 완료 통지 시:
//     GetQueuedCompletionStatus → OVERLAPPED* 반환
//     reinterpret_cast<IocpEvent*>(overlapped) 로 캐스팅
//     → eventType, owner에 즉시 접근 가능
// 
// 이 덕분에 Completion Key를 사용하지 않아도 이벤트 식별 가능.
// 
// owner의 역할 — Session 소멸 안전성:
//     IocpEvent::owner 는 shared_ptr<IocpObject>.
//     I/O 등록(Register*) 시: owner = shared_from_this()  → Session ref +1
//     I/O 완료(Process*)  시: owner = nullptr              → Session ref -1
//     
//     소켓이 닫혀도 IOCP 큐에 pending된 이벤트가 완료 통지될 때까지
//     owner가 Session을 살려두므로 Use-After-Free 없음.
//***************************************************************************
class CIocpEvent : public OVERLAPPED
{
public:
    //***************************************************************************
    // @brief IocpEvent 생성자
    // @param type 이벤트 타입
    //***************************************************************************
    CIocpEvent(Iocp::EventType type);

    //***************************************************************************
    // @brief OVERLAPPED 구조체의 모든 필드를 0으로 초기화합니다.
    // 
    // @details
    // WSARecv/WSASend/AcceptEx 재사용 전에 반드시 호출해야 한다.
    // 
    // 특히 Internal 필드가 중요:
    //     Internal: 완료된 I/O의 NTSTATUS 코드
    //     DispatchBatch에서 entries[i].lpOverlapped->Internal == 0 이면 성공으로 판단
    //     이전 완료의 값이 남아있으면 성공/실패를 잘못 판단할 수 있음
    //***************************************************************************
    void            Init();

public:
    Iocp::EventType     eventType;  // I/O 종류 식별자
    CIocpObjectRef      owner;      // 이 이벤트를 소유한 Session/Listener (shared_ptr)

    // 가장 최근 완료의 Win32 에러 코드 (성공 완료는 0).
    // CIocpCore가 Dispatch 직전에 기록하며, 실패 완료는 numOfBytes = 0 으로 통지되므로
    // 소유 객체가 실패 원인(연결 리셋/취소/타임아웃 등)을 구분해야 할 때 여기서 읽는다.
    // Init()이 0으로 되돌린다.
    DWORD               errorCode = 0;
};

//***************************************************************************
// @class ConnectEvent
// @brief 클라이언트 연결 성공(ConnectEx) 완료 이벤트 클래스.
// @details 사용: Session::RegisterConnect() → ConnectEx 호출 시 / 완료: Session::ProcessConnect()
//***************************************************************************
class ConnectEvent : public CIocpEvent
{
public:
    ConnectEvent() : CIocpEvent(Iocp::EventType::Connect) {}
};

//***************************************************************************
// @class DisconnectEvent
// @brief 연결 종료(DisconnectEx) 완료 이벤트 클래스.
// @details
// 사용: Session::RegisterDisconnect() → DisconnectEx(dwFlags = 0) 호출 시
// 완료: Session::ProcessDisconnect()
// 
// dwFlags = 0:
//     TF_REUSE_SOCKET을 지정하지 않으므로 연결만 종료하고 소켓 핸들은 재사용하지 않는다.
//     소켓은 세션 소멸자가 닫는다.
//***************************************************************************
class DisconnectEvent : public CIocpEvent
{
public:
    DisconnectEvent() : CIocpEvent(Iocp::EventType::Disconnect) {}
};

//***************************************************************************
// @class AcceptEvent
// @brief 서버 클라이언트 연결 수락(AcceptEx) 완료 이벤트 클래스.
// @details
// 사용: Listener::RegisterAccept() → AcceptEx 호출 시
// 완료: Listener::ProcessAccept()
// 
// session 멤버:
//     AcceptEx는 클라이언트 소켓을 미리 만들어 전달해야 한다.
//     RegisterAccept()에서 CreateSession()으로 미리 생성한 세션을 보관하고,
//     ProcessAccept()에서 꺼내 연결 완료 처리에 사용한다.
//     
//     이 구조 덕분에 연결 완료 즉시 세션 객체가 준비되어 있어
//     accept 지연 없이 즉시 처리 가능.
//
// retryCount 멤버:
//     RegisterAccept()(세션 생성 실패/AcceptEx 즉시 실패)와 ProcessAccept()
//     (AcceptEx 실패 완료/SetUpdateAcceptContext/IOCP Register 실패) 양쪽의 실패 경로가
//     전부 이 카운터 하나를 공유해 증가시킨다. 두 경로는 서로 다른 시점(동기 재시도 vs
//     비동기 완료 통지 이후)에 실행되므로 로컬 변수로는 값을 이어받을 수 없어 이벤트에
//     상태로 들고 있는다. 재시도 지연(백오프)을 계산하는 데 쓰이며 재등록을 포기하는
//     기준은 아니다. Accept가 최종적으로 성공하면(ProcessAccept) 0으로 리셋한다.
//***************************************************************************
class AcceptEvent : public CIocpEvent
{
public:
    //***************************************************************************
        // @brief AcceptEvent 생성자
        // @details eventType을 EventType::Accept로 초기화하고 버퍼를 0으로 채웁니다.
        //***************************************************************************
    AcceptEvent() : CIocpEvent(Iocp::EventType::Accept)
    {
        ::ZeroMemory(acceptBuffer, sizeof(acceptBuffer));
    }

public:
    // AcceptEx에 전달할 미리 생성된 세션.
    // Listener::ProcessAccept()에서 static_pointer_cast<CSession>으로 캐스팅해 사용.
    CIocpObjectRef  session;

    // AcceptEx 주소 정보 파싱용 내부 버퍼 (크기: Iocp::kAcceptBufferSize)
    BYTE            acceptBuffer[Iocp::kAcceptBufferSize];

    // RegisterAccept()/ProcessAccept() 실패 경로 전체가 공유하는 연속 실패 횟수.
    // 재시도 지연(백오프) 계산에 쓰이며, 성공하면 0으로 리셋된다.
    int32           retryCount = 0;
};

//***************************************************************************
// @class RecvEvent
// @brief 데이터 수신(WSARecv) 완료 이벤트 클래스.
// @details
// 사용: Session::RegisterRecv() → WSARecv 호출 시
// 완료: Session::ProcessRecv(numOfBytes)
// 수신 버퍼는 Session의 _recvBuffer가 담당하므로 여기서 따로 보관하지 않는다.
//***************************************************************************
class RecvEvent : public CIocpEvent
{
public:
    RecvEvent() : CIocpEvent(Iocp::EventType::Recv) {}
};

//***************************************************************************
// @class SendEvent
// @brief 데이터 전송(WSASend) 완료 이벤트 클래스.
// @details
// 사용: CIocpSession::RegisterSend() → WSASend(Scatter-Gather) 호출 시
// 완료: CIocpSession::ProcessSend(numOfBytes)
// 
// sendBuffers:
//     WSASend의 Scatter-Gather 전송을 위해 WSABUF 배열을 구성할 때
//     원본 SendBuffer들의 수명을 유지하기 위한 ref count 보유자.
//     
//     흐름:
//     RegisterSend() → _sendQueue에서 전부 꺼내(swap) sendBuffers에 보관
//                    → wsaBufs 구성 후 WSASend 호출
//     ProcessSend()  → 전송 완료 시 Reset()
//                    → SendBuffer ref 해제
//                    → SendBufferChunk ref count 감소 → 풀 반환
//     
//     이 벡터가 없으면:
//         WSASend pending 중에 SendBuffer가 소멸 → WSABUF의 buf 포인터가 댕글링
//
// wsaBufs:
//     WSASend에 전달할 Scatter-Gather 버퍼 배열. SendEvent가 보유하므로 매 전송마다
//     지역 CVector<WSABUF>를 새로 만들지 않는다. 다음 RegisterSend()는 이 WSASend의
//     완료 통지(ProcessSend) 이후에만 실행되므로 완료 전에는 변경되지 않는다.
//     Reset()은 capacity를 유지한 채 clear()하므로 반복되는 send에서 재할당 비용을 줄인다.
//***************************************************************************
class SendEvent : public CIocpEvent
{
public:
    SendEvent() : CIocpEvent(Iocp::EventType::Send) {}

    //***************************************************************************
    // @brief 현재 SendEvent의 전송 상태를 초기화합니다.
    //
    // @details
    // 이 함수는 해당 WSASend가 더 이상 pending 상태가 아닐 때만 호출해야 한다.
    // sendBuffers.clear()/wsaBufs.clear()는 capacity를 유지하므로 다음
    // Scatter-Gather 전송에서 재할당을 줄일 수 있다.
    //***************************************************************************
    void Reset()
    {
        sendBuffers.clear();
        wsaBufs.clear();
    }

public:
    // WSASend pending 중 SendBuffer 수명 보장. ProcessSend 완료 후 Reset()으로 clear.
    CVector<CSendBufferRef>   sendBuffers;

    // WSASend에 전달할 Scatter-Gather 버퍼.
    CVector<WSABUF>           wsaBufs;
};

#endif // ndef UC_IOCPEVENT_H