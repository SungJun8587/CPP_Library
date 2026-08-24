
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
	_socket = INVALID_SOCKET;
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
		// numOfBytes는 Connect 이벤트에서 성공/실패 구분에 쓸 수 없어(성공/실패
		// 둘 다 0바이트로 통지됨) 인자로 넘기지 않고 ProcessConnectEx() 내부에서
		// getsockopt(SO_ERROR)로 직접 검증합니다.
		ProcessConnectEx();
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
	_closeReason.store(Iocp::CloseReason::None, std::memory_order_release);

	// 세션 재사용(AcceptEx) 시 이전 연결의 잔여 데이터 오염 방지
	_recvBuffer.Clear();

	// 상위 레이어 이벤트 호출
	OnConnected();

	// 첫 비동기 수신(WSARecv) 등록
	RegisterRecv();
}

//***************************************************************************
// @brief 바이트 데이터 전송 요청 (RIO 인터페이스 호환)
// @param data 전송할 데이터 포인터
// @param size 전송할 바이트 크기 (uint16_t)
// @return bool 전송 요청 성공 여부
//***************************************************************************
bool CIocpSession::Send(const void* data, uint16_t size) noexcept
{
	if( IsConnected() == false || data == nullptr || size == 0 )
		return false;

	CSendBufferManager sendBufferManager;
	CSendBufferRef sendBuffer = sendBufferManager.Open(size);
	if( sendBuffer == nullptr || sendBuffer->AllocSize() < size )
		return false;

	// 2. 버퍼에 데이터 복사 및 실제 기록된 크기 마감(Close)
	std::memcpy(sendBuffer->Buffer(), data, size);
	sendBuffer->Close(size);

	// 3. 기존의 내부 private Send(CSendBufferRef) 호출
	Send(sendBuffer);

	return true;
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
// @brief 비동기 데이터 수신(WSARecv) 등록 (CRingBuffer 제로카피)
//***************************************************************************
void CIocpSession::RegisterRecv()
{
	if( IsConnected() == false )
		return;

	_recvEvent.Init();
	_recvEvent.owner = GetIocpObjectPtr(); // I/O 완료 시까지 수명 보장 (Ref +1)

	// CRingBuffer에서 WSARecv에 전달할 쓰기 가능 WSABUF 추출 (최대 2개 청크)
	WSABUF wsaBufs[2];
	int32 bufferCount = _recvBuffer.GetWSARecvBuffers(wsaBufs);

	if( bufferCount == 0 )
	{
		// 링버퍼 공간 부족 (Overflow)
		_recvEvent.owner = nullptr;
		Disconnect(Iocp::CloseReason::RingBufferOverflow);
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
			Disconnect(Iocp::CloseReason::SocketError);
		}
	}
}

//***************************************************************************
// @brief 수신 완료 처리 (WSARecv 완료 통지 시 호출)
// @param numOfBytes 수신된 데이터 바이트 수 (o인 경우 정상 연결 끊김)
// @note 링버퍼 경계 래핑(Wrap-around)으로 인한 메모리 침범 방지를 위해
//       GetSizeDirectDequeueAble() 크기만 OnRecv로 전달하며, 
//       누적된 모든 패킷을 소진할 때까지 while 루프를 순회합니다.
//***************************************************************************
void CIocpSession::ProcessRecv(int32 numOfBytes)
{
	if( numOfBytes == 0 )
	{
		_recvEvent.owner = nullptr;
		Disconnect(Iocp::CloseReason::RemoteClosed);
		return;
	}

	// 1. 링버퍼 쓰기 커서 수신된 바이트 수만큼 이동
	if( _recvBuffer.MoveWriteBuffer(numOfBytes) == false )
	{
		_recvEvent.owner = nullptr;
		Disconnect(Iocp::CloseReason::RingBufferOverflow);
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
			Disconnect(Iocp::CloseReason::InternalError);
			return;
		}

		// 완성된 패킷이 없어서 더 이상 처리를 진행할 수 없는 경우 루프 탈출
		if( processLen == 0 )
			break;

		// 처리한 바이트 수만큼 읽기 커서 이동
		if( _recvBuffer.MoveReadBuffer(processLen) == false )
		{
			_recvEvent.owner = nullptr;
			Disconnect(Iocp::CloseReason::InternalError);
			return;
		}
	}

	_recvEvent.owner = nullptr; // OnRecv 처리 완료 후 수명 해제

	// 다음 데이터 수신 대기
	RegisterRecv();
}

