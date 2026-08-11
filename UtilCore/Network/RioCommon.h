
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
    // @brief CRioEvent 디버그 Lifecycle 상태
    //
    // NOTE:
    //      이 상태는 CRioEventPool의 Free List 보호 락 내부에서만 변경됩니다.
    //      따라서 별도의 atomic이 필요하지 않습니다.
    //
    // Lifecycle:
    //
    //      Constructor
    //          |
    //          v
    //        Free
    //          |
    //          v
    //        InUse
    //          |
    //          v
    //        Free
    //
    //***************************************************************************
    enum class EEventState : uint8_t
    {
        Free = 0,
        InUse = 1
    };

    //***************************************************************************
    // @brief 비동기 RIO 작업의 종류
    //***************************************************************************
    enum class EventType : uint8_t
    {
        Receive = 0,        
        Send = 1            
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
}

#include <Network/RioObject.h>
#include <Network/RioEvent.h>
#include <Network/RioEventPool.h>
#include <Network/RioCore.h>
#include <Network/RioBuffer.h>
#include <Network/RioWorker.h>
#include <Network/RioSend.h>
#include <Network/RioReceive.h>

#endif // __RIOCOMMON_H__


