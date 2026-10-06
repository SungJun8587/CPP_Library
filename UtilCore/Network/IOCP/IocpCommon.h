
//***************************************************************************
// IocpCommon.h: Common header file for Iocp including macros, constants, and types.
//
//***************************************************************************

#ifndef UC_IOCPCOMMON_H
#define UC_IOCPCOMMON_H

#include <winsock2.h>
#include <BaseRedefineDataType.h>

#include <chrono>

namespace Iocp
{
    //***************************************************************************
    // @enum EventType
    // @brief IocpEvent가 어떤 종류의 비동기 I/O인지 식별하는 열거형.
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

    //***************************************************************************
    // @enum CloseReason
    // @brief 세션 종료 원인을 나타내는 열거형
    //***************************************************************************
    enum class CloseReason
    {
        None,
        RemoteClosed,           // 상대방 연결 정상 종료 (Recv 0 bytes)
        SocketError,            // 소켓 네트워크 에러 (WSARecv/WSASend 실패 등)
        ReceivePostFailed,      // 수신 요청(WSARecv) 게시 실패
        SendPostFailed,         // 송신 요청(WSASend) 게시 실패
        RingBufferOverflow,     // 수신 링버퍼 오버플로우
        SendBufferOverflow,     // 송신 링버퍼 오버플로우
        InternalError,          // 기타 내부 처리 에러
        ForcedClose             // 서버 측 또는 관리자에 의한 강제 종료
    };

    //***************************************************************************
    // @brief 기본 네트워크 수신 버퍼 크기 (10KB / 10,240 Byte).
    // @details
    // 일반적인 IOCP 게임 서버 및 네트워크 프로그램에 최적화된 균형 잡힌 기본값입니다.
    // - TCP MSS(약 1,460 Byte) 기준 약 7개의 세그먼트를 수용 (패킷 뭉침 및 Burst 트래픽 대응)
    // - 동시 접속자 1만 명(C10K) 기준 총 메모리 사용량 약 100MB 수준의 뛰어난 메모리 효율성
    // - 포인터 경계 비교 방식을 사용하는 CRingBuffer 구조상 2의 거듭제곱 크기가 아니어도 문제없이 동작
    //
    // [워크로드별 권장 버퍼 크기 가이드] 
    // - 일반 게임 서버 (기본) : 10,240 B (10 KB)   - 소형 패킷 위주, 메모리/안정성 최적 균형
    // - 대규모 MMORPG / 밀집 지역 : 16,384~32,768 B (16~32 KB) - 순간적 대량 패킷 폭주(Overflow) 방지
    // - 초고집적 서버 (수만 명+) : 4,096 B (4 KB)    - 동접자 증가에 따른 극도의 RAM 절감 필요 시
    // - 파일/패치/대용량 데이터 : 65,536 B 이상 (64 KB+) - Socket I/O 전송 효율 극대화
    //***************************************************************************
    inline constexpr int32 BUFFER_SIZE_DEFAULT = 10240;

    //***************************************************************************
    // @brief 한 번의 완료 큐(IOCP/CQ) 수거 작업 시 일괄 처리할 최대 이벤트 개수.
    // @details
    // GQCS(Ex) 또는 Dequeue 함수 호출 시 한 번에 배치로 수거할 최대 패킷/결과 수를 정의합니다.
    //***************************************************************************
    inline constexpr ULONG kBatchSize = 64;

    //***************************************************************************
    // @brief 세션 매니저에서 락 경합을 최소화하기 위해 분산 처리할 클러스터 개수.
    // @details
    // [클러스터 개수 최적화 가이드 및 선정 이유]
    // - 2의 제곱수 최적화: 16은 2의 제곱수($2^4$)이므로, 내부 해시 인덱스 연산(key % kSessionClusterCnt) 시 
    //   컴파일러가 느린 나눗셈 대신 매우 빠른 비트 연산(key & (kSessionClusterCnt - 1))으로 자동 최적화합니다.
    // - 락 경합 대폭 완화: 단일 락 구조 대비 동시 접근 시 충돌 확률을 1/16 수준으로 줄여줍니다.
    // - 코어 구조와의 조화: 일반적인 8~16코어(하이퍼스레딩 포함 16~32스레드) 상용 서버 환경에서 
    //   워커 스레드들이 서로 다른 락을 참조할 확률을 높여 병목을 효과적으로 해소합니다.
    // [예시]
    //		- 대규모 하이엔드 서버 환경(32코어 이상, 수천 명 동접)의 경우 클러스터 개수를 32 또는 64로 늘려 부하를 분산하는 것을 권장
    //		- 소규모 서버 또는 테스트 환경(2 ~ 4코어)의 경우 클러스터 개수를 8 또는 16
    //***************************************************************************
    inline constexpr int32 kSessionClusterCnt = 16;

    //***************************************************************************
    // @brief 워커 스레드 종료 알림용 특수 완료 키(Completion Key)입니다.
    // @details ULONG_PTR의 최댓값(-1)을 사용하여 일반 completion key와 구분합니다.
    //***************************************************************************
    inline constexpr ULONG_PTR QUIT_KEY = static_cast<ULONG_PTR>(-1);

    //***************************************************************************
    // @brief CSendBufferChunk의 기본 메모리 청크 크기 (8KB / 8,192 Byte).
    // @details
    // 스레드별로 할당되어 SendBuffer들에 메모리를 공급하는 청크 단위 크기입니다.
    // - 고정 크기 메모리 할당을 통해 동적 할당 오버헤드 및 단편화(Fragmentation) 최소화
    // - std::array의 템플릿 크기 인자 및 버퍼 남은 용량(FreeSize) 계산에 활용
    //***************************************************************************
    inline constexpr uint32 SEND_BUFFER_CHUNK_SIZE = 8192;

