
//***************************************************************************
// IocpCore.cpp: implementation of the CIocpCore class.
//
//***************************************************************************

#include "pch.h"
#include "IocpCore.h"

#include <unordered_map>
#include <utility>

namespace
{
    //***************************************************************************
    // @brief 같은 종류의 오류 로그가 폭주하지 않도록 첫 건과 이 횟수마다만 남기는 샘플링 간격.
    //***************************************************************************
    constexpr uint32 kLogSampleInterval = 100;

    //***************************************************************************
    // @brief 카운터를 1 증가시키고, 이번 발생을 로그로 남길지 판단하는 샘플러.
    // @param count 발생 횟수 카운터 (호출 후 이번 발생까지 포함한 횟수가 된다)
    // @param interval 샘플링 간격 (기본값: kLogSampleInterval)
    // @return 첫 발생과 interval회마다 true
    //***************************************************************************
    inline bool ShouldLogSampled(uint32& count, uint32 interval = kLogSampleInterval) noexcept
    {
        return (count++ % interval) == 0;
    }

    //***************************************************************************
    // @brief 정상적인 연결 종료·취소로 간주하여 로그를 남기지 않는 에러인지 판별합니다.
    // @param errorCode Win32 에러 코드
    //***************************************************************************
    bool IsExpectedDisconnectError(DWORD errorCode)
    {
        switch( errorCode )
        {
        case ERROR_NETNAME_DELETED:         // 원격지 강제 종료 (FIN 없이 연결 끊김)
        case ERROR_CONNECTION_ABORTED:      // 로컬/원격 연결 중단
        case ERROR_OPERATION_ABORTED:       // 소켓 Close/Disconnect로 인한 pending I/O 취소
        case ERROR_SEM_TIMEOUT:             // keepalive 타임아웃
        case WSAECONNRESET:                 // TCP RST 수신
        case WSAECONNABORTED:
            return true;

        default:
            return false;
        }
    }

    //***************************************************************************
    // @brief 정상 종료로 분류되지 않은 완료 에러를 로그로 남길지 판단합니다 (스레드별, 에러 코드별 샘플링).
    // @param errorCode Win32 에러 코드
    // @param occurrence 이 스레드에서 이 에러 코드가 발생한 누적 횟수(이번 포함)를 반환
    // @details 같은 에러가 고부하에서 반복되면 완료 경로의 로그가 네트워크 처리보다 비싸지므로,
    //          에러 코드별로 첫 발생과 kLogSampleInterval회마다만 남긴다. 서로 다른 에러는
    //          각각 첫 발생이 바로 기록되어 묻히지 않는다.
    //***************************************************************************
    bool ShouldLogCompletionError(DWORD errorCode, uint32& occurrence)
    {
        thread_local std::unordered_map<DWORD, uint32> counts;
        uint32& count = counts[errorCode];
        const bool log = ShouldLogSampled(count);
        occurrence = count;
        return log;
    }

    //***************************************************************************
    // @brief 완료 큐 수거 실패가 복구 불가능한 에러인지 판별합니다.
    // @param errorCode Win32 에러 코드
    // @details IOCP 핸들이 무효이거나 이미 닫힌 경우만 해당한다. 이 상태에서는 몇 번을 재시도해도
    //          수거에 성공할 수 없으므로 워커가 루프를 종료해야 한다. 그 밖의 에러는 일시적일 수
    //          있어 워커를 영구히 잃지 않도록 치명 오류로 분류하지 않는다.
    //***************************************************************************
    bool IsFatalDequeueError(DWORD errorCode)
    {
        return errorCode == ERROR_INVALID_HANDLE || errorCode == ERROR_ABANDONED_WAIT_0;
    }

