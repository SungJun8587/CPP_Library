
//***************************************************************************
// RioSession.cpp : implementation of the CRioSession class.
//
//***************************************************************************

#include "pch.h"
#include "RioSession.h"

#include <cassert>
#include <limits>

//***************************************************************************
// @brief CRioSession 생성자
//***************************************************************************
CRioSession::CRioSession() = default;

//***************************************************************************
// @brief CRioSession 소멸자
//***************************************************************************
CRioSession::~CRioSession() noexcept
{
    CloseSocketInternal();
}

//***************************************************************************
// @brief 새 세션을 초기화합니다 (Created 상태에서만 허용).
// @param sessionId 고유 세션 ID
// @param core RIO Core 객체 포인터
// @param globalRecvBufferPool 전역 수신 버퍼 풀 포인터
// @param socket 클라이언트 소켓 핸들
// @param requestQueue RIO Request Queue 핸들
// @param sendBufferId 송신 버퍼 ID
//***************************************************************************
void CRioSession::Init(
    uint64_t sessionId,
    CRioCore* core,
    CRioBuffer* globalRecvBufferPool,
    SOCKET socket,
    RIO_RQ requestQueue,
    RIO_BUFFERID sendBufferId) noexcept
{
    assert(_state.load(std::memory_order_acquire) == Rio::SessionState::Created);

    assert(core != nullptr);
    assert(globalRecvBufferPool != nullptr);
    assert(socket != INVALID_SOCKET);
    assert(requestQueue != RIO_INVALID_RQ);

    _sessionId = sessionId;
    _core = core;
    _globalRecvBufferPool = globalRecvBufferPool;
    _socket.store(socket, std::memory_order_release);
    _requestQueue.store(requestQueue, std::memory_order_release);
    _sendBufferId = sendBufferId;

    _closeReason.store(Rio::CloseReason::None, std::memory_order_release);

    {
        PRWriteLockGuard sendWriteGuard(_sendLock, __FUNCTION__);

        _isSending = false;
        _sendBuffer.Clear();
        _recvBuffer.Clear();
    }

    _state.store(Rio::SessionState::Active, std::memory_order_release);

    OnConnected();
}

//***************************************************************************
// @brief 외부(NetService 등)에서 요청한 세션 강제 종료 처리
// @param cause 종료 사유 메시지
//***************************************************************************
void CRioSession::Disconnect(const TCHAR* cause)
{
    Close(Rio::CloseReason::ForcedClose);
}

//***************************************************************************
// @brief 지정된 사유로 세션 종료를 요청합니다 (락 내부에서 상태 전이 후 락 밖에서 실행).
// @param reason 세션 종료 사유
//***************************************************************************
void CRioSession::Close(Rio::CloseReason reason) noexcept
{
    {
        PLockGuard guard(_ioSubmitLock, __FUNCTION__);

        Rio::SessionState expected = Rio::SessionState::Active;

        if( !_state.compare_exchange_strong(expected, Rio::SessionState::Closing, std::memory_order_acq_rel, std::memory_order_acquire) )
        {
            return;
        }

        _closeReason.store(reason, std::memory_order_release);

        // 소켓 셧다운 먼저 수행
        ShutdownSocketInternal();
    }

    // 즉시 최종 정리 진행
    FinalizeClose();
}

//***************************************************************************
// @brief 세션의 모든 리소스(RIO_RQ, 소켓 등)를 안전하게 해제하고 연결 해제 콜백을 호출합니다.
//***************************************************************************
void CRioSession::FinalizeClose() noexcept
{
    Rio::CloseReason closeReason = Rio::CloseReason::InternalError;

    {
        PLockGuard guard(_ioSubmitLock, __FUNCTION__);

        Rio::SessionState expected = Rio::SessionState::Closing;

        if( !_state.compare_exchange_strong(expected, Rio::SessionState::Closed, std::memory_order_acq_rel, std::memory_order_acquire) )
        {
            return;
        }

        closeReason = _closeReason.load(std::memory_order_acquire);

        // 1. RIO Request Queue 핸들 초기화
        _requestQueue.store(RIO_INVALID_RQ, std::memory_order_release);

        // 2. 소켓 핸들 완전 정리
        CloseSocketInternal();
    }

    // 3. 외부 락 범위 밖에서 안전하게 사용자 콜백 호출
    OnDisconnected(closeReason);

    // CSession에 정의된 상위 통지 함수 호출 -> CNetService의 ReleaseSession 자동 연동!
    CSession::OnDisconnected();
}

