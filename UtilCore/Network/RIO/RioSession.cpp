
//***************************************************************************
// RioSession.cpp : implementation of the CRioSession class.
//
//***************************************************************************

#include "pch.h"
#include "RioSession.h"

//***************************************************************************
// @brief CRioSession 생성자
// @details 부모 클래스 CRioObject를 초기화하고 내부에 할당된 변수들을 안전한 초기값으로 세팅합니다.
//***************************************************************************
CRioSession::CRioSession() noexcept
    : CRioObject()
    , _core(nullptr)
    , _socket(INVALID_SOCKET)
    , _requestQueue(RIO_INVALID_RQ)
    , _state(Rio::SessionState::None)
{
}

//***************************************************************************
// @brief CRioSession 소멸자
// @details 객체 소멸 과정에서 Close()를 호출하여 세션이 완전히 정리되도록 보장합니다.
//***************************************************************************
CRioSession::~CRioSession() noexcept
{
    Close();
}

//***************************************************************************
// @brief 세션 초기화
// @param core RIO Core 참조 객체
// @param socket 바인딩할 클라이언트 소켓
// @param requestQueue 생성된 RIO Request Queue
// @return 성공 시 true, 인자가 잘못되었거나 세션이 이미 초기화된 상태면 false
// @details CAS 연산을 통해 상태를 None에서 Active로 원자적으로 전이시키며,
//          초기화 성공 시 내부 소켓 및 RIO_RQ 핸들을 저장합니다.
//***************************************************************************
bool CRioSession::Initialize(CRioCore& core, SOCKET socket, RIO_RQ requestQueue) noexcept
{
    if( socket == INVALID_SOCKET )
        return false;

    if( requestQueue == RIO_INVALID_RQ )
        return false;

    Rio::SessionState expected = Rio::SessionState::None;

    // None -> Active 상태 전이 시도 (원자적 교환)
    if( !_state.compare_exchange_strong(expected, Rio::SessionState::Active, std::memory_order_acq_rel, std::memory_order_acquire) )
        return false;

    _core = &core;
    _socket = socket;
    _requestQueue = requestQueue;

    return true;
}

//***************************************************************************
// @brief 세션 종료 처리
// @details Active -> Closing -> Closed 단계별 전이를 수행합니다.
//          1. TryChangeState를 통해 Active -> Closing 전환을 시도하며 중복 Close 호출을 차단합니다.
//          2. OnClosing()을 실행하여 자원 정리 절차를 시작합니다.
//          3. Closing -> Closed 전환을 시도하고 성공 시 OnClosed()를 호출합니다.
//          ※ 주의: I/O 카운트(_ioCount)는 여기서 강제로 초기화하지 않으며, 
//                   제출되어 있는 비동기 I/O가 완료 완료(Completion)되면서 자연 감소합니다.
//***************************************************************************
void CRioSession::Close() noexcept
{
    // Active -> Closing 원자적 전이
    if( !TryChangeState(Rio::SessionState::Active, Rio::SessionState::Closing) )
        return;

    OnClosing();

    Rio::SessionState closing = Rio::SessionState::Closing;

    // Closing -> Closed 원자적 전이
    if( _state.compare_exchange_strong(closing, Rio::SessionState::Closed, std::memory_order_acq_rel, std::memory_order_acquire) )
        OnClosed();
}

//***************************************************************************
// @brief SessionId 반환
// @return 고유 세션 식별자
//***************************************************************************
CRioSession::SessionId CRioSession::GetSessionId() const noexcept
{
    return _sessionId;
}

//***************************************************************************
// @brief Active 상태 여부 확인
// @return 세션이 활성(Active) 상태이면 true
//***************************************************************************
bool CRioSession::IsActive() const noexcept
{
    return _state.load(std::memory_order_acquire) == Rio::SessionState::Active;
}

//***************************************************************************
// @brief Closing 상태 여부 확인
// @return 세션이 종료 진행 중(Closing) 상태이면 true
//***************************************************************************
bool CRioSession::IsClosing() const noexcept
{
    return _state.load(std::memory_order_acquire) == Rio::SessionState::Closing;
}

//***************************************************************************
// @brief Closed 상태 여부 확인
// @return 세션이 완전히 종료(Closed)되었으면 true
//***************************************************************************
bool CRioSession::IsClosed() const noexcept
{
    return _state.load(std::memory_order_acquire) == Rio::SessionState::Closed;
}

