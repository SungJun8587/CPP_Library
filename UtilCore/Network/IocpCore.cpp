#include "pch.h"
#include "IocpCore.h"
#include "IocpEvent.h"

/*==============================================================================
    IocpCore
==============================================================================*/

IocpCore::IocpCore()
{
    _iocpHandle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
    ASSERT_CRASH(_iocpHandle != INVALID_HANDLE_VALUE);
}

IocpCore::~IocpCore()
{
    ::CloseHandle(_iocpHandle);
}

bool IocpCore::Register(IocpObjectRef iocpObject)
{
    return ::CreateIoCompletionPort(iocpObject->GetHandle(), _iocpHandle, 0, 0) != NULL;
}

/*------------------------------------------------------------------------------
    Dispatch — 단건 GQCS (기존 유지, 하위 호환)
    저부하 / 단순 구조에서는 이것으로 충분
------------------------------------------------------------------------------*/
bool IocpCore::Dispatch(uint32 timeoutMs)
{
    DWORD       numOfBytes  = 0;
    ULONG_PTR   key         = 0;
    IocpEvent*  iocpEvent   = nullptr;

    BOOL success = ::GetQueuedCompletionStatus(
        _iocpHandle,
        OUT &numOfBytes,
        OUT &key,
        OUT reinterpret_cast<LPOVERLAPPED*>(&iocpEvent),
        timeoutMs
    );

    DWORD errorCode = success ? 0 : ::WSAGetLastError();
    ProcessOverlappedResult(success, iocpEvent, numOfBytes, errorCode);

    return (errorCode != WAIT_TIMEOUT);
}

/*------------------------------------------------------------------------------
    [추가] DispatchBatch — 배치 GQCSEx

    성능 근거:
      GQCS  : 완료 이벤트 1개당 syscall 1회 → 10,000 패킷/초 = 10,000 syscall/초
      GQCSEx: 완료 이벤트 최대 64개당 syscall 1회 → 동일 부하 = ~156 syscall/초

    워커 스레드 루프 권장 패턴:
      while (true) {
          GThreadManager->DistributeReservedJobs();
          int32 count = iocpCore->DispatchBatch(10);  // 10ms 타임아웃
          if (count == 0) GThreadManager->DoGlobalQueueWork();
      }
------------------------------------------------------------------------------*/
int32 IocpCore::DispatchBatch(uint32 timeoutMs)
{
    OVERLAPPED_ENTRY    entries[BATCH_SIZE];
    ULONG               numRemoved = 0;

    BOOL success = ::GetQueuedCompletionStatusEx(
        _iocpHandle,
        entries,
        BATCH_SIZE,
        OUT &numRemoved,
        timeoutMs,
        FALSE   // alertable I/O 미사용 (APC 콜백 불필요)
    );

    if (!success)
    {
        DWORD err = ::GetLastError();
        if (err == WAIT_TIMEOUT)
            return 0;
        // GQCSEx 자체 실패 (핸들 무효 등 시스템 레벨 오류)
        return 0;
    }

    for (ULONG i = 0; i < numRemoved; i++)
    {
        IocpEvent*  iocpEvent   = reinterpret_cast<IocpEvent*>(entries[i].lpOverlapped);
        DWORD       numOfBytes  = entries[i].dwNumberOfBytesTransferred;

        // GQCSEx에서 개별 완료 성공/실패는 OVERLAPPED::Internal로 판단
        // STATUS_SUCCESS(0) 이면 성공
        BOOL  entrySuccess  = (entries[i].lpOverlapped->Internal == 0);
        DWORD errorCode     = entrySuccess
            ? 0
            : static_cast<DWORD>(::RtlNtStatusToDosError(
                static_cast<NTSTATUS>(entries[i].lpOverlapped->Internal)));

        ProcessOverlappedResult(entrySuccess, iocpEvent, numOfBytes, errorCode);
    }

    return static_cast<int32>(numRemoved);
}

/*------------------------------------------------------------------------------
    ProcessOverlappedResult — Dispatch/DispatchBatch 공통 결과 처리

    [개선] 기존 default: TODO → 에러 코드 분류 실제 구현
      정상 연결 끊김 에러들은 Dispatch(numOfBytes=0)으로 세션 종료 유도
      → Session::ProcessRecv/Send에서 numOfBytes==0 감지 후 Disconnect 호출
------------------------------------------------------------------------------*/
void IocpCore::ProcessOverlappedResult(BOOL success, IocpEvent* iocpEvent,
                                        DWORD numOfBytes, DWORD errorCode)
{
    if (iocpEvent == nullptr)
        return;

    IocpObjectRef iocpObject = iocpEvent->owner;
    if (iocpObject == nullptr)
        return;

    if (success)
    {
        iocpObject->Dispatch(iocpEvent, static_cast<int32>(numOfBytes));
        return;
    }

    switch (errorCode)
    {
    case WAIT_TIMEOUT:
        // 타임아웃: 처리 대상 아님
        break;

    case ERROR_NETNAME_DELETED:     // 원격지 강제 종료 (FIN 없이 연결 끊김)
    case ERROR_CONNECTION_RESET:    // TCP RST 수신
    case ERROR_OPERATION_ABORTED:   // 소켓 Close로 인한 pending I/O 취소
    case ERROR_SEM_TIMEOUT:         // keepalive 타임아웃
        // 정상적인 연결 끊김 → numOfBytes=0으로 Dispatch → Session 정상 종료 흐름 탐
        iocpObject->Dispatch(iocpEvent, 0);
        break;

    default:
        // 예상치 못한 에러 → 동일하게 세션 종료 유도
        // TODO: GConsoleLogger->WriteStdErr("IOCP error: %u", errorCode);
        iocpObject->Dispatch(iocpEvent, 0);
        break;
    }
}
