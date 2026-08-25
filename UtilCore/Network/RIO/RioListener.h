
//***************************************************************************
// RioListener.h : interface for the CRioListener class.
//
//***************************************************************************

#ifndef __RIOLISTENER_H__
#define __RIOLISTENER_H__

#ifndef __RIO_COMMON_H__
#include <Network/Rio/RioCommon.h>
#endif

#ifndef __RIO_CORE_H__
#include <Network/Rio/RioCore.h>
#endif

#ifndef __NET_ADDRESS_H__
#include <Network/NetAddress.h>
#endif

#include <functional>
#include <thread>
#include <vector>
#include <atomic>
#include <memory>

//***************************************************************************
// @brief RIO 전용 세션 생성 팩터리 함수 타입
// @return CRioSessionRef 생성된 RIO 세션 객체 포인터
//***************************************************************************
using RioSessionFactory = std::function<CRioSessionRef()>;

//***************************************************************************
// @brief Accept 완료 후 외부 후속 처리를 담당할 콜백 함수 타입
// @param session 연결이 완료된 세션 객체
// @param clientSocket 클라이언트 소켓 핸들
// @param requestQueue 생성된 RIO_RQ 핸들
// @param netAddr 추출된 클라이언트의 IP/Port 주소 정보
//***************************************************************************
using OnRioAcceptCallback = std::function<void(CRioSessionRef session, SOCKET clientSocket, RIO_RQ requestQueue, CNetAddress netAddr)>;

//***************************************************************************
// @class CRioListener
// @brief RIO(Registered I/O) 기반의 클라이언트 접속 수락(Accept) 전담 클래스
//
// @details
// [재설계 배경 — A안: Accept 전용 IOCP + AcceptContext Pool]
//      기존 구현은 전용 스레드 1개(_acceptThread)가
//          AcceptEx 게시 -> WSAGetOverlappedResult(TRUE, 블로킹 대기)
//          -> SetUpdateAcceptContext -> 주소 파싱 -> RIOCreateRequestQueue
//          -> SessionFactory -> Callback -> 다음 AcceptEx
//      를 완전히 직렬로 반복했다. 즉 연결 하나의 후처리가 끝나야 다음
//      AcceptEx가 게시되어, 대량 동시 접속(서버 재시작 직후 재접속 폭주 등)
//      상황에서 CIocpListener(Accept Pool 방식) 대비 처리율이 크게 떨어질
//      수 있었다.
//
//      이 버전은 CIocpListener와 동일한 원칙으로 재설계한다 — Listen 소켓을
//      이 클래스 전용의 별도 IOCP(_acceptIocp)에 등록하고, kDefaultAcceptPoolSize
//      개의 AcceptContext에 대해 AcceptEx를 미리 동시에 게시해 둔다. 하나의
//      완료가 처리되는 동안에도 커널은 나머지 outstanding AcceptEx로 계속
//      새 연결을 받을 수 있다.
//
//      [CRioCore와의 completion 분리 — 유지]
//      _acceptIocp는 CRioCore가 쓰는 RIO CQ/IOCP와 완전히 독립적이다
//      (CRioConnectDispatcher가 ConnectEx 완료를 CRioCore와 분리한 것과
//      동일한 원칙). RIODequeueCompletion 전용 경로에 일반 OVERLAPPED
//      완료(AcceptEx)를 섞으면 CRioCore::IsValidCompletionPacket() 검증에
//      걸려 전체가 Faulted로 죽는 문제가 있었기 때문에, Accept subsystem은
//      끝까지 별도로 둔다.
//
//      [처리 경계 — 1단계 권장안 채택]
//      Accept Worker는 AcceptEx 완료 -> SetUpdateAcceptContext -> 주소 파싱
//      -> RIOCreateRequestQueue -> SessionFactory -> Callback 호출까지
//      수행한 뒤에 같은 AcceptContext를 재사용해 다음 AcceptEx를 재게시한다
//      (완료 처리 전체를 마친 뒤 재게시 — "빠른 재게시 우선" 2단계 최적화는
//      현재 규모에서는 불필요 판단). Callback 안에서 실제 CRioSession::Init()
//      과 PostInitialReceive()가 호출되며, 그 시점부터는 완전히 CRioCore의
//      RIO CQ 경로로 넘어간다 — 이 클래스는 그 이후를 전혀 관여하지 않는다.
//
//      [스레드 안전성]
//      _listenSocket은 std::atomic<SOCKET>으로 유지한다(Accept Worker와
//      Stop()을 호출하는 임의의 스레드가 동시 접근). _rioCore/_sessionFactory/
//      _onAcceptCallback은 Start() 이후 Stop()에서 리셋하지 않는다(기존
//      설계와 동일 — 워커들이 Stop()의 Join이 끝날 때까지 이 값들을 계속
//      참조하기 때문에, 이 값들을 이번 Stop()에서 지워버리면 아직 처리 중인
//      워커가 널 참조를 할 위험이 있다. 호출자는 Stop()이 반환할 때까지
//      리스너를 살려둔다).
//      _sessionFactory()/_onAcceptCallback() 호출은 사용자 코드이므로
//      예외가 발생할 수 있다. Accept Worker의 스레드 함수 밖으로 예외가
//      빠져나가면 std::terminate()가 호출되므로 둘 다 try/catch로 감싼다.
//
// 역할:
//      1. Listen 소켓 생성, 바인딩(Bind) 및 리슨(Listen) 수행
//      2. Accept 전용 IOCP 생성 및 kDefaultAcceptPoolSize개의 AcceptEx 사전 등록
//      3. Accept Worker(들)가 완료 통지를 받아 순차적으로 세션 생성까지 처리
//      4. 처리 완료 후 같은 AcceptContext를 재사용해 즉시 재게시(Pool 개수 영구 유지)
//***************************************************************************
class CRioListener : public std::enable_shared_from_this<CRioListener>
{
public:
    //***************************************************************************
    // @brief Accept Pool에 상시 유지할 AcceptContext(=outstanding AcceptEx) 개수 기본값.
    //        CIocpListener의 기본 acceptCount(10)와 동일한 수준으로 맞췄다.
    //***************************************************************************
    static constexpr uint32_t kDefaultAcceptPoolSize = 10;