//***************************************************************************
// @brief 최초 비동기 수신(Receive) 요청을 게시합니다.
// @return 게시 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSession::PostInitialReceive() noexcept
{
    return PostReceiveInternal();
}

//***************************************************************************
// @brief 내부 RIO 비동기 수신 요청을 게시합니다.
// @return 게시 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSession::PostReceiveInternal() noexcept
{
    // 실패 원인을 기록하기 위한 변수 선언 (초기값: 없음)
    Rio::CloseReason failureReason = Rio::CloseReason::None;

    // 수신 버퍼 슬롯 인덱스와 RIO 이벤트 객체 포인터 초기화
    uint32_t slotIndex = Rio::kInvalidSlotIndex;
    CRioEvent* rioEvent = nullptr;

    // RIO 전송/수신에 필요한 변수들 선언
    RIO_BUF rioBuf{};
    CRioCore* core = nullptr;
    CRioBuffer* bufferPool = nullptr;
    RIO_RQ requestQueue = RIO_INVALID_RQ;

    // 임계 구역(Critical Section) 시작: I/O 제출 락 가드 설정
    {
        PLockGuard guard(_ioSubmitLock, __FUNCTION__);

        // 1. 세션 상태 확인: 현재 세션이 Active 상태가 아니라면 수신 요청을 중단하고 실패 반환
        if( _state.load(std::memory_order_acquire) != Rio::SessionState::Active ) return false;

        // 2. 핵심 세션 멤버 변수 캐싱 (Core, 수신 버퍼 풀, 요청 큐 핸들)
        core = _core;
        bufferPool = _globalRecvBufferPool;
        requestQueue = _requestQueue.load(std::memory_order_acquire);

        // 3. 필수 리소스 포인터 및 큐 유효성 검사
        if( bufferPool == nullptr || core == nullptr || requestQueue == RIO_INVALID_RQ ) return false;

        // 4. 수신 버퍼 풀에서 사용할 슬롯(Slot) 할당 시도
        if( !bufferPool->AllocSlot(slotIndex) )
        {
            failureReason = Rio::CloseReason::BufferAllocationFailed;
        }
        // 5. 할당받은 슬롯의 실제 RIO 버퍼 정보(RIO_BUF) 획득 시도
        else if( !bufferPool->GetRioBuffer(slotIndex, rioBuf) )
        {
            // 정보 획득 실패 시, 앞서 할당받았던 슬롯을 곧바로 반납하고 상태 초기화
            bufferPool->FreeSlot(slotIndex);
            slotIndex = Rio::kInvalidSlotIndex;
            failureReason = Rio::CloseReason::InternalError;
        }
        else
        {
            // 6. 코어 객체로부터 RIO 이벤트 풀(Event Pool) 가져오기
            CRioEventPool* eventPool = core->GetEventPool();

            if( eventPool == nullptr )
            {
                // 이벤트 풀이 존재하지 않으면, 슬롯 반납 및 내부 에러 처리
                bufferPool->FreeSlot(slotIndex);
                slotIndex = Rio::kInvalidSlotIndex;
                failureReason = Rio::CloseReason::InternalError;
            }
            else
            {
                // 7. 이벤트 풀에서 비동기 I/O 처리를 위한 이벤트 객체 할당 받기
                rioEvent = eventPool->Alloc();
                if( rioEvent == nullptr )
                {
                    // 이벤트 할당 실패(풀 고갈) 시, 슬롯 반납 및 사유 기록
                    bufferPool->FreeSlot(slotIndex);
                    slotIndex = Rio::kInvalidSlotIndex;
                    failureReason = Rio::CloseReason::EventPoolExhausted;
                }
            }
        }

        // 8. 사전 자원 할당 및 준비 과정에 문제가 없다면 실제 수신(Receive) 요청 수행
        if( failureReason == Rio::CloseReason::None )
        {
            const bool success = CRioReceive::Receive(
                *core,
                requestQueue,
                rioBuf,
                bufferPool,
                slotIndex,
                rioEvent,
                this,
                0);

            // 9. RIO Receive 요청 제출 실패 시 실패 사유 기록
            if( !success )
            {
                failureReason = Rio::CloseReason::ReceivePostFailed;
            }
        }
    } // 임계 구역(Critical Section) 종료 (락 해제)

    // 10. 과정 중 발생한 실패 사유가 존재한다면 세션을 지정된 사유로 종료하고 false 반환
    if( failureReason != Rio::CloseReason::None )
    {
        Close(failureReason);
        return false;
    }

    // 11. 모든 수신 포스트 과정 성공
    return true;
}