//***************************************************************************
// @brief 바인딩된 WinSock 소켓 핸들 반환
// @return SOCKET 핸들
//***************************************************************************
SOCKET CRioSession::GetSocket() const noexcept
{
    return _socket;
}

//***************************************************************************
// @brief 바인딩된 RIO Request Queue 핸들 반환
// @return RIO_RQ 핸들
//***************************************************************************
RIO_RQ CRioSession::GetRequestQueue() const noexcept
{
    return _requestQueue;
}

//***************************************************************************
// @brief 바인딩된 CRioCore 참조 포인터 반환
// @return CRioCore 포인터
//***************************************************************************
CRioCore* CRioSession::GetCore() const noexcept
{
    return _core;
}

//***************************************************************************
// @brief 처리 대기 중인 Outstanding I/O 존재 여부 검사
// @return CRioObject의 Outstanding I/O 존재 여부 반환
//***************************************************************************
bool CRioSession::HasOutstandingIo() const noexcept
{
    return CRioObject::HasOutstandingIo();
}

//***************************************************************************
// @brief 단일 버퍼 RIO 수신(Receive) 요청 제출
// @param bufferOwner 슬롯 관리용 CRioBuffer 포인터
// @param slotIndex 할당된 버퍼 슬롯 인덱스
// @param buffer RIO_BUF 데이터
// @param rioEvent 비동기 완료 처리용 Event 포인터
// @param flags RIO 작업 플래그
// @return 제출 성공 시 true, 세션 비활성 또는 제출 실패 시 false
// @details 세션의 Active 상태를 검증한 후 실제 수신 처리를 CRioReceive 유틸리티로 위임합니다.
//***************************************************************************
bool CRioSession::StartReceive(CRioBuffer* bufferOwner, uint32_t slotIndex, const RIO_BUF& buffer, CRioEvent* rioEvent, DWORD flags) noexcept
{
    if( !IsActive() )
        return false;

    if( _core == nullptr )
        return false;

    if( _requestQueue == RIO_INVALID_RQ )
        return false;

    return CRioReceive::Receive(*_core, _requestQueue, buffer, bufferOwner, slotIndex, rioEvent, this, flags);
}

//***************************************************************************
// @brief 확장형 RIO 수신(ReceiveEx) 요청 제출
// @param data 수신 RIO_BUF 배열
// @param dataBufferCount 수신 버퍼 배열 개수 (0 또는 1 지원)
// @param dataBinding 버퍼 슬롯 바인딩 정보
// @param localAddress 로컬 주소 바인딩 버퍼
// @param remoteAddress 원격 주소 바인딩 버퍼
// @param control 제어 버퍼
// @param rioEvent 비동기 완료 처리용 Event 포인터
// @param flags RIO 작업 플래그
// @return 제출 성공 시 true
// @details CRioReceive::ReceiveEx 정책에 따라 다중/확장 버퍼 수신 요청을 커널에 제출합니다.
//***************************************************************************
bool CRioSession::StartReceiveEx(const RIO_BUF* data, ULONG dataBufferCount, const CRioEvent::BufferBinding* dataBinding, const RIO_BUF* localAddress, const RIO_BUF* remoteAddress, const RIO_BUF* control, CRioEvent* rioEvent, DWORD flags) noexcept
{
    if( !IsActive() )
        return false;

    if( _core == nullptr )
        return false;

    if( _requestQueue == RIO_INVALID_RQ )
        return false;

    return CRioReceive::ReceiveEx(*_core, _requestQueue, data, dataBufferCount, dataBinding, localAddress, remoteAddress, control, rioEvent, this, flags);
}

//***************************************************************************
// @brief 단일 버퍼 RIO 송신(Send) 요청 제출
// @param bufferOwner 슬롯 관리용 CRioBuffer 포인터
// @param slotIndex 할당된 버퍼 슬롯 인덱스
// @param buffer RIO_BUF 데이터
// @param rioEvent 비동기 완료 처리용 Event 포인터
// @param flags RIO 작업 플래그
// @return 제출 성공 시 true, 세션 비활성 또는 제출 실패 시 false
// @details 세션의 Active 상태를 확인한 후 실제 송신 처리를 CRioSend 유틸리티로 위임합니다.
//***************************************************************************
bool CRioSession::StartSend(CRioBuffer* bufferOwner, uint32_t slotIndex, const RIO_BUF& buffer, CRioEvent* rioEvent, DWORD flags) noexcept
{
    if( !IsActive() )
        return false;

    if( _core == nullptr )
        return false;

    if( _requestQueue == RIO_INVALID_RQ )
        return false;

    return CRioSend::Send(*_core, _requestQueue, buffer, bufferOwner, slotIndex, rioEvent, this, flags);
}