    //***************************************************************************
    // @brief Accept 전용 IOCP를 소비할 워커 스레드 기본 개수.
    //        연결 시도 자체는 CRioConnectDispatcher와 마찬가지로 상대적으로
    //        빈도가 낮고(데이터 송수신과 달리), 완료 처리(SetUpdateAcceptContext
    //        ~ Callback)도 짧은 논블로킹 호출 위주라 1개로도 충분하다고 판단했다.
    //        대규모 동시 접속 스트레스 테스트 결과에 따라 늘릴 수 있도록 Start()
    //        파라미터로 노출한다.
    //***************************************************************************
    static constexpr uint32_t kDefaultAcceptWorkerCount = 1;

    CRioListener();
    virtual ~CRioListener();

    CRioListener(const CRioListener&) = delete;
    CRioListener& operator=(const CRioListener&) = delete;

public:
    //***************************************************************************
    // @brief 리스너를 초기화하고 Accept IOCP + AcceptContext Pool을 구동합니다.
    // @param rioCore RIO 코어 참조 객체 (null이면 실패) — RIOCreateRequestQueue
    //        호출에만 사용되며, 이 클래스 자신의 completion 경로와는 무관하다.
    // @param netAddr 리슨할 네트워크 주소 (IP/Port)
    // @param sessionFactory 세션 생성 팩터리 (null이면 실패)
    // @param onAccept Accept 완료 시 호출될 콜백 (null이면 콜백 없이 동작 — 이 경우
    //        clientSocket/requestQueue는 즉시 정리되고 세션은 버려집니다)
    // @param acceptPoolSize 상시 유지할 outstanding AcceptEx 개수 (기본 kDefaultAcceptPoolSize)
    // @param acceptWorkerCount Accept 전용 IOCP를 소비할 워커 스레드 개수 (기본 kDefaultAcceptWorkerCount)
    // @return bool 소켓 생성/바인드/리슨, Accept IOCP 생성, 워커 시작, 초기 Pool 게시까지
    //         전부 성공하면 true
    //***************************************************************************
    bool Start(CRioCoreRef rioCore, CNetAddress netAddr, RioSessionFactory sessionFactory, OnRioAcceptCallback onAccept = nullptr,
        uint32_t acceptPoolSize = kDefaultAcceptPoolSize, uint32_t acceptWorkerCount = kDefaultAcceptWorkerCount);

    //***************************************************************************
    // @brief 리스너를 정지합니다.
    // @details 순서: _isListening=false -> CancelIoEx로 outstanding AcceptEx 전체
    //          취소 -> Listen 소켓 close -> Accept 워커들에 wake-up 패킷 포스팅
    //          -> 전체 워커 Join -> Accept IOCP close -> 잔여 accept 소켓 정리.
    //          이미 정지된 상태에서 다시 호출해도 안전합니다(idempotent).
    //***************************************************************************
    void Stop();