//***************************************************************************
// @brief CRioObject 완료 이벤트 디스패치 가상 함수 구현
// @param rioEvent 완료된 RIO 이벤트 포인터
// @param bytesTransferred 전송된 바이트 수
// @param status 연산 결과 상태 코드
//***************************************************************************
void CRioSession::Dispatch(CRioEvent* rioEvent, ULONG bytesTransferred, LONG status)
{
    // 세션이 활성 상태가 아니라면(이미 닫혔거나 종료 진행 중이라면) 뒤늦게 도착한 완료 이벤트를 안전하게 차단
    if( !IsActive() )
    {
        return;
    }

    if( rioEvent == nullptr )
    {
        Close(Rio::CloseReason::InternalError);
        return;
    }

    if( status != 0 )
    {
        Close(Rio::CloseReason::SocketError);
        return;
    }

    if( !rioEvent->GetBufferBindings().empty() )
    {
        OnReceiveCompleted(rioEvent, bytesTransferred);
    }
    else
    {
        OnSendCompleted(rioEvent, bytesTransferred);
    }
}

//***************************************************************************
// @brief 수신 완료 비동기 이벤트를 처리합니다.
// @param rioEvent 완료된 RIO 이벤트 포인터
// @param bytesTransferred 수신된 바이트 수
//***************************************************************************
void CRioSession::OnReceiveCompleted(CRioEvent* rioEvent, DWORD bytesTransferred) noexcept
{
    if( rioEvent == nullptr )
    {
        Close(Rio::CloseReason::InternalError);
        return;
    }

    const auto& bindings = rioEvent->GetBufferBindings();

    if( bindings.empty() )
    {
        Close(Rio::CloseReason::InternalError);
        return;
    }

    CRioBuffer* bufferPool = bindings[0].buffer;
    const uint32_t slotIndex = bindings[0].slotIndex;

    if( bufferPool == nullptr )
    {
        Close(Rio::CloseReason::InternalError);
        return;
    }

    if( bytesTransferred == 0 )
    {
        Close(Rio::CloseReason::RemoteClosed);
        return;
    }

    void* slotDataPtr = bufferPool->GetSlotAddress(slotIndex);

    if( slotDataPtr == nullptr )
    {
        Close(Rio::CloseReason::InternalError);
        return;
    }

    int64 enqueuedBytes = 0;

    const bool enqueueSuccess = _recvBuffer.Enqueue(
        static_cast<const char*>(slotDataPtr),
        static_cast<int64>(bytesTransferred),
        &enqueuedBytes,
        false);

    if( !enqueueSuccess || enqueuedBytes != static_cast<int64>(bytesTransferred) )
    {
        Close(Rio::CloseReason::RingBufferOverflow);
        return;
    }

    OnDataReceived();

    if( IsActive() && !PostReceiveInternal() )
    {
        return;
    }
}