    //***************************************************************************
    // @brief 완료 큐 수거 API 자체의 실패(핸들 무효 등)를 기록하고 잠시 대기합니다.
    // @param apiName 실패한 API 이름
    // @param errorCode Win32 에러 코드
    // @details 복구 불가능한 상태에서 워커 루프가 대기 없이 재호출되며
    //          CPU를 점유(spin)하지 않도록 짧게 양보합니다. 로그는 샘플링합니다.
    //          복구 불가능한 에러(IsFatalDequeueError)는 이 함수를 거치지 않고 호출 측이 워커를
    //          종료하므로, 여기서는 일시적일 수 있는 실패만 다룹니다.
    //***************************************************************************
    void ReportDequeueFailure(const TCHAR* apiName, DWORD errorCode)
    {
        thread_local uint32 failureCount = 0;
        if( ShouldLogSampled(failureCount) )
        {
            LOG_ERROR(_T("[CIocpCore] %s failed: error=%lu (occurrence %u on this thread)"),
                apiName, errorCode, failureCount);
        }
        ::Sleep(Iocp::kDequeueFailureBackoffMs);
    }
}

//***************************************************************************
// @brief CIocpCore 생성자
// @details IOCP 커널 오브젝트를 생성합니다. CreateIoCompletionPort는 실패 시
//          INVALID_HANDLE_VALUE가 아니라 NULL을 반환하므로 NULL로 판정하며,
//          생성에 실패하면 원인을 로그로 남기고 즉시 종료(fail-fast)합니다.
//***************************************************************************
CIocpCore::CIocpCore()
{
    _iocpHandle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if( _iocpHandle == NULL )
    {
        LOG_ERROR(_T("[CIocpCore] CreateIoCompletionPort failed: error=%lu"), ::GetLastError());
        ASSERT_CRASH(_iocpHandle != NULL);
    }
}

//***************************************************************************
// @brief CIocpCore 소멸자
// @details 생성된 IOCP 핸들을 닫습니다. 이 객체를 참조하는 모든 워커 스레드가 종료된 뒤에
//          호출돼야 합니다(서비스는 워커를 join한 뒤에야 코어 참조를 놓습니다).
//***************************************************************************
CIocpCore::~CIocpCore()
{
    if( _iocpHandle != NULL )
    {
        ::CloseHandle(_iocpHandle);
        _iocpHandle = NULL;
    }
}

//***************************************************************************
// @brief 소켓을 IOCP에 등록합니다.
// @param iocpObject 등록할 IOCP 오브젝트
// @return true 성공, false 실패 (null 오브젝트 포함)
// @details 실패 원인(Win32 에러 코드)은 로그로 남긴다(샘플링). CreateIoCompletionPort는
//          실패 시 NULL을 반환한다.
//***************************************************************************
bool CIocpCore::Register(const CIocpObjectRef& iocpObject)
{
    if( iocpObject == nullptr )
        return false;

    HANDLE result = ::CreateIoCompletionPort(iocpObject->GetHandle(), _iocpHandle, 0, 0);
    if( result == NULL )
    {
        const DWORD errorCode = ::GetLastError();

        thread_local uint32 failureCount = 0;
        if( ShouldLogSampled(failureCount) )
        {
            LOG_ERROR(_T("[CIocpCore] CreateIoCompletionPort failed: error=%lu (occurrence %u on this thread)"),
                errorCode, failureCount);
        }
        return false;
    }

    return true;
}

//***************************************************************************
// @brief 단건 GQCS 방식으로 완료 이벤트를 처리합니다.
// @param timeoutMs 대기 시간 (밀리초)
// @return true 완료 이벤트 1개 처리, false 처리한 이벤트 없음
//         (타임아웃 / QUIT 신호 / 큐 수거 자체 실패)
// @note 저부하 / 단순 구조에서는 이것으로 충분합니다.
//***************************************************************************
bool CIocpCore::Dispatch(uint32 timeoutMs)
{
    DWORD        numOfBytes = 0;
    ULONG_PTR    key = 0;
    LPOVERLAPPED overlapped = nullptr;

    BOOL success = ::GetQueuedCompletionStatus(
        _iocpHandle,
        OUT & numOfBytes,
        OUT & key,
        OUT & overlapped,
        timeoutMs
    );

    const DWORD errorCode = success ? 0 : ::GetLastError();

    if( overlapped == nullptr )
    {
        // 완료된 I/O 없음: QUIT 신호(PostQuit), 사용자 게시 패킷, 타임아웃, 수거 자체 실패.
        if( !success && errorCode != WAIT_TIMEOUT )
            ReportDequeueFailure(_T("GetQueuedCompletionStatus"), errorCode);

        return false;
    }

    CIocpEvent* iocpEvent = static_cast<CIocpEvent*>(overlapped);
    ProcessOverlappedResult(success, iocpEvent, numOfBytes, errorCode);

    return true;
}

