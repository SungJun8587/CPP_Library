
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

	// [수정] 세션 객체가 재사용되는 경로(위 주석 참고)에서, 이전 연결 사이클의
	// TryFinalizeDisconnect() 관련 상태가 남아있으면 안 된다 — 특히
	// _disconnectNotified가 true로 남아있으면 이번 연결이 끊길 때
	// OnDisconnected()가 "이미 통지함" 가드에 막혀 영원히 호출되지 않는다.
	// 새 연결 사이클은 항상 이 셋이 초기 상태(0/false)여야 한다.
	_pendingIoCount.store(0, std::memory_order_seq_cst);
	_disconnectCompleted.store(false, std::memory_order_seq_cst);
	_disconnectNotified.store(false, std::memory_order_seq_cst);

	// [수정] 송신 상태도 동일한 이유로 리셋 필요. ProcessSend()의
	// numOfBytes==0(WSASend 취소/실패 완료) 분기는 Disconnect()만 호출하고
	// _sendRegistered/_sendQueue를 정리하지 않은 채 반환하므로, 이전 연결이
	// Send 대기 중(또는 in-flight) 상태로 끊긴 뒤 이 세션 객체가 재사용되면
	// (1) _sendRegistered가 true로 남아 Send()가 영원히 RegisterSend()를
	//     트리거하지 못해(exchange(true)==false를 통과 못함) 송신이 마비되고,
	// (2) _sendQueue에 남아있던 이전 연결의 미전송 버퍼가 이후 어떤 경로로든
	//     RegisterSend()가 걸릴 때 새 클라이언트에게 그대로 전송되는
	//     세션 간 데이터 혼선이 발생한다. RegisterSend()의 즉시 실패 분기가
	//     동일하게 정리하는 것과 대칭되도록 여기서도 락 하에 정리한다.
	{
		std::lock_guard<std::mutex> guard(_lock);
		_sendQueue.clear();
		_sendRegistered.store(false);
	}

	// 상위 레이어 이벤트 호출
	OnConnected();

	// 첫 비동기 수신(WSARecv) 등록
	RegisterRecv();
}

