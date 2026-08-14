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
    // 소멸 시점에는 Outstanding I/O가 반드시 0이어야 안전합니다.
    assert(_outstandingIo.load(std::memory_order_acquire) == 0);

    // 정상적인 라이프사이클에서는 FinalizeClose()가 이미 소켓을 닫았어야 합니다.
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
    // Created 상태의 세션만 초기화 허용 (재사용 지점 전면 배제)
    assert(_state.load(std::memory_order_acquire) == SessionState::Created);
    assert(_outstandingIo.load(std::memory_order_acquire) == 0);

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

    _closeReason.store(CloseReason::None, std::memory_order_release);
    _outstandingIo.store(0, std::memory_order_relaxed);

    {
        PRWriteLockGuard sendWriteGuard(_sendLock, "CRioSession_Init_SendLock");

        _isSending = false;
        _sendBuffer.Clear();
        _recvBuffer.Clear();
    }

    _state.store(SessionState::Active, std::memory_order_release);

    OnConnected();
}

//***************************************************************************
// @brief 지정된 사유로 세션 종료를 요청합니다 (락 내부에서 상태 전이 후 락 밖에서 실행).
// @param reason 세션 종료 사유
//***************************************************************************
void CRioSession::Close(CloseReason reason) noexcept
{
    bool needFinalize = false;

    {
        PLockGuard guard(_ioSubmitLock, "CRioSession_Close");

        SessionState expected = SessionState::Active;

        if( !_state.compare_exchange_strong(expected, SessionState::Closing, std::memory_order_acq_rel, std::memory_order_acquire) )
        {
            return;
        }

        _closeReason.store(reason, std::memory_order_release);

        // 소켓 셧다운 먼저 수행
        ShutdownSocketInternal();

        // 현재 Outstanding I/O가 0이라면 락 탈출 후 Finalize 진행
        if( _outstandingIo.load(std::memory_order_acquire) == 0 )
        {
            needFinalize = true;
        }
    }

    if( needFinalize ) FinalizeClose();
}

//***************************************************************************
// @brief I/O 카운트를 줄이고 0에 도달하면 Closing 상태에서 Finalize를 시도합니다.
//***************************************************************************
void CRioSession::DecrementIoCountNoLock() noexcept
{
    const uint32_t prevCount = _outstandingIo.fetch_sub(1, std::memory_order_acq_rel);

    assert(prevCount > 0);

    if( prevCount == 1 ) TryFinalizeClose();
}

//***************************************************************************
// @brief 세션이 Closing 상태이고 Outstanding I/O가 모두 드레인되었는지 검사 후 최종 정리를 수행합니다.
//***************************************************************************
void CRioSession::TryFinalizeClose() noexcept
{
    if( _state.load(std::memory_order_acquire) != SessionState::Closing ) return;
    if( _outstandingIo.load(std::memory_order_acquire) != 0 ) return;

    FinalizeClose();
}

