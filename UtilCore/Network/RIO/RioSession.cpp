
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
// @note 정상 라이프사이클에서는 FinalizeClose()가 이미 소켓/송신버퍼를 정리했어야
//       합니다. 여기서는 비정상 경로(Init() 실패 후 소멸 등)에 대비한 방어적 정리만
//       수행합니다. UnregisterSendBuffer()/CloseSocketInternal() 둘 다 idempotent라
//       중복 호출해도 안전합니다.
//***************************************************************************
CRioSession::~CRioSession() noexcept
{
    UnregisterSendBuffer();
    CloseSocketInternal();
}

//***************************************************************************
// @brief 새 세션을 초기화합니다 (Created 상태에서만 허용).
// @param sessionId 고유 세션 ID
// @param core RIO Core 객체 포인터
// @param globalRecvBufferPool 전역 수신 버퍼 풀 포인터
// @param socket 클라이언트 소켓 핸들
// @param requestQueue RIO Request Queue 핸들
// @return bool 성공 시 true. false면 이 세션은 Active로 전이하지 않으므로
//         호출자가 socket/requestQueue를 직접 정리해야 합니다.
//***************************************************************************
bool CRioSession::Init(uint64_t sessionId, CRioCore* core, CRioBuffer* globalRecvBufferPool, SOCKET socket, RIO_RQ requestQueue) noexcept
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

    _closeReason.store(Rio::CloseReason::None, std::memory_order_release);

    {
        PRWriteLockGuard sendWriteGuard(_sendLock, __FUNCTION__);

        _isSending = false;
        _sendBuffer.Clear();
        _recvBuffer.Clear();
    }

    // 이 세션 소유의 _sendBuffer 메모리를 RIO에 등록합니다. FlushSendInternal()의
    // GetRioSendBuffers()가 이 등록된 영역 기준으로 Offset을 계산하므로, 반드시
    // Active로 전이하기 전에(=첫 Send() 호출보다 먼저) 등록이 끝나 있어야 합니다.
    if( !RegisterSendBufferIfNeeded() )
    {
        assert(false && "CRioSession::Init send buffer registration failed");
        return false;
    }

    _state.store(Rio::SessionState::Active, std::memory_order_release);

    OnConnected();
    return true;
}

//***************************************************************************
// @brief 이 세션 소유의 _sendBuffer 메모리를 RIORegisterBuffer()로 등록합니다.
// @return 이미 등록됐거나 새로 등록 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSession::RegisterSendBufferIfNeeded() noexcept
{
    // 이미 등록되어 있다면(예: 방어적 재호출) 그대로 성공 처리
    if( _sendBufferId != RIO_INVALID_BUFFERID ) return true;

    if( _core == nullptr ) return false;

    const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _core->GetRioTable();
    if( rioTable.RIORegisterBuffer == nullptr ) return false;

    char* bufferBegin = _sendBuffer.GetBufferBegin();
    char* bufferEnd = _sendBuffer.GetBufferEnd();

    if( bufferBegin == nullptr || bufferEnd <= bufferBegin ) return false;

    const size_t totalSize = static_cast<size_t>(bufferEnd - bufferBegin);

    // RIORegisterBuffer()의 DataLength 파라미터는 DWORD입니다.
    if( totalSize > static_cast<size_t>((std::numeric_limits<DWORD>::max)()) ) return false;

    const RIO_BUFFERID bufferId = rioTable.RIORegisterBuffer(bufferBegin, static_cast<DWORD>(totalSize));
    if( bufferId == RIO_INVALID_BUFFERID ) return false;

    _sendBufferId = bufferId;
    return true;
}

