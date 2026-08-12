
//***************************************************************************
// IocpCore.h : interface for the IocpCore class.
//
//***************************************************************************

#ifndef __IOCPCORE_H__
#define __IOCPCORE_H__

#ifndef	__IOCPCOMMON_H__
#include <Network/IOCP/IocpCommon.h>
#endif

#ifndef	__IOCPEVENT_H__
#include <Network/IOCP/IocpEvent.h>
#endif

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
// enable_shared_from_this:
//     IocpEvent::owner에 shared_from_this()로 자신을 등록하기 위해 필요.
//     이를 통해 I/O가 완료될 때까지 객체 수명이 보장된다.
//***************************************************************************
class CIocpObject : public enable_shared_from_this<CIocpObject>
{
public:
    //***************************************************************************
    // @brief IOCP에 등록할 소켓 핸들을 반환합니다.
    // @return HANDLE 소켓 핸들
    //***************************************************************************
    virtual HANDLE  GetHandle() abstract;

    //***************************************************************************
    // @brief IOCP 완료 통지가 왔을 때 IocpCore가 호출하는 가상 함수입니다.
    // @param iocpEvent 완료된 IOCP 이벤트 포인터
    // @param numOfBytes 전송된 바이트 수 (0인 경우 정상 연결 끊김 또는 에러)
    //***************************************************************************
    virtual void    Dispatch(class CIocpEvent* iocpEvent, int32 numOfBytes = 0) abstract;
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
// [성능 개선]
//     ① DispatchBatch() 추가 (GetQueuedCompletionStatusEx)
//        - 1회 syscall로 최대 64개 이벤트 처리
//        - 10,000 이벤트/초 기준: syscall 10,000회 → 157회
//     ② 에러 처리 완성 (기존 default: TODO → 실제 분기)
//     ③ ProcessOverlappedResult() 추출 (Dispatch/DispatchBatch 공통 처리)
//***************************************************************************
class CIocpCore
{
public:
    CIocpCore();
    ~CIocpCore();

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
    bool    Register(CIocpObjectRef iocpObject);

    //***************************************************************************
    // @brief 단건 GQCS 방식으로 완료 이벤트를 1개 꺼내 처리합니다.
    // @param timeoutMs 대기 시간 (기본값: INFINITE)
    // @return true 이벤트 처리 완료, false 타임아웃
    // 
    // @details
    // GetQueuedCompletionStatus로 완료 이벤트 1개를 꺼낸다.
    // timeoutMs = INFINITE: 이벤트가 올 때까지 무한 대기.
    // timeoutMs = N: N밀리초 대기 후 타임아웃 반환.
    // 
    // 반환 false: 타임아웃 (이벤트 없음)
    // 반환 true:  이벤트 처리 완료 (성공 또는 연결 끊김 에러)
    // 
    // 저부하 환경이나 단순 구조에서 충분하다.
    //***************************************************************************
    bool    Dispatch(uint32 timeoutMs = INFINITE);

    //***************************************************************************
    // @brief 배치 GQCSEx 방식으로 최대 BATCH_SIZE개의 완료 이벤트를 수거해 처리합니다.
    // @param timeoutMs 대기 시간 (기본값: INFINITE)
    // @return int32 실제 처리한 이벤트 수
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
    //     iocpObject->Dispatch(iocpEvent, numOfBytes) 직접 호출
    //     
    // 실패(success == FALSE):
    //     에러 코드에 따라 분류:
    //     - WAIT_TIMEOUT: 처리 없음
    //     - ERROR_NETNAME_DELETED:  원격지 강제 종료 (프로세스 kill 등)
    //     - ERROR_CONNECTION_RESET: TCP RST 수신 (연결 재설정)
    //     - ERROR_OPERATION_ABORTED: 소켓 Close로 인한 I/O 취소
    //     - ERROR_SEM_TIMEOUT:       keepalive 타임아웃
    //     → 위 4가지는 정상적인 연결 끊김으로 처리:
    //       Dispatch(iocpEvent, 0) → Session::ProcessRecv/Send에서 Disconnect 유도
    //     - 그 외: 로그 후 동일하게 Dispatch(0)
    //     
    //     [기존 문제]
    //     에러 처리가 없으면 연결 끊김 이벤트가 세션 종료로 이어지지 않아
    //     좀비 세션이 누적된다.
    //***************************************************************************
    void    ProcessOverlappedResult(BOOL success, CIocpEvent* iocpEvent,
        DWORD numOfBytes, DWORD errorCode);

private:
    HANDLE  _iocpHandle;    // CreateIoCompletionPort로 생성된 IOCP 커널 오브젝트
};

#endif // ndef __IOCPCORE_H__