//***************************************************************************
// @brief 바이트 데이터 전송 요청 (RIO 인터페이스 호환)
// @param data 전송할 데이터 포인터
// @param size 전송할 바이트 크기 (uint16)
// @return bool 전송 요청 성공 여부
//***************************************************************************
bool CIocpSession::Send(const void* data, uint16 size) noexcept
{
	if( IsConnected() == false || data == nullptr || size == 0 )
		return false;

	CSendBufferRef sendBuffer = CSendBufferManager::Open(size);
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

	// [수정] 실제 게시 직전에 증가시키고, 게시 자체가 즉시 실패하면(completion이
	// 절대 안 옴) 바로 롤백한다 — TryFinalizeDisconnect()의 설명 참고.
	_pendingIoCount.fetch_add(1, std::memory_order_seq_cst);

	if( ::WSARecv(_socket, wsaBufs, static_cast<DWORD>(bufferCount), OUT & numOfBytes, &flags, static_cast<LPOVERLAPPED>(&_recvEvent), nullptr) == SOCKET_ERROR )
	{
		int32 errorCode = ::WSAGetLastError();
		if( errorCode != WSA_IO_PENDING )
		{
			_pendingIoCount.fetch_sub(1, std::memory_order_seq_cst); // 게시 실패 롤백 — completion이 안 옴
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
	// [수정] 이 completion 하나에 대응하는 outstanding 카운트를 함수 최상단에서
	// 즉시 감소시킨다 — 아래 여러 갈래의 early-return 경로 전부에서 정확히
	// 1회씩만 실행되도록 보장하는 가장 단순한 위치. TryFinalizeDisconnect()는
	// _disconnectCompleted/카운트 조건을 스스로 재확인하므로, 매 completion마다
	// 무조건 호출해도 안전하다(조건 미충족이면 즉시 반환).
	_pendingIoCount.fetch_sub(1, std::memory_order_seq_cst);
	TryFinalizeDisconnect();

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

		// 콘텐츠 레이어로 데이터 전달 (이 호출 중 상위 레이어가 Disconnect()를
		// 호출할 수 있음 — 그 경우 RegisterRecv()는 자체적으로 IsConnected()를
		// 체크하므로 아래 루프 종료 후 안전하게 처리된다)
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

	// 다음 데이터 수신 대기. RegisterRecv() 진입 시 자체적으로 IsConnected()를
	// 검사하므로, OnRecv() 도중 상위 레이어가 Disconnect()를 호출한 경우에도
	// 여기서 새로운 WSARecv가 게시되지 않는다.
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

	// [수정] 실제 게시 직전에 증가시키고, 게시 자체가 즉시 실패하면(completion이
	// 절대 안 옴) 바로 롤백한다 — TryFinalizeDisconnect()의 설명 참고.
	_pendingIoCount.fetch_add(1, std::memory_order_seq_cst);

	if( ::WSASend(_socket, wsaBufs.data(), static_cast<DWORD>(wsaBufs.size()), OUT & numOfBytes, 0, static_cast<LPOVERLAPPED>(&_sendEvent), nullptr) == SOCKET_ERROR )
	{
		int32 errorCode = ::WSAGetLastError();
		if( errorCode != WSA_IO_PENDING )
		{
			_pendingIoCount.fetch_sub(1, std::memory_order_seq_cst); // 게시 실패 롤백 — completion이 안 옴

			// [수정] Disconnect()를 가장 먼저 호출해 _connected를 즉시 false로
			// 전환한다. 이렇게 해야 이 지점과 아래 정리 코드 사이의 시간 창에서
			// 다른 스레드가 Send()를 호출해 IsConnected()==true를 관측하고
			// _sendRegistered.exchange(true)==false를 통과해 같은 _sendEvent에
			// 대해 RegisterSend()를 동시에 재진입하는 Race Condition을 막을 수
			// 있다(기존에는 _sendRegistered를 Disconnect()보다 먼저 false로
			// 되돌려, 그 창에서 다른 스레드가 새 WSASend를 거는 동시에 이
			// 스레드가 Disconnect()로 DisconnectEx를 거는 문제가 있었다).
			Disconnect(Iocp::CloseReason::SocketError);

			std::lock_guard<std::mutex> guard(_lock);
			_sendEvent.owner = nullptr;
			_sendEvent.sendBuffers.clear();
			_sendQueue.clear();
			_sendRegistered.store(false);
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
	// [수정] ProcessRecv()와 동일한 이유로 함수 최상단에서 즉시 감소시킨다.
	_pendingIoCount.fetch_sub(1, std::memory_order_seq_cst);
	TryFinalizeDisconnect();

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
// @details [수정] 예전에는 여기서 곧바로 OnDisconnected()를 통지했다. 이제는
//          _disconnectCompleted만 세팅해두고 TryFinalizeDisconnect()에
//          위임한다 — 그 시점에 이미 게시돼 있던 WSARecv/WSASend가 아직
//          outstanding이면(다른 워커 스레드가 처리 중) 그 마지막 완료가
//          알아서 마저 통지해준다. 자세한 배경은 헤더의 TryFinalizeDisconnect()
//          선언부 주석 참고.
//***************************************************************************
void CIocpSession::ProcessDisconnect()
{
	_disconnectEvent.owner = nullptr; // Ref -1

	_disconnectCompleted.store(true, std::memory_order_seq_cst);
	TryFinalizeDisconnect();
}

//***************************************************************************
// @brief outstanding recv/send가 전부 끝났고 DisconnectEx 자신의 completion도
//        도착했을 때만 OnDisconnected()를 1회 통지합니다.
// @details 어느 쪽 조건이 나중에 만족되든(마지막 recv/send completion, 또는
//          DisconnectEx completion 그 자체) 그쪽 호출부가 이 함수를 통해
//          통지를 트리거합니다 — 자세한 설계 배경/알려진 잔여 레이스는
//          헤더의 이 함수 선언부 주석 참고.
//***************************************************************************
void CIocpSession::TryFinalizeDisconnect() noexcept
{
	if( !_disconnectCompleted.load(std::memory_order_seq_cst) )
		return;

	if( _pendingIoCount.load(std::memory_order_seq_cst) != 0 )
		return;

	if( _disconnectNotified.exchange(true, std::memory_order_seq_cst) )
		return; // 이미 다른 스레드가 통지 완료

	OnDisconnected();
	CSession::OnDisconnected();
}

//***************************************************************************
// @brief 지정된 사유로 세션 종료 요청
// @param reason 세션 종료 사유
// @details
// [수정] 기존에는 `_connected.exchange(false) == false`면(즉 한 번도 연결
// 완료 전이던 상태 — Accept/ConnectEx가 아직 진행 중인 세션) 완전히 no-op으로
// 반환했다. 그런데 CNetService::Close()는 세션 생성 직후(연결 완료 전)부터
// AddSession()으로 _sessions에 등록된 세션에 대해서도 이 Disconnect()를
// 호출하므로, 그 세션이 실제로 연결 완료/실패해 스스로 OnDisconnected()를
// 통지하기 전까지 _sessionsEmptyCv가 영원히 깨어나지 않아 Close() 호출
// 스레드가 무한 대기(hang)하는 문제가 있었다.
//
// 이제는 미연결 상태에서도 FailConnect()와 동일하게 소켓을 직접 닫아
// pending AcceptEx/ConnectEx를 취소시키고, 즉시 OnDisconnected() 통지까지
// 완료한다. _disconnectNotified CAS로 최초 1회만 통지되도록 가드하며,
// FailConnect()도 동일한 가드를 거치도록 통일해(아래 참고) — 취소된 I/O의
// 완료 통지가 나중에 도착해 FailConnect()를 다시 태워도 중복 통지되지 않는다.
//***************************************************************************
void CIocpSession::Disconnect(Iocp::CloseReason reason)
{
	if( _connected.exchange(false) == false )
	{
		// 아직 연결 완료 전(Accept/ConnectEx 진행 중) — FailConnect()와 동일한
		// 강제 정리 경로. _disconnectNotified가 이미 true면(FailConnect()가
		// 먼저 통지를 마쳤거나 이 경로가 이미 실행됨) 아무 것도 하지 않는다.
		if( _disconnectNotified.exchange(true, std::memory_order_seq_cst) )
			return;

		_closeReason.store(reason, std::memory_order_release);

		CSocketUtils::Close(_socket); // pending AcceptEx/ConnectEx 취소 유도
		_socket = INVALID_SOCKET;

		OnDisconnected();
		CSession::OnDisconnected();
		return;
	}

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
	// [수정] 새 연결 시도 사이클 시작 — 이전 시도(재사용된 세션 객체의 과거
	// 실패한 connect 등)에서 _disconnectNotified가 true로 남아있으면 이번
	// 시도의 FailConnect()/Disconnect() 강제종료 경로가 가드에 막혀 아예
	// 통지되지 않는다(ProcessConnect()는 "성공"한 연결에서만 리셋하므로 실패로
	// 끝난 이전 시도 뒤에는 이 리셋을 거치지 못함). ConnectOneMoreSession()이
	// AddSession()을 이 함수 호출보다 먼저 수행하므로, 그 좁은 창에서 Close()가
	// 끼어들면 여전히 스테일 가드를 볼 수 있는 잔여 레이스가 있으나(클라이언트
	// 세션 객체가 실패 직후 재사용되는 경우에 한정), ProcessConnect() 리셋과
	// 대칭을 맞추는 것으로 실질적인 케이스는 대부분 닫힌다.
	_disconnectNotified.store(false, std::memory_order_seq_cst);

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
// @details [수정] Disconnect(Iocp::CloseReason)의 "미연결 상태 강제 종료" 경로와
//          동일한 _disconnectNotified CAS 가드를 공유한다. CNetService::Close()가
//          연결 완료 전인 이 세션에 대해 먼저 Disconnect()를 호출해 소켓을 이미
//          닫고 통지까지 마친 뒤, 취소된 ConnectEx의 완료가 뒤늦게 도착해
//          ProcessConnectEx()가 이 함수를 호출하는 경우 — 가드가 없으면
//          OnDisconnected()가 두 번 호출된다.
//***************************************************************************
void CIocpSession::FailConnect(Iocp::CloseReason reason)
{
	if( _disconnectNotified.exchange(true, std::memory_order_seq_cst) )
		return; // Disconnect()의 강제 종료 경로가 이미 통지를 마침

	_closeReason.store(reason, std::memory_order_release);

	CSocketUtils::Close(_socket);
	_socket = INVALID_SOCKET;

	OnDisconnected();            // 상위 콘텐츠 레이어 훅 (protected virtual)
	CSession::OnDisconnected();  // 서비스의 ReleaseSession 콜백 연동
}