//***************************************************************************
// @brief 등록했던 _sendBuffer의 RIO 버퍼 ID를 해제합니다.
//***************************************************************************
void CRioSession::UnregisterSendBuffer() noexcept
{
    if( _sendBufferId == RIO_INVALID_BUFFERID ) return;

    if( _core != nullptr )
    {
        const RIO_EXTENSION_FUNCTION_TABLE& rioTable = _core->GetRioTable();

        if( rioTable.RIODeregisterBuffer != nullptr )
            rioTable.RIODeregisterBuffer(_sendBufferId);
    }

    _sendBufferId = RIO_INVALID_BUFFERID;
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
// @note [알려진 트레이드오프] 여기서는 outstanding I/O(진행 중인 RIO 요청)를 기다리지
//       않고 즉시 FinalizeClose()로 넘어가 소켓을 닫습니다. 진행 중인 receive/send가
//       남아 있는 상태에서 closesocket()이 호출될 수 있다는 뜻입니다. 서버 전체
//       종료 시퀀스(CRioServer::Stop())는 그와 별개로 CRioCore::Shutdown() 자체의
//       outstanding I/O drain을 거친 뒤에만 전역 버퍼풀/이벤트풀을 해제하므로
//       UAF로는 이어지지 않는다는 것은 확인했으나, RIO가 이런 식으로 도중에 닫힌
//       RQ의 미완료 요청에 대해 항상 에러 completion을 정상적으로 돌려주는지는
//       별도로 확인이 필요합니다(확인 전까지는 낮은 확률로 이벤트/버퍼 슬롯이
//       완료 통지 없이 방치될 수 있음).
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
// @brief 세션의 모든 리소스(RIO_RQ, 소켓, 송신 버퍼 등록 등)를 안전하게 해제하고
//        연결 해제 콜백을 호출합니다.
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

        // 1. RIO Request Queue 핸들 초기화 (소켓 닫힐 때 RIO 서브시스템이 함께 정리함)
        _requestQueue.store(RIO_INVALID_RQ, std::memory_order_release);

        // 2. 소켓 핸들 완전 정리
        CloseSocketInternal();

        // 3. 이 세션이 등록했던 송신 버퍼 해제. Microsoft 문서상 outstanding
        //    send/receive가 남아있는 동안 deregister하지 말라는 권고가 있으나,
        //    바로 위 CloseSocketInternal()과 같은 이유로 여기서는 즉시 처리합니다
        //    (Close() 주석 참고 — 아직 확인이 필요한 부분).
        UnregisterSendBuffer();
    }

    // 4. 외부 락 범위 밖에서 안전하게 사용자 콜백 호출
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
    Rio::CloseReason failureReason = Rio::CloseReason::None;

    uint32_t slotIndex = Rio::kInvalidSlotIndex;
    CRioEvent* rioEvent = nullptr;

    RIO_BUF rioBuf{};
    CRioCore* core = nullptr;
    CRioBuffer* bufferPool = nullptr;
    RIO_RQ requestQueue = RIO_INVALID_RQ;

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

        // 8. 사전 자원 할당 및 준비 과정에 문제가 없다면 실제 수신(Receive) 요청 수행.
        //    Initialize()/BindBufferSlot()은 여기서 직접 하지 않습니다 —
        //    CRioReceive::Receive() 내부가 전적으로 책임집니다.
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

            // 9. RIO Receive 요청 제출 실패 시 실패 사유 기록.
            //    실패했다면 CRioReceive::Receive() 내부에서 이미 slot/event/IoCount
            //    롤백까지 전부 처리했으므로 여기서 추가로 정리할 것은 없습니다.
            if( !success )
            {
                failureReason = Rio::CloseReason::ReceivePostFailed;
            }
        }
    }

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
    // 세션이 활성 상태가 아니라면(이미 닫혔거나 종료 진행 중이라면) 뒤늦게 도착한
    // 완료 이벤트를 안전하게 차단합니다. IoCount 자체는 CRioCore::ProcessRioResult()의
    // ObjectIoCountGuard가 이 함수 반환 이후 자동으로 감소시키므로 여기서 별도로
    // 손댈 필요가 없습니다.
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

    // BufferBinding이 있으면 Receive(슬롯 풀에서 받아온 버퍼 바인딩이 존재),
    // 없으면 Send(사전 등록된 _sendBuffer를 그대로 참조하므로 별도 바인딩이 없음)로 간주합니다.
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
        // 상대방이 정상 종료(FIN)했을 때. BufferSlot/EventPool 반환은
        // CRioCore::ProcessRioResult()가 completion 처리 후 수행합니다.
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
        // PostReceiveInternal() 실패 시 내부에서 이미 Close()가 호출됩니다.
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
    if( data == nullptr || size == 0 ) return false;
    if( !IsActive() ) return false;

    bool needStartSend = false;
    bool enqueueFailed = false;

    {
        PRWriteLockGuard lockGuard(_sendLock, __FUNCTION__);

        // 락 획득 직후 세션 상태를 재확인 (IsActive() 체크와의 사이에 Close()가 끼어들 수 있음)
        if( _state.load(std::memory_order_acquire) != Rio::SessionState::Active ) return false;

        int64 enqueuedBytes = 0;

        const bool enqueueSuccess = _sendBuffer.Enqueue(
            static_cast<const char*>(data),
            static_cast<int64>(size),
            &enqueuedBytes,
            false);

        if( !enqueueSuccess || enqueuedBytes != static_cast<int64>(size) )
        {
            enqueueFailed = true;
        }
        else if( _isSending )
        {
            return true; // 이미 전송 루프 진행 중이므로 큐잉만 완료
        }
        else
        {
            _isSending = true;
            needStartSend = true;
        }
    }

    // 중요: _sendLock을 보유한 상태에서 Close()를 호출하지 않습니다.
    // OnDisconnected()가 Send()를 호출하는 재진입 deadlock을 방지합니다.
    if( enqueueFailed )
    {
        Close(Rio::CloseReason::SendBufferOverflow);
        return false;
    }

    if( !needStartSend ) return true;

    if( !FlushSendInternal() )
    {
        PRWriteLockGuard lockGuard(_sendLock, __FUNCTION__);
        _isSending = false;
        return false;
    }

    return true;
}