//***************************************************************************
// @brief 데이터를 송신 버퍼에 큐잉하고 RIO 전송을 진행합니다.
// @param data 전송할 데이터 버퍼 포인터
// @param size 전송할 데이터 크기 (바이트)
// @return 전송 큐잉 및 처리 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSession::Send(const void* data, uint16_t size) noexcept
{
    // 1. 입력된 데이터 포인터가 유효하지 않거나 크기가 0인 경우 즉시 실패 반환
    if( data == nullptr || size == 0 ) return false;

    // 2. 현재 세션이 활성(Active) 상태가 아니라면 전송 요청을 거부하고 실패 반환
    if( !IsActive() ) return false;

    // 3. 실제 비동기 전송 시작이 필요한지 여부와 큐 적재 실패 여부를 나타내는 플래그 초기화
    bool needStartSend = false;
    bool enqueueFailed = false;

    // 4. 송신 전용 쓰기 락(Write Lock) 구간 시작 (멀티스레드 환경에서 송신 버퍼 동기화 보호)
    {
        PRWriteLockGuard lockGuard(_sendLock, __FUNCTION__);

        // 5. 락 획득 직후 세션 상태를 재확인하여 Active 상태가 아니면 실패 반환
        if( _state.load(std::memory_order_acquire) != Rio::SessionState::Active ) return false;

        int64 enqueuedBytes = 0;

        // 6. 내부 송신 버퍼(`_sendBuffer`)에 보낼 데이터를 큐 형태로 적재(Enqueue) 시도
        const bool enqueueSuccess = _sendBuffer.Enqueue(
            static_cast<const char*>(data),
            static_cast<int64>(size),
            &enqueuedBytes,
            false);

        // 7. 큐 적재가 실패했거나 요청한 크기만큼 완전히 적재되지 못한 경우 버퍼 오버플로우로 처리
        if( !enqueueSuccess || enqueuedBytes != static_cast<int64>(size) )
        {
            enqueueFailed = true;
        }
        // 8. 이미 다른 작업에 의해 전송이 진행 중(`_isSending == true`)인 경우, 큐에 쌓기만 하고 성공 반환
        else if( _isSending )
        {
            return true;
        }
        // 9. 현재 전송 중이 아니라면, 이번 전송을 주도하기 위해 송신 상태로 전환하고 플래그 설정
        else
        {
            _isSending = true;
            needStartSend = true;
        }
    } // 송신 락 구간 종료

    // 10. 큐 적재 실패 플래그가 켜져 있다면 SendBufferOverflow 사유로 세션 종료 후 실패 반환
    if( enqueueFailed )
    {
        Close(Rio::CloseReason::SendBufferOverflow);
        return false;
    }

    // 11. 이번 호출에서 전송을 직접 시작할 필요가 없다면 그대로 성공 반환
    if( !needStartSend ) return true;

    // 12. 실제 송신 플러시(`FlushSendInternal`)를 호출하여 RIO 전송 요청 게시 수행
    if( !FlushSendInternal() )
    {
        // 13. 전송 게시 실패 시 다시 송신 락을 획득하여 전송 중 상태(`_isSending`)를 원상 복구(`false`) 후 실패 반환
        //  - 전송 실패 시 _isSending 상태를 안전하게 복구하여, 다른 스레드가 전송 중인 것으로 오인해 데이터 전송이 누락되는 경합(Race Condition) 방지
        PRWriteLockGuard lockGuard(_sendLock, __FUNCTION__);
        _isSending = false;
        return false;
    }

    // 14. 모든 송신 요청 및 적재 과정 성공
    return true;
}

//***************************************************************************
// @brief 송신 링버퍼 데이터를 가져와 RIO 전송을 요청합니다.
// @return 전송 요청 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSession::FlushSendInternal() noexcept
{
    // 실패 원인을 기록하기 위한 변수 선언 (초기값: 없음)
    Rio::CloseReason failureReason = Rio::CloseReason::None;

    // RIO 비동기 전송에 사용할 이벤트 객체 포인터 초기화
    CRioEvent* rioEvent = nullptr;

    // 링 버퍼 등에서 가져올 RIO 전송용 버퍼 배열(최대 2개, Scatter/Gather 대응)과 개수 초기화
    RIO_BUF rioBufs[2]{};
    int bufferCount = 0;

    // RIO 코어와 요청 큐 핸들을 저장할 변수 초기화
    CRioCore* core = nullptr;
    RIO_RQ requestQueue = RIO_INVALID_RQ;

    // 임계 구역(Critical Section) 시작: I/O 제출 락 가드 설정 (동시에 여러 I/O 제출 방지)
    {
        PLockGuard guard(_ioSubmitLock, __FUNCTION__);

        // 1. 세션 상태 확인: 현재 세션이 Active 상태가 아니라면 전송을 중단하고 실패 반환
        if( _state.load(std::memory_order_acquire) != Rio::SessionState::Active ) return false;

        // 2. 핵심 세션 멤버 변수 캐싱 (Core, 요청 큐 핸들)
        core = _core;
        requestQueue = _requestQueue.load(std::memory_order_acquire);

        // 3. 필수 리소스 포인터 및 큐 유효성 검사
        if( core == nullptr || requestQueue == RIO_INVALID_RQ ) return false;

        // 4. 송신 읽기 락(Read Lock) 구간: 송신 버퍼로부터 전송할 RIO 버퍼 정보(시그니처 및 데이터) 안전하게 획득
        {
            PRReadLockGuard sendReadGuard(_sendLock, __FUNCTION__);
            bufferCount = _sendBuffer.GetRioSendBuffers(rioBufs, _sendBufferId);
        }

        // 5. 보낼 데이터가 없는 경우(bufferCount <= 0): 더 이상 보낼 내용이 없으므로 전송 중 상태(`_isSending`)를 해제하고 종료
        if( bufferCount <= 0 )
        {
            PRWriteLockGuard sendWriteGuard(_sendLock, __FUNCTION__);
            _isSending = false;
            return true;
        }

        // 6. 코어 객체로부터 RIO 이벤트 풀(Event Pool) 가져오기
        CRioEventPool* eventPool = core->GetEventPool();

        if( eventPool == nullptr )
        {
            failureReason = Rio::CloseReason::InternalError;
        }
        else
        {
            // 7. 이벤트 풀에서 비동기 I/O 처리를 위한 이벤트 객체 할당 받기
            rioEvent = eventPool->Alloc();
            if( rioEvent == nullptr )
            {
                // 이벤트 할당 실패(풀 고갈) 시 사유 기록
                failureReason = Rio::CloseReason::EventPoolExhausted;
            }
        }

        // 8. 사전 자원 할당 및 버퍼 조회에 문제가 없다면 실제 확장 송신(SendEx) 요청 수행
        if( failureReason == Rio::CloseReason::None )
        {
            const bool success = CRioSend::SendEx(
                *core,
                requestQueue,
                rioBufs,
                static_cast<ULONG>(bufferCount),
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                rioEvent,
                this,
                0);
                /*

            const bool success = CRioSend::Send(
                *core,
                requestQueue,
                rioBufs[0],          // 👈 단일 RIO_BUF 전달
                nullptr,             // bufferOwner (필요시 전달)
                Rio::kInvalidSlotIndex, // slotIndex
                rioEvent,
                this,
                0);
            */

            // 9. RIO SendEx 요청 제출 실패 시 실패 사유 기록
            if( !success )
            {
                failureReason = Rio::CloseReason::SendPostFailed;
            }
        }
    } // 임계 구역(Critical Section) 종료 (락 해제)

    // 10. 과정 중 발생한 실패 사유가 존재한다면 세션을 지정된 사유로 종료하고 false 반환
    if( failureReason != Rio::CloseReason::None )
    {
        Close(failureReason);
        return false;
    }

    // 11. 송신 플러시(전송 게시) 과정 성공
    return true;
}

