
//***************************************************************************
// IocpCore.h : interface for the IocpCore class.
//
//***************************************************************************

#ifndef UC_IOCPCORE_H
#define UC_IOCPCORE_H

#include <Network/IOCP/IocpCommon.h>
#include <Network/IOCP/IocpEvent.h>

#pragma comment(lib, "ntdll.lib")

//***************************************************************************
// @class CIocpObject
// @brief IOCP에 등록할 수 있는 모든 객체의 추상 기반 클래스.
//
// @details
// 역할:
//     IOCP에 등록할 수 있는 모든 객체의 추상 기반 클래스.
//     현재 구현체: Session, Listener
//     
// GetHandle():
//     IOCP에 등록할 소켓 핸들을 반환한다.
//     CreateIoCompletionPort의 첫 번째 인자로 전달된다.
//     
// Dispatch():
//     IOCP 완료 통지가 왔을 때 CIocpCore가 호출하는 가상 함수.
//     각 구현체(Session/Listener)가 이벤트 타입에 따라 처리를 분기한다.
//     
//     numOfBytes = 0:
//         정상적인 연결 끊김 또는 에러 → Disconnect 처리 유도
//         
//***************************************************************************
class CIocpObject
{
public:
    //***************************************************************************
    // @brief 가상 소멸자.
    // @details CIocpObjectRef(shared_ptr<CIocpObject>)가 어떤 경로로 생성되더라도
    //          파생 객체가 올바르게 소멸되도록 보장합니다.
    //***************************************************************************
    virtual ~CIocpObject() = default;

    //***************************************************************************
    // @brief IOCP에 등록할 소켓 핸들을 반환합니다.
    // @return HANDLE 소켓 핸들
    //***************************************************************************
    virtual HANDLE  GetHandle() abstract;

    //***************************************************************************
    // @brief IOCP 완료 통지가 왔을 때 IocpCore가 호출하는 가상 함수입니다.
    // @param iocpEvent 완료된 IOCP 이벤트 포인터
    // @param numOfBytes 전송된 바이트 수 (0인 경우 정상 연결 끊김 또는 에러)
    // @note 실패한 완료는 numOfBytes = 0 으로 통지되며, 원인 Win32 에러 코드는
    //       iocpEvent->errorCode 로 조회합니다.
    //***************************************************************************
    virtual void    Dispatch(class CIocpEvent* iocpEvent, int32 numOfBytes = 0) abstract;

    virtual CIocpObjectRef GetIocpObjectPtr() = 0;
};

//***************************************************************************
// @class CIocpCore
// @brief IOCP 커널 오브젝트를 관리하고 워커 스레드에서 완료 이벤트를 처리하는 핵심 클래스.
//
// @details
// 역할:
//     IOCP 커널 오브젝트를 생성/관리하고, 워커 스레드에서 완료 이벤트를 꺼내
//     해당 IocpObject의 Dispatch()를 호출하는 핵심 클래스.
//     
// IOCP 동작 원리:
//     1. CreateIoCompletionPort로 IOCP 핸들 생성
//     2. Register()로 소켓을 IOCP에 연결 (소켓 하나에 IOCP 하나만 연결 가능)
//     3. WSARecv/WSASend 등 비동기 I/O 요청 → I/O 완료 시 IOCP 큐에 적재
//     4. 워커 스레드에서 Dispatch()/DispatchBatch()로 완료 이벤트 꺼내 처리
//     
// 스레드 안전성:
//     GetQueuedCompletionStatus(Ex)는 내부적으로 스레드 안전하다.
//     여러 워커 스레드가 동시에 Dispatch()를 호출해도 각자 다른 이벤트를 처리한다.
//     이것이 IOCP가 고성능 멀티스레드 서버의 기반이 되는 이유다.
//
// [설계 요약]
//     ① DispatchBatch() (GetQueuedCompletionStatusEx)
//        - 1회 syscall로 최대 kBatchSize(64)개 이벤트 처리
//        - 큐가 계속 깊은 이론상 최선: 10,000 이벤트/초 → 약 157 syscall/초
//          (실제 이득은 부하 패턴과 워커 수에 따라 달라지므로 측정 기반으로 조정)
//     ② 실패 완료는 numOfBytes = 0 으로 통지하고, 원인 에러 코드는 CIocpEvent::errorCode로 전달
//     ③ ProcessOverlappedResult()가 Dispatch/DispatchBatch의 공통 완료 처리를 담당
//     ④ 종료: PostQuit()은 워커 1개당 1회 게시. Dispatch()는 false, DispatchBatch()는 Iocp::kDispatchQuit(-1)로 종료를 알림.
//        IOCP 핸들 무효/폐쇄처럼 복구 불가능한 수거 실패는 DispatchBatch()가 Iocp::kDispatchFatal(-2)로 알림
//***************************************************************************
class CIocpCore
{
public:
    CIocpCore();
    ~CIocpCore();