//***************************************************************************
// @brief 세션의 모든 리소스(RIO_RQ, 소켓 등)를 안전하게 해제하고 연결 해제 콜백을 호출합니다.
//***************************************************************************
void CRioSession::FinalizeClose() noexcept
{
    CloseReason closeReason = CloseReason::InternalError;

    {
        // 중요:
        // 마지막 completion과 새로운 I/O submit 사이의 race를 차단합니다.
        //
        // OutstandingIo == 0인 순간 다른 submit thread가 Active 상태를
        // 확인하여 다시 I/O를 제출하는 것을 막고, Closed 전환과 resource
        // invalidation을 동일한 submit gate 안에서 수행합니다.
        PLockGuard guard(_ioSubmitLock, "CRioSession_FinalizeClose");

        SessionState expected = SessionState::Closing;

        if( !_state.compare_exchange_strong(expected, SessionState::Closed, std::memory_order_acq_rel, std::memory_order_acquire) )
        {
            return;
        }

        closeReason = _closeReason.load(std::memory_order_acquire);

        // 1. RIO Request Queue 핸들 초기화 (소켓 닫힐 때 RIO 서브시스템이 함께 정리함)
        _requestQueue.store(RIO_INVALID_RQ, std::memory_order_release);

        // 2. 소켓 핸들 완전 정리
        CloseSocketInternal();
    }

    // 3. 외부 락 범위 밖에서 안전하게 사용자 콜백 호출
    OnDisconnected(closeReason);
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
// @brief 내부 RIO 비동기 수신 요청을 게시합니다 (데드락 원천 차단: 락 내부에서 Close() 호출 금지).
// @return 게시 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSession::PostReceiveInternal() noexcept
{
    CloseReason failureReason = CloseReason::None;

    uint32_t slotIndex = Rio::kInvalidSlotIndex;
    CRioEvent* rioEvent = nullptr;

    RIO_BUF rioBuf{};
    CRioCore* core = nullptr;
    CRioBuffer* bufferPool = nullptr;
    RIO_RQ requestQueue = RIO_INVALID_RQ;

    {
        // 중요:
        // 여기서 실제 RIO Receive submit까지 동일한 gate를 유지합니다.
        //
        // 단순히 OutstandingIo를 증가시킨 후 락을 풀면
        // Close() -> FinalizeClose()가 먼저 실행되어 socket/RQ가
        // 무효화된 뒤 RIO submit이 수행될 수 있습니다.
        PLockGuard guard(_ioSubmitLock, "CRioSession_PostReceiveInternal");

        if( _state.load(std::memory_order_acquire) != SessionState::Active ) return false;

        core = _core;
        bufferPool = _globalRecvBufferPool;
        requestQueue = _requestQueue.load(std::memory_order_acquire);

        if( bufferPool == nullptr || core == nullptr || requestQueue == RIO_INVALID_RQ ) return false;

        if( !bufferPool->AllocSlot(slotIndex) )
        {
            failureReason = CloseReason::BufferAllocationFailed;
        }
        else if( !bufferPool->GetRioBuffer(slotIndex, rioBuf) )
        {
            bufferPool->FreeSlot(slotIndex);
            slotIndex = Rio::kInvalidSlotIndex;
            failureReason = CloseReason::InternalError;
        }
        else
        {
            CRioEventPool* eventPool = core->GetEventPool();

            if( eventPool == nullptr )
            {
                bufferPool->FreeSlot(slotIndex);
                slotIndex = Rio::kInvalidSlotIndex;
                failureReason = CloseReason::InternalError;
            }
            else
            {
                rioEvent = eventPool->Alloc();
                if( rioEvent == nullptr )
                {
                    bufferPool->FreeSlot(slotIndex);
                    slotIndex = Rio::kInvalidSlotIndex;
                    failureReason = CloseReason::EventPoolExhausted;
                }
            }
        }

        if( failureReason != CloseReason::None )
        {
            // 실패 시 실제 I/O는 제출되지 않았으므로 여기서는 OutstandingIo를 증가시키지 않습니다.
        }
        else
        {
            // 성공 시에만 락 내부에서 Outstanding 증가 처리
            IncrementIoCountNoLock();

            // rioBuf / core / requestQueue snapshot 이후 실제 RIO submit까지
            // _ioSubmitLock을 유지하여 FinalizeClose()와의 race를 차단합니다.
            const bool success = CRioReceive::Receive(
                *core,
                requestQueue,
                rioBuf,
                bufferPool,
                slotIndex,
                rioEvent,
                this,
                0);

            if( !success )
            {
                // CRioReceive::Receive() 내부 rollback은 해당 이벤트/버퍼의
                // submit 실패 정리를 담당합니다.
                DecrementIoCountNoLock();

                failureReason = CloseReason::ReceivePostFailed;
            }
        }
    }

    // 락 외부에서 실패 사유 처리 (자기 자신 데드락 원천 방지)
    if( failureReason != CloseReason::None )
    {
        Close(failureReason);
        return false;
    }

    return true;
}

//***************************************************************************
// @brief CRioObject 완료 이벤트 디스패치 가상 함수 구현
// @param rioEvent 완료된 RIO 이벤트 포인터
// @param bytesTransferred 전송된 바이트 수
// @param status 연산 결과 상태 코드
//***************************************************************************
void CRioSession::Dispatch(
    CRioEvent* rioEvent,
    ULONG bytesTransferred,
    LONG status)
{
    // CRioCore::ProcessRioResult()가 Dispatch 이후
    // BufferBinding의 FreeSlot 및 EventPool::Free를 담당합니다.
    //
    // 따라서 CRioSession에서는 여기서 FreeSlot() 또는
    // ClearBufferBindings()를 수행하면 안 됩니다.
    //
    // 특히 EventPool::Free()의 Reset()은 BufferSlot의 반환까지
    // 책임지지 않으므로 해당 책임은 CRioCore에 유지합니다.

    if( rioEvent == nullptr )
    {
        DecrementIoCountNoLock();
        Close(CloseReason::InternalError);
        return;
    }

    if( status != 0 )
    {
        DecrementIoCountNoLock();
        Close(CloseReason::SocketError);
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

    DecrementIoCountNoLock();
}

//***************************************************************************
// @brief 수신 완료 비동기 이벤트를 처리합니다.
// @param rioEvent 완료된 RIO 이벤트 포인터
// @param bytesTransferred 수신된 바이트 수
//***************************************************************************
void CRioSession::OnReceiveCompleted(
    CRioEvent* rioEvent,
    DWORD bytesTransferred) noexcept
{
    if( rioEvent == nullptr )
    {
        Close(CloseReason::InternalError);
        return;
    }

    const auto& bindings = rioEvent->GetBufferBindings();

    if( bindings.empty() )
    {
        Close(CloseReason::InternalError);
        return;
    }

    CRioBuffer* bufferPool = bindings[0].buffer;
    const uint32_t slotIndex = bindings[0].slotIndex;

    if( bufferPool == nullptr )
    {
        Close(CloseReason::InternalError);
        return;
    }

    if( bytesTransferred == 0 )
    {
        // BufferSlot 반환 및 EventPool 반환은 CRioCore::ProcessRioResult()
        // 에서 completion 처리 후 수행합니다.
        Close(CloseReason::RemoteClosed);
        return;
    }

    void* slotDataPtr = bufferPool->GetSlotAddress(slotIndex);

    if( slotDataPtr == nullptr )
    {
        Close(CloseReason::InternalError);
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
        Close(CloseReason::RingBufferOverflow);
        return;
    }

    ProcessPackets();

    if( IsActive() && !PostReceiveInternal() )
    {
        // PostReceiveInternal()이 실패한 경우 내부에서 이미 Close()가 수행됩니다.
        return;
    }
}

//***************************************************************************
// @brief 수신 링버퍼의 데이터를 파싱하여 완성된 패킷 핸들러를 호출합니다.
//***************************************************************************
void CRioSession::ProcessPackets() noexcept
{
    while( true )
    {
        if( !IsActive() ) break;
        if( _recvBuffer.GetSizeUsed() < sizeof(PacketHeader) ) break;

        PacketHeader header{};
        int64 peekedSize = 0;

        if( !_recvBuffer.Peek(reinterpret_cast<char*>(&header), sizeof(PacketHeader), &peekedSize, false) )
        {
            Close(CloseReason::InternalError);
            break;
        }

        if( peekedSize != static_cast<int64>(sizeof(PacketHeader)) )
        {
            Close(CloseReason::InternalError);
            break;
        }

        if( header.size < sizeof(PacketHeader) || header.size > kMaxPacketSize )
        {
            Close(CloseReason::InvalidPacketHeader);
            break;
        }

        if( _recvBuffer.GetSizeUsed() < header.size ) break;

        if( !_recvBuffer.MoveReadBuffer(sizeof(PacketHeader)) )
        {
            Close(CloseReason::InternalError);
            break;
        }

        const uint16_t payloadSize = static_cast<uint16_t>(header.size - sizeof(PacketHeader));

        if( payloadSize > 0 )
        {
            int64 dequeuedSize = 0;

            const bool dequeueSuccess = _recvBuffer.Dequeue(
                _packetPayloadBuffer,
                payloadSize,
                &dequeuedSize,
                false,
                false);

            if( !dequeueSuccess || dequeuedSize != static_cast<int64>(payloadSize) )
            {
                Close(CloseReason::InternalError);
                break;
            }
        }

        OnPacketReceived(header, _packetPayloadBuffer, payloadSize);

        // 사용자 packet callback에서 Close()가 호출될 수 있습니다.
        if( !IsActive() ) break;
    }
}

//***************************************************************************
// @brief 데이터를 송신 버퍼에 큐잉하고 RIO 전송을 진행합니다 (_sendLock 및 순차 제어 분리).
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
        PRWriteLockGuard lockGuard(_sendLock, "CRioSession_Send");

        // 최초 IsActive() 검사 이후 Close()가 발생했을 수 있으므로
        // 실제 queue 삽입 직전에 한 번 더 상태를 확인합니다.
        if( _state.load(std::memory_order_acquire) != SessionState::Active ) return false;

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

    // 중요:
    // _sendLock을 보유한 상태에서 Close()를 호출하지 않습니다.
    // OnDisconnected()가 Send()를 호출하는 경우의 재진입 deadlock을 방지합니다.
    if( enqueueFailed )
    {
        Close(CloseReason::SendBufferOverflow);
        return false;
    }

    if( !needStartSend ) return true;

    // 락 외부에서 실제 RIO 전송 제출 수행
    if( !FlushSendInternal() )
    {
        PRWriteLockGuard lockGuard(_sendLock, "CRioSession_Send_Fail");
        _isSending = false;
        return false;
    }

    return true;
}

//***************************************************************************
// @brief 송신 링버퍼 데이터를 가져와 RIO 전송을 요청합니다 (락 외부에서 호출되는 내부 함수).
// @return 전송 요청 성공 시 true, 실패 시 false
//***************************************************************************
bool CRioSession::FlushSendInternal() noexcept
{
    CloseReason failureReason = CloseReason::None;

    CRioEvent* rioEvent = nullptr;

    RIO_BUF rioBufs[2]{};
    int bufferCount = 0;

    CRioCore* core = nullptr;
    RIO_RQ requestQueue = RIO_INVALID_RQ;

    {
        // 실제 SendEx() submit까지 _ioSubmitLock을 유지합니다.
        // 이로 인해 FinalizeClose()가 submit 중간에 socket/RQ를
        // 무효화하는 race를 방지합니다.
        PLockGuard guard(_ioSubmitLock, "CRioSession_FlushSendInternal");

        if( _state.load(std::memory_order_acquire) != SessionState::Active ) return false;

        core = _core;
        requestQueue = _requestQueue.load(std::memory_order_acquire);

        if( core == nullptr || requestQueue == RIO_INVALID_RQ ) return false;

        {
            PRReadLockGuard sendReadGuard(_sendLock, "CRioSession_FlushSendInternal_SendLock");
            bufferCount = _sendBuffer.GetRioSendBuffers(rioBufs, _sendBufferId);
        }

        if( bufferCount <= 0 )
        {
            PRWriteLockGuard sendWriteGuard(_sendLock, "CRioSession_FlushSendInternal_ResetSending");
            _isSending = false;
            return true;
        }

        CRioEventPool* eventPool = core->GetEventPool();

        if( eventPool == nullptr )
        {
            failureReason = CloseReason::InternalError;
        }
        else
        {
            rioEvent = eventPool->Alloc();
            if( rioEvent == nullptr ) failureReason = CloseReason::EventPoolExhausted;
        }

        if( failureReason != CloseReason::None )
        {
            // event/buffer가 이미 할당된 경우 여기서 정리해야 합니다.
            // eventPool::Alloc 이후 실패 경로는 현재 Alloc 성공 상태이므로
            // submit 이전 rollback 책임을 이 함수가 유지합니다.
            if( rioEvent != nullptr )
            {
                if( CRioEventPool* eventPool = core->GetEventPool(); eventPool != nullptr )
                {
                    eventPool->Free(rioEvent);
                }

                rioEvent = nullptr;
            }

            return false;
        }

        // 성공 시에만 락 내부에서 Outstanding 증가 처리
        IncrementIoCountNoLock();

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

        if( !success )
        {
            // CRioSend::SendEx() 내부 rollback에서 EventPool/BufferBinding
            // 정리를 수행하도록 책임을 유지합니다.
            DecrementIoCountNoLock();

            failureReason = CloseReason::SendPostFailed;
        }
    }

    if( failureReason != CloseReason::None )
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
void CRioSession::OnSendCompleted(
    CRioEvent* rioEvent,
    DWORD bytesTransferred) noexcept
{
    if( rioEvent == nullptr )
    {
        Close(CloseReason::InternalError);
        return;
    }

    bool needFlush = false;
    bool invalidCompletion = false;

    {
        PRWriteLockGuard lockGuard(_sendLock, "CRioSession_OnSendCompleted");

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
        Close(CloseReason::InternalError);
        return;
    }

    if( needFlush && !FlushSendInternal() )
    {
        PRWriteLockGuard lockGuard(_sendLock, "CRioSession_OnSendCompleted_Fail");
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