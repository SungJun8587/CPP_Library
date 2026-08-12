
//***************************************************************************
// RioCommon.h: Common header file for Rio including macros, constants, and types.
//
//***************************************************************************

#ifndef __RIOCOMMON_H__
#define __RIOCOMMON_H__

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#include <winsock2.h>
#include <mswsock.h>
#include <cstdint>
#include <chrono>

namespace Rio
{
    //**********************************************************************************************************************
    // @brief Shutdown 작업 수행 시 반환되는 결과 상태 코드
    //**********************************************************************************************************************
    enum class ShutdownResult : int32
    {
        Success = 0,        // 모든 Outstanding I/O Drain 완료 및 자원 정상 해제
        DrainTimeout = 1,   // 지정된 타임아웃 내에 Outstanding I/O가 0에 도달하지 못함
        CorruptCq = 2,      // CQ 오염(RIO_CORRUPT_CQ)으로 인해 비정상 종료됨
        InvalidCall = 3,    // 잘못된 호출Context (예: Dispatch 콜백 내부에서 Shutdown 호출)
        DispatchError = 4   // Shutdown 드레인 중 디스패치 또는 워커 스레드 오류 발생
    };

    //**********************************************************************************************************************
    // @brief CRioCore 엔진의 라이프사이클 상태 정의
    //**********************************************************************************************************************
    enum class State : int32
    {
        Uninitialized = 0,  // 초기화 전 상태
        Initializing = 1,   // 초기화 진행 중 상태
        Initialized = 2,    // CQ/IOCP 생성 및 초기화 완료 상태
        Running = 3,        // 워커 구동 중 및 I/O 제출/디스패치 정상 작동 상태
        Stopping = 4,       // 정지 요청 진입, 신규 I/O 제출 차단 및 Drain 시작 상태
        Stopped = 5,        // Outstanding I/O가 0에 도달하여 완전히 정지된 상태
        Faulted = 6,        // internal fault, CQ corruption 등 치명적 오류 발생 상태
        Closed = 7          // 모든 자원이 파괴되고 최종 닫힌 상태
    };

    //**********************************************************************************************************************
    // @brief 이벤트 수거 및 디스패치 모드
    //**********************************************************************************************************************
    enum class DispatchMode : int32
    {
        Wait = 0,   // 수거할 이벤트가 없을 경우 RIONotify 후 IOCP(GQCS) 대기 진입
        Drain = 1   // Shutdown 시 블로킹 대기 없이 잔여 이벤트만 비우고 즉시 반환
    };

    //**********************************************************************************************************************
    // @brief 슬롯의 할당 및 사용 상태를 정의하는 열거형
    //**********************************************************************************************************************
    enum class SlotState : uint8_t
    {
        Free = 0,           // 슬롯이 비어 있어 AllocSlot()을 통해 할당 가능한 상태
        Allocated = 1       // 슬롯이 할당되어 현재 사용 중인 상태(FreeSlot()을 통해서만 Free 상태로 전환 가능)
    };

    //***************************************************************************
    // @enum EEventState
    // @brief CRioEvent 객체의 디버그 및 메모리 상태 추적용 열거형입니다.
    // @details
    //      CRioEventPool 내 Free List의 보호 락(Lock) 내부에서만 상태가 변경되므로
    //      별도의 원자적 연산(std::atomic) 없이 안전하게 관리됩니다.
    //
    //      [Lifecycle Flow]
    //      Constructor -> Free <---> InUse
    //***************************************************************************
    enum class EEventState : uint8_t
    {
        Free = 0,   // 이벤트 객체가 풀(Pool)에 반납되어 재사용 가능한 상태
        InUse = 1   // 이벤트 객체가 할당되어 비동기 I/O 요청 처리에 사용 중인 상태
    };

    //***************************************************************************
    // @enum EventType
    // @brief 비동기 RIO I/O 작업의 유형을 정의하는 열거형입니다.
    // @details
    //      RIO 완료 통지(Completion) 이벤트가 발생했을 때 처리해야 할
    //      I/O 작업(수신/송신)의 종류를 구분하는 데 사용됩니다.
    //***************************************************************************
    enum class EventType : uint8_t
    {
        Receive = 0,    // 비동기 패킷 수신 요청 작업 (RIOReceive)
        Send = 1        // 비동기 패킷 송신 요청 작업 (RIOSend / RIOSendEx)
    };