//***************************************************************************
// @brief 확장형 RIO 송신(SendEx) 요청 제출
// @param data 송신 RIO_BUF 배열
// @param dataBufferCount 송신 버퍼 배열 개수
// @param dataBindings 버퍼 슬롯 바인딩 정보
// @param localAddress 로컬 주소 버퍼
// @param remoteAddress 원격 주소 버퍼
// @param control 제어 버퍼
// @param rioEvent 비동기 완료 처리용 Event 포인터
// @param flags RIO 작업 플래그
// @return 제출 성공 시 true
// @details CRioSend::SendEx 유틸리티 함수를 호출하여 스캐터/개더 및 확장 송신을 수행합니다.
//***************************************************************************
bool CRioSession::StartSendEx(const RIO_BUF* data, ULONG dataBufferCount, const CRioEvent::BufferBinding* dataBindings, const RIO_BUF* localAddress, const RIO_BUF* remoteAddress, const RIO_BUF* control, CRioEvent* rioEvent, DWORD flags) noexcept
{
    if( !IsActive() )
        return false;

    if( _core == nullptr )
        return false;

    if( _requestQueue == RIO_INVALID_RQ )
        return false;

    return CRioSend::SendEx(*_core, _requestQueue, data, dataBufferCount, dataBindings, localAddress, remoteAddress, control, rioEvent, this, flags);
}

//***************************************************************************
// @brief RIO Completion Queue 디스패처
// @param rioEvent 완료된 작업 정보를 담고 있는 CRioEvent 포인터
// @param bytesTransferred 완료된 전송 바이트 수
// @param status 비동기 작업 결과 오류 상태 코드
// @details EventType에 따라 Receive completion 또는 Send completion 이벤트 핸들러로 분기합니다.
//***************************************************************************
void CRioSession::Dispatch(CRioEvent* rioEvent, ULONG bytesTransferred, LONG status)
{
    if( rioEvent == nullptr )
        return;

    switch( rioEvent->GetEventType() )
    {
    case Rio::EventType::Receive:
        OnReceiveCompleted(rioEvent, bytesTransferred, status);
        break;

    case Rio::EventType::Send:
        OnSendCompleted(rioEvent, bytesTransferred, status);
        break;

    default:
        assert(false && "CRioSession::Dispatch unknown EventType");
        break;
    }
}

//***************************************************************************
// @brief Receive 완료 가상 이벤트 핸들러 기본 구현
// @details 기본 동작은 없으며, 파생 클래스에서 오버라이드하여 패킷 조립, 파싱 등을 처리합니다.
//***************************************************************************
void CRioSession::OnReceiveCompleted(CRioEvent* rioEvent, ULONG bytesTransferred, LONG status) noexcept
{
    (void)rioEvent;
    (void)bytesTransferred;
    (void)status;
}

//***************************************************************************
// @brief Send 완료 가상 이벤트 핸들러 기본 구현
// @details 기본 동작은 없으며, 파생 클래스에서 오버라이드하여 전송 완료 처리 및 버퍼 후속 정리를 수행합니다.
//***************************************************************************
void CRioSession::OnSendCompleted(CRioEvent* rioEvent, ULONG bytesTransferred, LONG status) noexcept
{
    (void)rioEvent;
    (void)bytesTransferred;
    (void)status;
}

//***************************************************************************
// @brief Closing 진입 가상 이벤트 핸들러 기본 구현
// @details 세션 종료 절차가 시작될 때 호출되며, 상속받은 세션 클래스에서 필요한 정리 작업을 재정의합니다.
//***************************************************************************
void CRioSession::OnClosing() noexcept
{
}

//***************************************************************************
// @brief Closed 완료 가상 이벤트 핸들러 기본 구현
// @details 세션이 최종 Closed 상태가 되었을 때 호출되며, 상속받은 세션 클래스에서 최종 리소스 해제를 구현합니다.
//***************************************************************************
void CRioSession::OnClosed() noexcept
{
}

//***************************************************************************
// @brief CAS 기반 세션 상태 변경 연산
// @param expected 변경 전 기대 상태
// @param desired 변경 후 목표 상태
// @return 상태 원자적 변경 성공 여부
//***************************************************************************
bool CRioSession::TryChangeState(Rio::SessionState expected, Rio::SessionState desired) noexcept
{
    return _state.compare_exchange_strong(expected, desired, std::memory_order_acq_rel, std::memory_order_acquire);
}