//***************************************************************************
// @brief 송신 완료 비동기 이벤트를 처리합니다.
// @param rioEvent 완료된 송신 RIO 이벤트 포인터
// @param bytesTransferred 전송된 바이트 수
//***************************************************************************
void CRioSession::OnSendCompleted(CRioEvent* rioEvent, DWORD bytesTransferred) noexcept
{
    if( rioEvent == nullptr )
    {
        Close(Rio::CloseReason::InternalError);
        return;
    }

    bool needFlush = false;
    bool invalidCompletion = false;

    {
        PRWriteLockGuard lockGuard(_sendLock, __FUNCTION__);

        const int64 currentQueuedBytes = _sendBuffer.GetSizeUsed();

        if( bytesTransferred == 0 && currentQueuedBytes > 0 )
        {
            invalidCompletion = true;
        }
        else if( bytesTransferred > static_cast<DWORD>(currentQueuedBytes) )
        {
            invalidCompletion = true;
        }
        else if( bytesTransferred > 0 )
        {
            if( !_sendBuffer.MoveReadBuffer(bytesTransferred) )
            {
                invalidCompletion = true;
            }
        }

        if( !invalidCompletion )
        {
            if( IsActive() && _sendBuffer.GetSizeUsed() > 0 )
            {
                needFlush = true;
            }
            else
            {
                _isSending = false;
            }
        }
        else
        {
            _isSending = false;
        }
    }

    if( invalidCompletion )
    {
        Close(Rio::CloseReason::InternalError);
        return;
    }

    if( needFlush && !FlushSendInternal() )
    {
        PRWriteLockGuard lockGuard(_sendLock, __FUNCTION__);
        _isSending = false;
    }
}

//***************************************************************************
// @brief 소켓 셧다운 내부 처리
//***************************************************************************
void CRioSession::ShutdownSocketInternal() noexcept
{
    const SOCKET socket = _socket.load(std::memory_order_acquire);

    if( socket != INVALID_SOCKET ) ::shutdown(socket, SD_BOTH);
}

//***************************************************************************
// @brief 소켓 닫기 내부 처리
//***************************************************************************
void CRioSession::CloseSocketInternal() noexcept
{
    const SOCKET socketToClose = _socket.exchange(INVALID_SOCKET, std::memory_order_acq_rel);

    if( socketToClose != INVALID_SOCKET ) ::closesocket(socketToClose);
}