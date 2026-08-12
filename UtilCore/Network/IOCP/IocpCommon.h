
//***************************************************************************
// IocpCommon.h: Common header file for Iocp including macros, constants, and types.
//
//***************************************************************************

#ifndef __IOCPCOMMON_H__
#define __IOCPCOMMON_H__

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#include <winsock2.h>
#include <winternl.h>

namespace Iocp
{
    //***************************************************************************
    // @brief 한 번의 완료 큐(IOCP/CQ) 수거 작업 시 일괄 처리할 최대 이벤트 개수.
    //
    // @details
    // GQCS(Ex) 또는 Dequeue 함수 호출 시 한 번에 배치로 수거할 최대 패킷/결과 수를 정의합니다.
    //***************************************************************************
    static constexpr ULONG kBatchSize = 64;

    //***************************************************************************
    // @enum EventType
    // @brief IocpEvent가 어떤 종류의 비동기 I/O인지 식별하는 열거형.
    //
    // @details
    // Session::Dispatch에서 switch문으로 분기해 적절한 Process 함수를 호출합니다.
    //***************************************************************************
    enum class EventType : uint8
    {
        Connect,        // ConnectEx 완료 (클라이언트 → 서버 연결 성공)
        Disconnect,     // DisconnectEx 완료 (연결 종료 및 소켓 초기화 완료)
        Accept,         // AcceptEx 완료 (서버 → 클라이언트 연결 수락)
        Recv,           // WSARecv 완료 (데이터 수신)
        Send,           // WSASend 완료 (데이터 전송)
    };
}

#endif // __IOCPCOMMON_H__