//***************************************************************************
// @brief 배치 GQCSEx 방식으로 최대 kBatchSize개의 완료 이벤트를 일괄 수거하여 처리합니다.
// @param timeoutMs 대기 시간 (밀리초)
// @return int32 실제 디스패치한 I/O 완료 항목 수. 0은 "처리한 I/O 완료가 없음"을 뜻하며,
//         타임아웃뿐 아니라 사용자 게시 패킷(wake-up 등)만 수거한 경우도 포함한다.
//         QUIT 패킷을 수거했다면 나머지 이벤트를 모두 처리한 뒤 Iocp::kDispatchQuit(-1).
//         복구 불가능한 수거 실패(IsFatalDequeueError)면 Iocp::kDispatchFatal(-2) — 호출 측 워커는 루프를
//         종료해야 한다. 그 밖의 일시적 수거 실패는 로그와 짧은 대기 후 0을 반환한다.
// @note
//     성능 근거:
//      - GQCS: 완료 이벤트 1개당 syscall 1회
//      - GQCSEx: 완료 이벤트 최대 kBatchSize개당 syscall 1회 (큐가 깊을 때)
//
//     QUIT 처리:
//      한 배치에서 QUIT 패킷을 여러 개 수거하면 이 스레드가 소비할 1개만 남기고
//      초과분은 다시 게시하여 다른 워커가 받을 수 있게 합니다.
//
//     워커 스레드 루프 권장 패턴 :
//      while( true ) {
//          GThreadManager->DistributeReservedJobs();
//          int32 count = iocpCore->DispatchBatch(10);  // 10ms 타임아웃
//          if( count < 0 ) break;                       // QUIT 수거 또는 IOCP 치명 오류
//          if( count == 0 ) GThreadManager->DoGlobalQueueWork();
//      }
//***************************************************************************
int32 CIocpCore::DispatchBatch(uint32 timeoutMs)
{
    OVERLAPPED_ENTRY    entries[Iocp::kBatchSize];
    ULONG               numRemoved = 0;

    BOOL success = ::GetQueuedCompletionStatusEx(
        _iocpHandle,
        entries,
        Iocp::kBatchSize,
        OUT & numRemoved,
        timeoutMs,
        FALSE   // alertable I/O 미사용 (APC 콜백 불필요)
    );

    if( !success )
    {
        const DWORD err = ::GetLastError();
        if( err == WAIT_TIMEOUT )
            return 0;

        if( IsFatalDequeueError(err) )
        {
            // IOCP 핸들 무효/폐쇄 — 재시도해도 복구되지 않는다. 워커가 루프를 종료하도록 알린다.
            LOG_ERROR(_T("[CIocpCore] GetQueuedCompletionStatusEx failed with a fatal error: error=%lu - worker should exit"), err);
            return Iocp::kDispatchFatal;
        }

        // 일시적일 수 있는 수거 실패 — 기록하고 잠시 양보한 뒤 다음 호출에서 재시도한다.
        ReportDequeueFailure(_T("GetQueuedCompletionStatusEx"), err);
        return 0;
    }

    int32 processed = 0;
    ULONG quitCount = 0;

    for( ULONG i = 0; i < numRemoved; i++ )
    {
        LPOVERLAPPED lpOverlapped = entries[i].lpOverlapped;
        if( lpOverlapped == nullptr )
        {
            // I/O 완료가 아닌 PostQueuedCompletionStatus 패킷
            if( entries[i].lpCompletionKey == Iocp::QUIT_KEY )
                ++quitCount;

            continue;
        }

        CIocpEvent* iocpEvent = static_cast<CIocpEvent*>(lpOverlapped);
        DWORD       numOfBytes = entries[i].dwNumberOfBytesTransferred;

        // GQCSEx에서 개별 완료 성공/실패는 OVERLAPPED::Internal로 판단
        // STATUS_SUCCESS(0) 이면 성공
        BOOL entrySuccess = (lpOverlapped->Internal == 0);
        DWORD errorCode = entrySuccess
            ? 0
            : static_cast<DWORD>(::RtlNtStatusToDosError(
                static_cast<NTSTATUS>(lpOverlapped->Internal)));

        ProcessOverlappedResult(entrySuccess, iocpEvent, numOfBytes, errorCode);
        ++processed;
    }

    if( quitCount > 0 )
    {
        // 이 스레드는 1개를 소비하고, 나머지는 다른 워커 몫으로 되돌린다.
        for( ULONG i = 1; i < quitCount; i++ )
        {
            if( !PostQuit() )
            {
                // 다른 워커가 QUIT을 받지 못할 수 있다. 호출 측이 별도의 종료 플래그를 확인하는
                // 루프(예: IocpService의 워커 루프)라면 다음 루프 조건 검사에서 종료할 수 있지만,
                // 이 함수는 그것을 보장하지 않는다. 원인을 남기고 나머지 재게시를 계속 시도한다.
                LOG_ERROR(_T("[CIocpCore] PostQuit re-post failed: error=%lu"), ::GetLastError());
            }
        }

        return Iocp::kDispatchQuit;
    }

    return processed;
}