    // IOCP 핸들을 소유하므로 복사를 금지한다 — 복사본이 소멸하며 같은 핸들을 두 번 닫게 된다.
    // 공유가 필요하면 CIocpCoreRef(shared_ptr)를 사용한다.
    CIocpCore(const CIocpCore&) = delete;
    CIocpCore& operator=(const CIocpCore&) = delete;

    //***************************************************************************
    // @brief 워커 스레드 종료를 위한 Quit 이벤트 패킷을 IOCP 큐에 게시합니다.
    // @return bool 게시 성공 여부
    // @details
    // QUIT_KEY를 완료 키로 전달하여 IOCP 큐에 종료 이벤트를 등록합니다.
    // 워커 스레드는 큐에 대기 중인 잔여 I/O 패킷들을 순차적으로 모두 처리한 후,
    // 이 패킷을 수거하는 시점에 안전하게 스레드 루프를 탈출합니다.
    // 수거 시 Dispatch()는 false, DispatchBatch()는 -1을 반환합니다.
    // 워커 N개를 멈추려면 N회 호출합니다. 한 번의 배치에서 여러 개가 수거되면
    // 초과분은 DispatchBatch()가 다시 게시하므로 다른 워커가 받을 수 있습니다.
    //***************************************************************************
    bool PostQuit()
    {
        return ::PostQueuedCompletionStatus(_iocpHandle, 0, Iocp::QUIT_KEY, nullptr) != FALSE;
    }

    //***************************************************************************
    // @brief 관리 중인 IOCP 핸들을 반환합니다.
    // @return HANDLE IOCP 핸들
    //***************************************************************************
    HANDLE  GetHandle() { return _iocpHandle; }

    //***************************************************************************
    // @brief IocpObject(Session/Listener)의 소켓을 IOCP에 연결합니다.
    // @param iocpObject 등록할 IOCP 오브젝트
    // @return true 성공, false 실패
    // 
    // @details
    // 이후 해당 소켓의 모든 비동기 I/O 완료가 이 IOCP로 모인다.
    // 
    // 내부적으로 CreateIoCompletionPort를 두 번 호출하는 셈:
    // - 생성자: CreateIoCompletionPort(INVALID_HANDLE_VALUE, ...) → IOCP 생성
    // - Register: CreateIoCompletionPort(socket, iocpHandle, ...) → 소켓 연결
    // 
    // Completion Key = 0:
    //     완료 통지에 포함되는 사용자 정의 값.
    //     이 라이브러리는 Completion Key 대신 IocpEvent::owner 포인터로
    //     객체를 식별하므로 0으로 고정해도 무방하다.
    //***************************************************************************
    bool    Register(const CIocpObjectRef& iocpObject);

    //***************************************************************************
    // @brief 단건 GQCS 방식으로 완료 이벤트를 1개 꺼내 처리합니다.
    // @param timeoutMs 대기 시간 (기본값: INFINITE)
    // @return true 이벤트 처리 완료, false 처리한 이벤트 없음
    // 
    // @details
    // GetQueuedCompletionStatus로 완료 이벤트 1개를 꺼낸다.
    // timeoutMs = INFINITE: 이벤트가 올 때까지 무한 대기.
    // timeoutMs = N: N밀리초 대기 후 타임아웃 반환.
    // 
    // 반환 false: 처리한 완료 이벤트 없음 (타임아웃, QUIT 신호, 큐 수거 자체 실패)
    // 반환 true:  완료 이벤트 1개 처리 (I/O 성공 또는 실패 완료)
    // 
    // 저부하 환경이나 단순 구조에서 충분하다.
    //***************************************************************************
    bool    Dispatch(uint32 timeoutMs = INFINITE);

