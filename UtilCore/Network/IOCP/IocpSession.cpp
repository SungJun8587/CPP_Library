
//***************************************************************************
// IocpSession.cpp: implementation of the CIocpSession class.
//
//***************************************************************************

#include "pch.h"
#include "IocpSession.h"

//***************************************************************************
// @brief CIocpSession 생성자
// @details 기본 버퍼 크기(Iocp::BUFFER_SIZE_DEFAULT)로 수신 CRingBuffer를 초기화합니다.
//***************************************************************************
CIocpSession::CIocpSession() : _recvBuffer(Iocp::BUFFER_SIZE_DEFAULT)
{
    _socket = CSocketUtils::CreateSocket();
    if( _socket == INVALID_SOCKET )
    {
        // TODO: 로그 기록 등 소켓 생성 실패 예외 처리
        LOG_INFO(_T("CIocpSession::CIocpSession Failed to create socket"));
    }
}

//***************************************************************************
// @brief CIocpSession 소멸자
//***************************************************************************
CIocpSession::~CIocpSession()
{
    CSocketUtils::Close(_socket);
}

//***************************************************************************
// @brief IOCP Dispatch 함수 구현 (CIocpObject)
// @param iocpEvent 완료 통지된 IOCP 이벤트
// @param numOfBytes 전송/수신된 바이트 수
//***************************************************************************
void CIocpSession::Dispatch(CIocpEvent* iocpEvent, int32 numOfBytes)
{
    switch( iocpEvent->eventType )
    {
    case Iocp::EventType::Connect:
        ProcessConnect();
        break;

    case Iocp::EventType::Disconnect:
        ProcessDisconnect();
        break;

    case Iocp::EventType::Recv:
        ProcessRecv(numOfBytes);
        break;

    case Iocp::EventType::Send:
        ProcessSend(numOfBytes);
        break;

    default:
        ASSERT_CRASH(false);
        break;
    }
}

//***************************************************************************
// @brief 클라이언트 연결 성공 후 초기화 로직
//***************************************************************************
void CIocpSession::ProcessConnect()
{
    _connected.store(true);

    // 세션 재사용(AcceptEx) 시 이전 연결의 잔여 데이터 오염 방지
    _recvBuffer.Clear();

    // 상위 레이어 이벤트 호출
    OnConnected();

    // 첫 비동기 수신(WSARecv) 등록
    RegisterRecv();
}

//***************************************************************************
// @brief 비동기 데이터 수신(WSARecv) 등록 (CRingBuffer 제로카피)
//***************************************************************************
void CIocpSession::RegisterRecv()
{
    if( IsConnected() == false )
        return;

    _recvEvent.Init();
    _recvEvent.owner = CIocpObject::shared_from_this(); // I/O 완료 시까지 수명 보장 (Ref +1)

    // CRingBuffer에서 WSARecv에 전달할 쓰기 가능 WSABUF 추출 (최대 2개 청크)
    WSABUF wsaBufs[2];
    int32 bufferCount = _recvBuffer.GetWSARecvBuffers(wsaBufs);

    if( bufferCount == 0 )
    {
        // 링버퍼 공간 부족 (Overflow)
        _recvEvent.owner = nullptr;
        Disconnect(L"RecvBuffer Full (GetWSARecvBuffers returned 0)");
        return;
    }

    DWORD numOfBytes = 0;
    DWORD flags = 0;

    if( ::WSARecv(_socket, wsaBufs, static_cast<DWORD>(bufferCount), OUT & numOfBytes, &flags, static_cast<LPOVERLAPPED>(&_recvEvent), nullptr) == SOCKET_ERROR )
    {
        int32 errorCode = ::WSAGetLastError();
        if( errorCode != WSA_IO_PENDING )
        {
            _recvEvent.owner = nullptr;
            Disconnect(L"WSARecv Register Failed");
        }
    }
}

//***************************************************************************
// @brief 수신 완료 처리 (WSARecv 완료 통지 시 호출)
// @param numOfBytes 수신된 데이터 바이트 수 (0인 경우 정상 연결 끊김)
// @note 링버퍼 경계 래핑(Wrap-around)으로 인한 메모리 침범 방지를 위해
//       GetSizeDirectDequeueAble() 크기만 OnRecv로 전달하며, 
//       누적된 모든 패킷을 소진할 때까지 while 루프를 순회합니다.
//***************************************************************************
void CIocpSession::ProcessRecv(int32 numOfBytes)
{
    if( numOfBytes == 0 )
    {
        _recvEvent.owner = nullptr;
        Disconnect(L"Recv 0 Bytes (Disconnected)");
        return;
    }

    // 1. 링버퍼 쓰기 커서 수신된 바이트 수만큼 이동
    if( _recvBuffer.MoveWriteBuffer(numOfBytes) == false )
    {
        _recvEvent.owner = nullptr;
        Disconnect(L"RecvBuffer MoveWriteBuffer Failed");
        return;
    }

    // 2. 수신 버퍼 처리 루프 (누적된 패킷 소진)
    while( true )
    {
        int64 dataSize = _recvBuffer.GetSizeUsed();
        if( dataSize <= 0 )
            break;

        // 경계 래핑(Wrap-around) 오버런 방지: 연속 메모리 청크 크기 전달
        int64 directSize = _recvBuffer.GetSizeDirectDequeueAble();
        BYTE* readPos = reinterpret_cast<BYTE*>(_recvBuffer.GetReadBuffer());

        // 콘텐츠 레이어로 데이터 전달
        int32 processLen = OnRecv(readPos, static_cast<int32>(directSize));

        if( processLen < 0 || directSize < processLen )
        {
            _recvEvent.owner = nullptr;
            Disconnect(L"OnRecv Process Length Error");
            return;
        }

        // 완성된 패킷이 없어서 더 이상 처리를 진행할 수 없는 경우 루프 탈출
        if( processLen == 0 )
            break;

        // 처리한 바이트 수만큼 읽기 커서 이동
        if( _recvBuffer.MoveReadBuffer(processLen) == false )
        {
            _recvEvent.owner = nullptr;
            Disconnect(L"RecvBuffer MoveReadBuffer Failed");
            return;
        }
    }

    _recvEvent.owner = nullptr; // OnRecv 처리 완료 후 수명 해제

    // 다음 데이터 수신 대기
    RegisterRecv();
}