    //***************************************************************************
    // @brief Accept Pool에 상시 유지할 기본 AcceptContext 개수 (10개).
    // @details
    // 서버가 동시 접속 요청을 수락하기 위해 미리 게시(Post)해 두는 AcceptEx의 기본 수량입니다.
    // - CIocpListener의 기본 Accept 요청 수(acceptCount)와 동기화된 기본값
    // - 동시 접속 폭주(Connection Burst) 트래픽 환경에서는 수치를 늘려 대기열 병목 예방 권장
    //***************************************************************************
    inline constexpr uint32 kDefaultAcceptPoolSize = 10;

    //***************************************************************************
    // @brief AcceptEvent::acceptBuffer의 크기 (128 Byte).
    // @details
    // AcceptEx가 로컬/원격 주소를 기록하는 버퍼입니다. dwReceiveDataLength = 0으로 호출하므로
    // 로컬 주소 + 원격 주소(각 CSocketUtils::kAcceptExAddrLen) 두 칸만 있으면 되며,
    // IPv6 주소 길이까지 담을 수 있는 여유를 둔 값입니다(IocpListener.cpp에서 static_assert로 검증).
    //***************************************************************************
    inline constexpr uint32 kAcceptBufferSize = 128;

    //***************************************************************************
    // @brief AcceptEx 재등록 실패 시 재시도 지연(백오프) 설정.
    // @details
    // - kAcceptRetryBaseDelayMs: 1회째 실패 후 지연(ms). 이후 연속 실패마다 두 배씩 늘어난다.
    // - kAcceptRetryMaxDelayMs : 지연의 상한(ms). 이 값에서 멈추고 재시도는 포기하지 않는다.
    // - kAcceptRetryLogInterval: 연속 실패 로그를 첫 실패와 이 횟수마다 남긴다(로그 폭주 방지).
    //***************************************************************************
    inline constexpr uint32 kAcceptRetryBaseDelayMs = 10;
    inline constexpr uint32 kAcceptRetryMaxDelayMs = 1000;
    inline constexpr int32  kAcceptRetryLogInterval = 20;

    //***************************************************************************
    // @brief CIocpSession::Send() 한 번에 보낼 수 있는 최대 바이트 수와, 그것을 담는 데 필요한
    //        SendBuffer(청크 크기 단위) 최대 개수.
    // @details Send()의 크기 인자가 uint16이므로 상한은 65535입니다.
    //***************************************************************************
    inline constexpr uint32 kMaxSendSize = 65535;
    inline constexpr uint32 kMaxSendPieces = (kMaxSendSize + SEND_BUFFER_CHUNK_SIZE - 1) / SEND_BUFFER_CHUNK_SIZE;
    static_assert(kMaxSendPieces* SEND_BUFFER_CHUNK_SIZE >= kMaxSendSize, "pieces must cover the maximum send size");

    //***************************************************************************
    // @brief CIocpCore::DispatchBatch() 반환값 규약 중 음수 값.
    // @details 양수 = 처리한 완료 이벤트 수, 0 = 처리할 이벤트 없음(타임아웃 등),
    //          음수 = 워커가 루프를 종료해야 한다는 신호.
    // - kDispatchQuit : PostQuit() 패킷을 수거함
    // - kDispatchFatal: IOCP 핸들 무효/폐쇄 등 복구 불가능한 수거 실패
    //***************************************************************************
    inline constexpr int32 kDispatchQuit = -1;
    inline constexpr int32 kDispatchFatal = -2;

    //***************************************************************************
    // @brief 워커 스레드가 DispatchBatch()에서 한 번에 대기하는 시간(ms).
    // @details 종료 플래그(IsShuttingDown)를 이 주기로 다시 확인할 수 있어, 종료 wake-up 패킷이
    //          유실되더라도 워커가 이 시간 안에 루프를 빠져나옵니다.
    //***************************************************************************
    inline constexpr uint32 kWorkerPollTimeoutMs = 10;

    //***************************************************************************
    // @brief 완료 큐 수거 API 자체가 일시적으로 실패했을 때 워커가 양보하는 시간(ms).
    // @details 같은 실패를 대기 없이 반복 호출하며 CPU를 점유(spin)하는 것을 막는다.
    //***************************************************************************
    inline constexpr uint32 kDequeueFailureBackoffMs = 10;

    //***************************************************************************
    // @brief 서비스 Close() 시 세션 해제 통지(OnDisconnected)를 기다리는 최대 시간.
    // @details 이 시간 안에 끝나지 않는 세션(응답 없는 피어 등)은 CNetService::Close()가
    //          Disconnect()의 강제 정리 경로로 마무리한다.
    //***************************************************************************
    inline constexpr std::chrono::seconds kCloseDrainTimeout{ 5 };

    //***************************************************************************
    // @brief 서버 세션 매니저의 닫힌 세션 정리(reap) 주기.
    // @details 해제 통지 경로(OnSessionDisconnected)가 놓친 엔트리를 정리하는 안전망의 주기이며,
    //          세션 처리량/서버 규모에 맞춰 조정할 수 있는 기본값입니다.
    //***************************************************************************
    inline constexpr std::chrono::seconds kSessionReapInterval{ 30 };
}

#endif // ndef UC_IOCPCOMMON_H