    //***************************************************************************
    // @brief 현재 Listen 소켓 핸들을 반환합니다.
    // @return SOCKET 현재 리슨 중인 소켓 핸들 (정지 상태면 INVALID_SOCKET)
    //***************************************************************************
    SOCKET GetListenSocket() const noexcept { return _listenSocket.load(std::memory_order_acquire); }

private:
    //***************************************************************************
    // @struct RioAcceptContext
    // @brief 하나의 outstanding AcceptEx 요청을 표현하는 슬롯. OVERLAPPED를
    //        상속해 완료 통지 시 곧바로 이 구조체로 캐스팅할 수 있도록 한다
    //        (CIocpEvent/AcceptEvent와 동일한 패턴 — CONTAINING_RECORD 대신
    //        상속을 택해 프로젝트 내 기존 스타일과 통일).
    //***************************************************************************
    struct RioAcceptContext : public OVERLAPPED
    {
        static constexpr DWORD kAddrLen = sizeof(SOCKADDR_IN) + 16;

        //***********************************************************************
        // @brief OVERLAPPED 필드를 0으로 초기화합니다. AcceptEx 재사용 전 필수.
        //***********************************************************************
        void Init()
        {
            Internal = 0;
            InternalHigh = 0;
            Offset = 0;
            OffsetHigh = 0;
            hEvent = nullptr;
        }

        SOCKET acceptSocket{ INVALID_SOCKET };     // AcceptEx가 채울 클라이언트 소켓 (사전 생성)
        BYTE   acceptBuffer[kAddrLen * 2]{};       // GetAcceptExSockaddrs 파싱용 로컬/원격 주소 버퍼

        // RegisterAccept/ProcessAccept 실패 경로가 공유하는 누적 연속 실패 횟수.
        // kMaxAcceptRetry 초과 시 이 슬롯은 재게시를 포기한다(Pool 크기가
        // 영구적으로 줄어들 수 있음 — 매우 드문 상황이며 로그로 남긴다).
        int32  retryCount{ 0 };
    };

    static constexpr int32 kMaxAcceptRetry = 5;

    bool InitializeAcceptIocp(uint32_t workerCount);

    //***************************************************************************
    // @brief 지정된 AcceptContext에 대해 신규 클라이언트 소켓을 만들고 AcceptEx를
    //        게시합니다. 즉시 실패 시 재시도 상한 내에서 짧게 대기 후 재시도합니다
    //        (이 스레드는 RIO 데이터 경로와 완전히 분리된 Accept 전용 워커이므로,
    //        여기서의 짧은 블로킹은 RIO 처리량에 영향을 주지 않는다).
    // @return bool 게시(또는 즉시 성공) 성공 여부. false는 재시도 상한 초과로
    //         이 슬롯의 outstanding AcceptEx가 더 이상 없다는 뜻이다.
    //***************************************************************************
    bool PostAccept(RioAcceptContext* context);

    //***************************************************************************
    // @brief Accept 전용 IOCP를 소비하는 워커 루프.
    //***************************************************************************
    void AcceptWorkerLoop();

    //***************************************************************************
    // @brief AcceptEx 완료를 처리합니다: 주소 컨텍스트 갱신, 주소 파싱,
    //        RIO Request Queue 생성, 세션 생성, 콜백 통지, 이후 같은 컨텍스트로
    //        재게시까지 전부 수행합니다.
    // @param context 완료된 AcceptContext
    // @param succeeded GetQueuedCompletionStatus가 보고한 이 I/O의 성공 여부
    //***************************************************************************
    void ProcessAccept(RioAcceptContext* context, bool succeeded);

    //***************************************************************************
    // @brief 지정된 클라이언트 소켓용 RIO Request Queue를 생성합니다.
    // @param clientSocket 바인딩할 클라이언트 소켓
    // @return 생성된 RIO_RQ 핸들 (실패 시 RIO_INVALID_RQ)
    //***************************************************************************
    RIO_RQ CreateRequestQueueForSocket(SOCKET clientSocket);

private:
    std::atomic<SOCKET>     _listenSocket{ INVALID_SOCKET };    // 리스닝 소켓 핸들 (Accept Worker와 Stop() 호출 스레드가 동시 접근)
    CRioCoreRef              _rioCore = nullptr;                 // 연동할 RIO 코어 참조 (RIOCreateRequestQueue 호출용, completion 경로는 무관)
    RioSessionFactory        _sessionFactory = nullptr;          // 세션 생성 팩터리
    OnRioAcceptCallback      _onAcceptCallback = nullptr;        // Accept 완료 알림 콜백

    std::atomic<bool>        _isListening{ false };              // 리스닝 상태 플래그

    HANDLE                   _acceptIocp{ nullptr };             // Accept 전용 IOCP (CRioCore의 CQ/IOCP와 완전히 별개)
    std::vector<std::thread> _acceptWorkers;                     // Accept 전용 IOCP를 소비하는 워커 스레드들
    std::vector<std::unique_ptr<RioAcceptContext>> _acceptContexts; // 상시 유지되는 AcceptContext Pool
};
#endif // ndef __RIOLISTENER_H__