//***************************************************************************
// @brief I/O 결과 및 에러 코드를 분석하여 적절한 후속 처리를 위임합니다.
// @param success I/O 성공 여부
// @param iocpEvent 완료된 IOCP 이벤트 포인터
// @param numOfBytes 전송된 바이트 수
// @param errorCode 에러 코드
// @note
//      실패한 완료는 iocpEvent->errorCode에 원인을 기록하고 numOfBytes = 0 으로 Dispatch합니다.
//      성공한 Recv의 numOfBytes == 0은 상대의 graceful shutdown(FIN)을 뜻할 수 있으므로, 실패와
//      구분이 필요하면 errorCode를 봅니다. 연결 종료 정책은 Session 계층이 결정합니다.
//      owner 참조는 완료 통지 처리 중에만 해제되므로, 실패 완료도 반드시 Dispatch합니다.
//***************************************************************************
void CIocpCore::ProcessOverlappedResult(BOOL success, CIocpEvent* iocpEvent,
    DWORD numOfBytes, DWORD errorCode)
{
    if( iocpEvent == nullptr )
        return;

    // owner 참조는 이벤트에서 지역 변수로 옮겨(복사하지 않음) Dispatch 동안 객체 수명을 유지한다.
    // 참조 카운트 증감을 한 번씩 아끼고, 이벤트의 owner는 이 시점에 비워진다.
    CIocpObjectRef iocpObject = std::move(iocpEvent->owner);
    if( iocpObject == nullptr )
    {
        // 불변식 위반: I/O가 게시된 이벤트의 owner는 완료 통지가 도착하기 전에 해제되면 안 된다.
        // 이 완료는 처리할 대상이 없어 버려지며, 대응하는 pending 상태(_pendingIoCount 등)는
        // 정리되지 않는다. ASSERT_CRASH는 릴리스에서도 프로세스를 종료시키므로, 코어 계층에서는
        // 원인 추적용 로그만 남긴다.
        thread_local uint32 orphanCount = 0;
        if( ShouldLogSampled(orphanCount) )
        {
            LOG_ERROR(_T("[CIocpCore] completion arrived for an event without owner: eventType=%d, success=%d, error=%lu (occurrence %u on this thread)"),
                static_cast<int>(iocpEvent->eventType), success ? 1 : 0, errorCode, orphanCount);
        }
        return;
    }

    if( success )
    {
        iocpEvent->errorCode = 0;
        iocpObject->Dispatch(iocpEvent, static_cast<int32>(numOfBytes));
        return;
    }

    iocpEvent->errorCode = errorCode;

    // 정상적인 연결 종료·취소는 로그 생략. Connect 실패는 Session 계층이 처리/통지한다.
    if( iocpEvent->eventType != Iocp::EventType::Connect && !IsExpectedDisconnectError(errorCode) )
    {
        uint32 occurrence = 0;
        if( ShouldLogCompletionError(errorCode, occurrence) )
        {
            LOG_ERROR(_T("[CIocpCore] I/O completion failed: eventType=%d, error=%lu (occurrence %u on this thread)"),
                static_cast<int>(iocpEvent->eventType), errorCode, occurrence);
        }
    }

    iocpObject->Dispatch(iocpEvent, 0);
}