//***************************************************************************
// @brief 비동기 데이터 송신(WSASend) 등록 (Scatter-Gather 패턴)
//***************************************************************************
void CIocpSession::RegisterSend()
{
	if( IsConnected() == false )
		return;

	_sendEvent.Init();
	_sendEvent.owner = GetIocpObjectPtr(); // Ref +1

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
			Disconnect(Iocp::CloseReason::SocketError);
		}
	}
}

//***************************************************************************
// @brief 비동기 DisconnectEx 등록
//***************************************************************************
void CIocpSession::RegisterDisconnect()
{
	_disconnectEvent.Init();
	_disconnectEvent.owner = GetIocpObjectPtr(); // Ref +1

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
// @brief 전송 완료 처리 (WSASend 완료 통지 시 호출)
// @param numOfBytes 전송 완료된 바이트 수
//***************************************************************************
void CIocpSession::ProcessSend(int32 numOfBytes)
{
	_sendEvent.owner = nullptr; // Ref -1
	_sendEvent.sendBuffers.clear(); // 전송 끝난 SendBuffer 수명 해제

	if( numOfBytes == 0 )
	{
		Disconnect(Iocp::CloseReason::SocketError);
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
// @brief DisconnectEx 완료 처리
//***************************************************************************
void CIocpSession::ProcessDisconnect()
{
	_disconnectEvent.owner = nullptr; // Ref -1

	OnDisconnected();
}

//***************************************************************************
// @brief 지정된 사유로 세션 종료 요청
// @param reason 세션 종료 사유
//***************************************************************************
void CIocpSession::Disconnect(Iocp::CloseReason reason)
{
	if( _connected.exchange(false) == false )
		return;

	_closeReason.store(reason, std::memory_order_release);

	// DisconnectEx 호출로 소켓 재사용 상태(TF_REUSE_SOCKET) 유도
	RegisterDisconnect();
}

//***************************************************************************
// @brief 세션 종료 요청 (CSession 인터페이스 구현)
// @param cause 종료 원인 로그 문자열
//***************************************************************************
void CIocpSession::Disconnect(const TCHAR* cause)
{
	Disconnect(Iocp::CloseReason::ForcedClose);
}

//***************************************************************************
// @brief ConnectEx로 비동기 연결을 게시합니다 (클라이언트 측 전용).
// @param remoteAddr 접속할 원격 주소
// @return bool 게시 시도 자체의 성공 여부. 상세 계약은 헤더의 ConnectAsync()
//         주석 참고 — 이 값이 true라고 해서 연결이 성공했다는 뜻이 아닙니다.
// @details ConnectEx는 사전에 bind()된 소켓에서만 호출 가능합니다 — 클라이언트가
//          로컬 포트를 지정할 이유가 없으므로 와일드카드(0.0.0.0:0)로 바인딩합니다.
//          CNetAddress()의 기본 생성자는 SOCKADDR_IN을 전부 0으로 두는데,
//          이러면 sin_family도 0이 되어 AF_INET 소켓에 bind()가 실패합니다
//          (CNetAddress(ip, port) 생성자만 sin_family=AF_INET을 명시적으로
//          세팅함 — NetAddress.h/.cpp 확인 후 발견/수정). 그래서 명시적으로
//          CNetAddress(_T("0.0.0.0"), 0)을 사용합니다.
//          이 함수의 모든 실패 경로는 FailConnect()를 호출해 OnDisconnected()까지
//          통지를 완료하므로, 호출부는 반환값이 false여도 별도로 정리할 것이
//          없습니다(FailConnect() 계약 — 헤더 주석 참고).
//***************************************************************************
bool CIocpSession::ConnectAsync(const CNetAddress& remoteAddr)
{
	if( _socket == INVALID_SOCKET )
	{
		FailConnect(Iocp::CloseReason::SocketError);
		return false;
	}

	if( !CSocketUtils::Bind(_socket, CNetAddress(_T("0.0.0.0"), 0)) )
	{
		FailConnect(Iocp::CloseReason::SocketError);
		return false;
	}

	// RegisterConnect() 내부에서 ConnectEx 게시가 즉시 실패하면 자체적으로
	// FailConnect()를 호출합니다 — 그 경우도 이 함수는 true를 반환합니다(게시
	// "시도" 자체는 정상적으로 이뤄졌고, 실패 통지는 OnDisconnected()로 이미
	// 처리됐기 때문). 헤더의 ConnectAsync() 계약 설명 참고.
	RegisterConnect(remoteAddr);
	return true;
}

//***************************************************************************
// @brief ConnectEx 비동기 연결을 실제로 게시합니다.
//***************************************************************************
void CIocpSession::RegisterConnect(const CNetAddress& remoteAddr)
{
	_connectEvent.Init();
	_connectEvent.owner = GetIocpObjectPtr(); // 완료 통지까지 수명 보장 (Ref +1)

	SOCKADDR_IN sockAddr = remoteAddr.GetSockAddr();
	DWORD bytesSent = 0;

	if( CSocketUtils::ConnectEx(_socket, reinterpret_cast<SOCKADDR*>(&sockAddr), sizeof(sockAddr),
		nullptr, 0, &bytesSent, static_cast<LPOVERLAPPED>(&_connectEvent)) == FALSE )
	{
		int32 errorCode = ::WSAGetLastError();
		if( errorCode != WSA_IO_PENDING )
		{
			// 게시 자체가 즉시 실패 — IOCP 완료 통지가 오지 않으므로 여기서 직접 정리.
			_connectEvent.owner = nullptr;
			FailConnect(Iocp::CloseReason::SocketError);
		}
	}
}

//***************************************************************************
// @brief ConnectEx 완료 통지 처리 (Dispatch가 호출).
//***************************************************************************
void CIocpSession::ProcessConnectEx()
{
	_connectEvent.owner = nullptr; // Ref -1

	int32 sockError = 0;
	bool getOptOk = CSocketUtils::GetSocketError(_socket, sockError);

	if( !getOptOk || sockError != 0 )
	{
		FailConnect(Iocp::CloseReason::SocketError);
		return;
	}

	// ConnectEx로 연결된 소켓은 SO_UPDATE_CONNECT_CONTEXT를 걸어야
	// getpeername/setsockopt(TCP_NODELAY 등)/getsockname이 정상 동작합니다.
	if( !CSocketUtils::SetUpdateConnectContext(_socket) )
	{
		FailConnect(Iocp::CloseReason::SocketError);
		return;
	}

	// 이후는 accept 경로와 완전히 동일한 공통 처리
	// (_connected 플래그 세팅, OnConnected(), 첫 RegisterRecv())
	ProcessConnect();
}

//***************************************************************************
// @brief connect 실패 시 정리 전용 경로.
// @param reason 실패 사유
//***************************************************************************
void CIocpSession::FailConnect(Iocp::CloseReason reason)
{
	_closeReason.store(reason, std::memory_order_release);

	CSocketUtils::Close(_socket);
	_socket = INVALID_SOCKET;

	OnDisconnected();            // 상위 콘텐츠 레이어 훅 (protected virtual)
	CSession::OnDisconnected();  // 서비스의 ReleaseSession 콜백 연동
}