//***************************************************************************
// @brief 송신 링버퍼 데이터를 가져와 RIO 전송을 요청합니다.
// @return 전송 요청 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSession::FlushSendInternal() noexcept
{
    Rio::CloseReason failureReason = Rio::CloseReason::None;

    CRioEvent* rioEvent = nullptr;

    // 링버퍼가 wrap되면 최대 2개 세그먼트가 나올 수 있습니다.
    RIO_BUF rioBufs[2]{};
    int bufferCount = 0;

    CRioCore* core = nullptr;
    RIO_RQ requestQueue = RIO_INVALID_RQ;

    {
        PLockGuard guard(_ioSubmitLock, __FUNCTION__);

        if( _state.load(std::memory_order_acquire) != Rio::SessionState::Active ) return false;

        core = _core;
        requestQueue = _requestQueue.load(std::memory_order_acquire);

        if( core == nullptr || requestQueue == RIO_INVALID_RQ ) return false;

        {
            PRReadLockGuard sendReadGuard(_sendLock, __FUNCTION__);
            bufferCount = _sendBuffer.GetRioSendBuffers(rioBufs, _sendBufferId);
        }

        if( bufferCount <= 0 )
        {
            PRWriteLockGuard sendWriteGuard(_sendLock, __FUNCTION__);
            _isSending = false;
            return true;
        }

        CRioEventPool* eventPool = core->GetEventPool();

        if( eventPool == nullptr )
        {
            failureReason = Rio::CloseReason::InternalError;
        }
        else
        {
            rioEvent = eventPool->Alloc();
            if( rioEvent == nullptr )
            {
                failureReason = Rio::CloseReason::EventPoolExhausted;
            }
        }

        if( failureReason == Rio::CloseReason::None )
        {
            // 중요: RIOSend/RIOSendEx/RIOReceive/RIOReceiveEx는 데이터 버퍼 쪽
            // scatter-gather를 지원하지 않습니다(MS 문서: DataBufferCount는
            // pData가 NULL이 아니면 반드시 1). 링버퍼 wrap으로 bufferCount==2가
            // 나오더라도 이번 호출에서는 첫 세그먼트(rioBufs[0])만 1개로 보냅니다.
            // 두 번째 세그먼트는 이 completion 이후 OnSendCompleted()가
            // FlushSendInternal()을 다시 부를 때(그 시점엔 read 커서가 이동해
            // 있어 wrap이 풀린 상태) 자연스럽게 처리됩니다.
            const ULONG sendBufferCount = 1;

            const bool success = CRioSend::SendEx(
                *core,
                requestQueue,
                &rioBufs[0],
                sendBufferCount,
                nullptr,   // dataBindings: 사전등록 버퍼라 slot ownership 이전 불필요
                nullptr,
                nullptr,
                nullptr,
                rioEvent,
                this,
                0);

            if( !success )
            {
                failureReason = Rio::CloseReason::SendPostFailed;
            }
        }
    }

    if( failureReason != Rio::CloseReason::None )
    {
        Close(failureReason);
        return false;
    }

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
            // 이번 호출은 항상 1세그먼트만 보냈으므로, 남은 데이터(wrap의 나머지
            // 세그먼트 포함)가 있으면 다시 FlushSendInternal()을 호출해 이어서 보냅니다.
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

    CSocketUtils::Close(socketToClose);
}