    //***************************************************************************
    // @brief 배치 GQCSEx 방식으로 최대 BATCH_SIZE개의 완료 이벤트를 수거해 처리합니다.
    // @param timeoutMs 대기 시간 (기본값: INFINITE)
    // @return int32 실제 디스패치한 I/O 완료 항목 수. 0은 "처리한 I/O 완료가 없음"을 뜻하며,
    //         타임아웃뿐 아니라 사용자 게시 패킷(PostQueuedCompletionStatus로 넣은 wake-up 등)만
    //         수거한 경우도 포함한다 — 0만 보고 타임아웃이었다고 단정해서는 안 된다.
    //         QUIT 패킷을 수거했다면 나머지 이벤트를 모두 처리한 뒤 Iocp::kDispatchQuit(-1),
    //         복구 불가능한 수거 실패(ERROR_INVALID_HANDLE/ERROR_ABANDONED_WAIT_0)면 Iocp::kDispatchFatal(-2).
    //         그 밖의 일시적 수거 실패는 로그와 짧은 대기 후 0을 반환합니다.
    // 
    // @details
    // GetQueuedCompletionStatusEx로 최대 BATCH_SIZE개의 완료 이벤트를
    // 1회 syscall로 수거한다.
    // 
    // 성능 근거:
    //     syscall은 유저 모드 → 커널 모드 전환 비용(수백 ns)이 발생.
    //     GQCS:   10,000 이벤트/초 → 10,000 syscall/초
    //     GQCSEx: 10,000 이벤트/초 → 약 157 syscall/초 (BATCH=64 기준)
    // 
    // 권장 워커 스레드 루프:
    //     while (true) {
    //         GThreadManager->DistributeReservedJobs();
    //         int32 processed = iocpCore->DispatchBatch(10);
    //         if (processed == 0)
    //             GThreadManager->DoGlobalQueueWork();
    //     }
    //***************************************************************************
    int32   DispatchBatch(uint32 timeoutMs = INFINITE);

private:
    //***************************************************************************
    // @brief Dispatch / DispatchBatch 양쪽에서 공통으로 사용하는 완료 이벤트 처리 함수.
    // @param success I/O 성공 여부
    // @param iocpEvent 완료된 IOCP 이벤트 포인터
    // @param numOfBytes 전송된 바이트 수
    // @param errorCode 에러 코드
    // 
    // @details
    // 성공(success == TRUE):
    //     iocpObject->Dispatch(iocpEvent, numOfBytes) 호출
    //     
    // 실패(success == FALSE):
    //     iocpEvent->errorCode에 에러 코드를 기록한 뒤 Dispatch(iocpEvent, 0)을 호출한다.
    //     실패한 완료와 성공한 Recv의 numOfBytes == 0(상대의 graceful shutdown, FIN)은 둘 다 0으로
    //     전달되므로 구분이 필요하면 iocpEvent->errorCode를 본다. 연결 종료 정책은 Session 계층이
    //     결정하며(Recv/Send 완료 경로에서 Disconnect를 유도), Listener/Connect 경로는 자체 실패
    //     처리를 수행한다.
    //     완료 통지가 owner 참조를 해제하는 유일한 경로이므로 실패 완료도 반드시 Dispatch한다.
    //     
    //     정상적인 연결 종료·취소로 간주하는 에러 (로그 생략):
    //     - ERROR_NETNAME_DELETED / ERROR_CONNECTION_ABORTED / WSAECONNRESET / WSAECONNABORTED:
    //       원격지 강제 종료, TCP RST
    //     - ERROR_OPERATION_ABORTED: 소켓 Close/Disconnect로 인한 pending I/O 취소
    //     - ERROR_SEM_TIMEOUT: keepalive 타임아웃
    //     그 외 에러는 LOG_ERROR로 기록한다. 단, Connect 이벤트는 실패가 일상적이므로
    //     Session 계층(ProcessConnectEx/FailConnect)의 처리에 맡기고 로그를 생략한다.
    //***************************************************************************
    void    ProcessOverlappedResult(BOOL success, CIocpEvent* iocpEvent,
        DWORD numOfBytes, DWORD errorCode);

private:
    HANDLE  _iocpHandle;    // CreateIoCompletionPort로 생성된 IOCP 커널 오브젝트
};

#endif // ndef UC_IOCPCORE_H