    //***************************************************************************
    // @enum SessionState
    // @brief RIO 세션의 라이프사이클 상태를 정의하는 열거형입니다.
    // @details
    //      세션의 생성부터 완전 종료까지의 원자적 상태 전이를 추적합니다.
    //***************************************************************************
    enum class SessionState : uint8_t
    {
        None = 0,   // 초기화되지 않은 미정의 상태
        Active,     // 연결이 활성화되어 I/O 요청 및 송수신 처리가 가능한 상태
        Closing,    // 종료 절차가 시작되어 신규 I/O 제출이 차단된 상태
        Closed      // 세션 연결이 완전 종료되고 정리 대기/완료된 상태
    };

    //***************************************************************************
    // @enum ServerState
    // @brief 서버 엔진의 동작 라이프사이클 상태를 정의하는 열거형입니다.
    // @details
    //      서버 인스턴스의 생성부터 리소스 완전 해제까지의 구동 상태 전이를 관리합니다.
    //***************************************************************************
    enum class ServerState : uint8_t
    {
        Created = 0,    // 서버 객체가 생성되었으나 초기화되지 않은 상태
        Initialized,    // RIO 커널 자원 및 네트워크 바인딩 초기화가 완료된 상태
        Running,        // I/O 완료 통지 루프 및 서비스가 정상 동작 중인 상태
        Stopping,       // 서버 정지 요청을 받아 세션 CloseAll 및 잔여 I/O 정리를 진행 중인 상태
        Stopped,        // 모든 세션 정리 및 네트워크 I/O 처리가 안전하게 정지된 상태
        Shutdown        // 서버 할당 자원이 완벽히 해제되고 최종 종료된 상태
    };

    //******************************************************************************************************************
    // @brief 한 번의 RIODequeueCompletion() 호출에서 수거할 최대 completion 수
    //******************************************************************************************************************
    static constexpr ULONG kBatchSize = 64;

    // DispatchBatch 및 내부 처리 반환용 내부 상태 상숫값
    static constexpr int32 kCorruptCq = -1;             // CQ 오염 발생
    static constexpr int32 kStopped = -2;               // Core 정지 완료
    static constexpr int32 kNotifyError = -3;           // RIONotify 실패
    static constexpr int32 kIocpError = -4;             // GQCS 오류 발생
    static constexpr int32 kInvalidCompletion = -5;     // 유효하지 않은 완료 패킷 수신

    static constexpr std::chrono::milliseconds kDefaultDrainTimeout{ 5000 }; // 기본 Drain 타임아웃 (5초)

    //**********************************************************************************************************************
    // @brief 유효하지 않은 슬롯 인덱스를 나타내는 센티널(Sentinel) 상숫값
    // @details AllocSlot() 등의 함수가 슬롯 할당 실패 시 반환하거나, 초기화되지 않은 슬롯 인덱스를
    //          표시할 때 사용합니다. uint32_t의 최댓값(0xFFFFFFFFu, UINT32_MAX)을 가리킵니다.
    //**********************************************************************************************************************
    static constexpr uint32_t kInvalidSlotIndex = 0xFFFFFFFFu;

    //***************************************************************************
    // @brief Accept 폴링 주기
    // @details
    //      Accept 루프에서 클라이언트 접속이 없을 때 대기할 시간 간격을 정의합니다.
    //***************************************************************************
    constexpr std::chrono::milliseconds kAcceptPollInterval(1);

    //***************************************************************************
    // @brief Listen 소켓 Backlog 최소 값
    // @details
    //      Listen 소켓 생성 시 설정 가능한 연결 대기 큐(Backlog)의 최소 허용치입니다.
    //***************************************************************************
    constexpr int kListenBacklogMinimum = 1;
}

#endif // __RIOCOMMON_H__