//***************************************************************************
// @brief 패킷 전송 요청 (Thread-safe)
// @param sendBuffer 전송할 패킷 버퍼
//***************************************************************************
void CIocpSession::Send(CSendBufferRef sendBuffer)
{
    if( IsConnected() == false || sendBuffer == nullptr )
        return;

    bool registerSend = false;

    {
        std::lock_guard<std::mutex> guard(_lock);
        _sendQueue.push_back(sendBuffer);

        // 현재 진행 중인 WSASend가 없다면 등록 수행
        if( _sendRegistered.exchange(true) == false )
        {
            registerSend = true;
        }
    }

    if( registerSend )
    {
        RegisterSend();
    }
}

//***************************************************************************
// @brief 비동기 데이터 송신(WSASend) 등록 (Scatter-Gather 패턴)
//***************************************************************************
void CIocpSession::RegisterSend()
{
    if( IsConnected() == false )
        return;

    _sendEvent.Init();
    _sendEvent.owner = CIocpObject::shared_from_this(); // Ref +1

    // Scatter-Gather: SendQueue에 쌓인 모든 버퍼를 꺼내 1회 WSASend로 전송
    {
        std::lock_guard<std::mutex> guard(_lock);
        _sendEvent.sendBuffers.swap(_sendQueue); // 원본 ref count 보장용 백업
    }

    CVector<WSABUF> wsaBufs;
    wsaBufs.reserve(_sendEvent.sendBuffers.size());

    for( CSendBufferRef& sendBuffer : _sendEvent.sendBuffers )
    {
        WSABUF wsaBuf;
        wsaBuf.buf = reinterpret_cast<char*>(sendBuffer->Buffer());
        wsaBuf.len = static_cast<ULONG>(sendBuffer->WriteSize());
        wsaBufs.push_back(wsaBuf);
    }

    DWORD numOfBytes = 0;
    if( ::WSASend(_socket, wsaBufs.data(), static_cast<DWORD>(wsaBufs.size()), OUT & numOfBytes, 0, static_cast<LPOVERLAPPED>(&_sendEvent), nullptr) == SOCKET_ERROR )
    {
        int32 errorCode = ::WSAGetLastError();
        if( errorCode != WSA_IO_PENDING )
        {
            _sendEvent.owner = nullptr;
            _sendEvent.sendBuffers.clear();
            _sendRegistered.store(false);
            Disconnect(L"WSASend Register Failed");
        }
    }
}

//***************************************************************************
// @brief 전송 완료 처리 (WSASend 완료 통지 시 호출)
// @param numOfBytes 전송 완료된 바이트 수
//***************************************************************************
void CIocpSession::ProcessSend(int32 numOfBytes)
{
    _sendEvent.owner = nullptr; // Ref -1
    _sendEvent.sendBuffers.clear(); // 전송 끝난 SendBuffer 수명 해제

    if( numOfBytes == 0 )
    {
        Disconnect(L"Send 0 Bytes");
        return;
    }

    OnSend(numOfBytes);

    // 대기 중인 남은 Send 데이터 확인 후 재등록
    bool hasPendingSend = false;
    {
        std::lock_guard<std::mutex> guard(_lock);
        if( _sendQueue.empty() )
        {
            _sendRegistered.store(false);
        }
        else
        {
            hasPendingSend = true;
        }
    }

    if( hasPendingSend )
    {
        RegisterSend();
    }
}

//***************************************************************************
// @brief 세션 종료 요청
// @param cause 종료 원인 로그 문자열
//***************************************************************************
void CIocpSession::Disconnect(const TCHAR* cause)
{
    if( _connected.exchange(false) == false )
        return;

    // DisconnectEx 호출로 소켓 재사용 상태(TF_REUSE_SOCKET) 유도
    RegisterDisconnect();
}

//***************************************************************************
// @brief 비동기 DisconnectEx 등록
//***************************************************************************
void CIocpSession::RegisterDisconnect()
{
    _disconnectEvent.Init();
    _disconnectEvent.owner = CIocpObject::shared_from_this(); // Ref +1

    if( CSocketUtils::DisconnectEx(_socket, static_cast<LPOVERLAPPED>(&_disconnectEvent), 0, 0) == FALSE )
    {
        int32 errorCode = ::WSAGetLastError();
        if( errorCode != WSA_IO_PENDING )
        {
            _disconnectEvent.owner = nullptr;
            ProcessDisconnect();
        }
    }
}

//***************************************************************************
// @brief DisconnectEx 완료 처리
//***************************************************************************
void CIocpSession::ProcessDisconnect()
{
    _disconnectEvent.owner = nullptr; // Ref -1

    OnDisconnected();
}