
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
	// 연결이 완료되기 전에 Disconnect()/FailConnect()가 이미 종료를 통지한 세션(서비스 종료와 겹친 경우)은
	// 소켓이 닫혀 있으므로 연결 처리를 하지 않는다. 세션은 연결 사이클을 한 번만 겪는다 — Accept/Connect마다
	// 팩토리가 새 세션을 만들고 DisconnectEx는 소켓을 재사용하지 않으므로(dwFlags = 0), 통지 가드를
	// 되돌려 세션을 다시 살릴 필요가 없다.
	if( _disconnectNotified.load(std::memory_order_seq_cst) )
		return;

	_closeReason.store(Iocp::CloseReason::None, std::memory_order_release);
	_gracefulClose.store(false, std::memory_order_release);

	// 수신 버퍼와 종료 추적/송신 상태를 연결 시작 상태로 맞춘다.
	_recvBuffer.Clear();
	_pendingIoCount.store(0, std::memory_order_seq_cst);
	_disconnectCompleted.store(false, std::memory_order_seq_cst);

	{
		std::lock_guard<std::mutex> guard(_lock);
		_sendQueue.clear();
		_sendQueueBytes = 0;
		_sendRegistered.store(false);
	}

	// 소켓 옵션(TCP_NODELAY, keep-alive)은 상위 레이어가 연결 완료를 통지받기 전에 적용한다.
	ApplySocketOptions();

	// 연결 상태 전환은 위의 상태 초기화와 소켓 옵션 적용이 모두 끝난 뒤에 한다. 이 세션은 서비스/세션
	// 매니저에 이미 등록돼 있어, 먼저 true로 바꾸면 Broadcast() 등 다른 스레드의 Send()가 초기화
	// 구간에 끼어들어 _sendRegistered/_pendingIoCount를 되돌려 버리거나(WSASend 중복 게시, 카운트 불일치),
	// 큐에 넣은 데이터가 지워질 수 있다.
	_connected.store(true);

	// 상위 레이어 이벤트 호출
	OnConnected();

	// 첫 비동기 수신(WSARecv) 등록
	RegisterRecv();
}

//***************************************************************************
// @brief 지정된 TCP 소켓 옵션(TCP_NODELAY, keep-alive)을 적용합니다. 실패해도 연결은 계속한다(경고만 기록).
//***************************************************************************
void CIocpSession::ApplySocketOptions() noexcept
{
	if( _socket == INVALID_SOCKET )
		return;

	if( _socketOptions.noDelay && CSocketUtils::SetNoDelay(_socket, true) == false )
		LOG_WARNING(_T("[CIocpSession] TCP_NODELAY failed: sessionId=%llu, error=%d"), _sessionId, ::WSAGetLastError());

	if( _socketOptions.keepAlive &&
		CSocketUtils::SetKeepAlive(_socket, true, _socketOptions.keepAliveIdleMs, _socketOptions.keepAliveIntervalMs) == false )
		LOG_WARNING(_T("[CIocpSession] SO_KEEPALIVE failed: sessionId=%llu, error=%d"), _sessionId, ::WSAGetLastError());
}

//***************************************************************************
// @brief 바이트 데이터를 송신 큐에 넣고 전송을 요청합니다 (RIO 인터페이스 호환, Thread-safe).
// @param data 전송할 데이터 포인터
// @param size 전송할 바이트 크기 (uint16)
// @return bool 큐잉 성공 여부
// @details 데이터를 PrepareSend()로 SendBuffer들에 복사한 뒤 EnqueueSend()에서 한 번에 큐에 넣는다.
//          HTTP/TLS 계층은 이 함수를 최대 65535바이트 단위로 호출한다.
//          큐/WSABUF 할당 중 예외(메모리 부족 등)가 나면 송신 상태(_sendRegistered 등)가 어긋나
//          이후 송신이 멈출 수 있으므로, noexcept 함수 밖으로 예외가 새어 프로세스가 종료되지
//          않도록 여기서 받아 연결을 종료한다.
//***************************************************************************
bool CIocpSession::Send(const void* data, uint16 size) noexcept
{
	if( IsConnected() == false )
		return false;

	try
	{
		SendPieces pieces;
		if( PrepareSend(data, size, pieces) == false )
			return false;

		return EnqueueSend(pieces.buffers, pieces.count, pieces.totalBytes);
	}
	catch( ... )
	{
		LOG_ERROR(_T("[CIocpSession] Send failed with exception: sessionId=%llu"), _sessionId);
		Disconnect(Iocp::CloseReason::InternalError);
		return false;
	}
}

//***************************************************************************
// @brief 데이터를 SendBuffer들로 복사해 SendPieces를 만듭니다 (호출 스레드의 thread_local 청크 사용).
// @param data 전송할 데이터 포인터
// @param size 전송할 바이트 수 (1 ~ 65535)
// @param out 결과를 받을 SendPieces
// @return bool 성공 여부
// @details 한 SendBuffer는 청크(Iocp::SEND_BUFFER_CHUNK_SIZE)를 넘을 수 없으므로, 그보다 큰 데이터는
//          여러 SendBuffer로 나눈다. 실패하면 이미 만든 조각은 out이 해제될 때 함께 풀로 반환된다.
//***************************************************************************
bool CIocpSession::PrepareSend(const void* data, uint16 size, SendPieces& out) noexcept
{
	out.count = 0;
	out.totalBytes = 0;

	if( data == nullptr || size == 0 )
		return false;

	const BYTE* src = static_cast<const BYTE*>(data);
	uint32 remaining = size;

	try
	{
		while( remaining > 0 )
		{
			const uint32 pieceSize = (std::min)(remaining, Iocp::SEND_BUFFER_CHUNK_SIZE);

			CSendBufferRef sendBuffer = CSendBufferManager::Open(pieceSize);
			if( sendBuffer == nullptr || sendBuffer->AllocSize() < pieceSize )
				return false;

			// 버퍼에 데이터 복사 및 실제 기록된 크기 마감(Close)
			std::memcpy(sendBuffer->Buffer(), src, pieceSize);
			sendBuffer->Close(pieceSize);

			out.buffers[out.count++] = std::move(sendBuffer);
			src += pieceSize;
			remaining -= pieceSize;
		}
	}
	catch( ... )
	{
		// 메모리 부족 등 — noexcept 함수 밖으로 예외가 새지 않게 실패로 돌려준다.
		return false;
	}

	out.totalBytes = size;
	return true;
}

//***************************************************************************
// @brief PrepareSend()로 만든 SendBuffer 묶음을 이 세션의 송신 큐에 넣습니다 (Thread-safe).
// @param pieces 전송할 묶음 (참조만 복사하므로 다른 세션에 다시 쓸 수 있다)
// @return bool 큐잉 성공 여부
//***************************************************************************
bool CIocpSession::SendPrepared(const SendPieces& pieces) noexcept
{
	if( IsConnected() == false || pieces.count == 0 )
		return false;

	try
	{
		CSendBufferRef queued[Iocp::kMaxSendPieces];
		for( size_t i = 0; i < pieces.count; ++i )
			queued[i] = pieces.buffers[i];

		return EnqueueSend(queued, pieces.count, pieces.totalBytes);
	}
	catch( ... )
	{
		// Send()와 같은 이유로, 큐 할당 중 예외가 나면 송신 상태가 어긋나므로 연결을 종료한다.
		LOG_ERROR(_T("[CIocpSession] SendPrepared failed with exception: sessionId=%llu"), _sessionId);
		Disconnect(Iocp::CloseReason::InternalError);
		return false;
	}
}

//***************************************************************************
// @brief Close()된 SendBuffer들을 하나의 단위로 송신 큐에 넣습니다 (Thread-safe).
// @param buffers SendBuffer 배열 (큐로 이동됨)
// @param count 배열 원소 수
// @param totalBytes 배열이 담고 있는 데이터의 총 바이트 수
// @return bool 큐잉 성공 여부 (미연결 또는 송신 큐 상한 초과 시 false)
// @details 한 번의 락 구간에서 모든 조각을 넣어 다른 스레드의 Send()와 섞이지 않게 한다.
//          상한(_maxSendQueueBytes) 초과 시 데이터는 큐에 넣지 않고 연결을 종료한다.
//***************************************************************************
bool CIocpSession::EnqueueSend(CSendBufferRef* buffers, size_t count, uint64 totalBytes)
{
	if( IsConnected() == false || buffers == nullptr || count == 0 )
		return false;

	bool registerSend = false;
	bool overflow = false;
	uint64 queuedBytes = 0;
	const uint64 limit = _maxSendQueueBytes.load(std::memory_order_relaxed);

	{
		std::lock_guard<std::mutex> guard(_lock);

		queuedBytes = _sendQueueBytes;
		if( limit != 0 && queuedBytes + totalBytes > limit )
		{
			overflow = true;
		}
		else
		{
			for( size_t i = 0; i < count; ++i )
				_sendQueue.push_back(std::move(buffers[i]));

			_sendQueueBytes += totalBytes;

			// 현재 진행 중인 WSASend가 없다면 등록 수행
			if( _sendRegistered.exchange(true) == false )
			{
				registerSend = true;
			}
		}
	}

	if( overflow )
	{
		LOG_WARNING(_T("[CIocpSession] send queue overflow: sessionId=%llu, queued=%llu, request=%llu, limit=%llu"),
			_sessionId, queuedBytes, totalBytes, limit);
		Disconnect(Iocp::CloseReason::SendBufferOverflow);
		return false;
	}

	if( registerSend )
	{
		RegisterSend();
	}

	return true;
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

	// 실제 게시 직전에 증가시키고, 게시 자체가 즉시 실패하면(completion이 절대 안 옴)
	// 바로 롤백한다 — TryFinalizeDisconnect()의 설명 참고.
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
// @brief completion 하나분의 outstanding I/O 카운트를 내리고 종료 통지 조건을 재확인합니다.
//***************************************************************************
void CIocpSession::ReleaseIo() noexcept
{
	_pendingIoCount.fetch_sub(1, std::memory_order_seq_cst);
	TryFinalizeDisconnect();
}

//***************************************************************************
// @brief 수신 완료 처리 (WSARecv 완료 통지 시 호출)
// @param numOfBytes 수신된 데이터 바이트 수 (0인 경우 정상 연결 끊김)
// @note 링버퍼 경계 래핑(Wrap-around) 처리: 사용 중인 데이터가 버퍼 끝을
//       넘어 두 조각으로 나뉘어 있으면(write < read) OnRecv()가 조각 하나만
//       보고는 경계에 걸친 패킷을 영원히 완성할 수 없다(첫 조각은 항상
//       불완전하고, 뒤 조각은 OnRecv에 전달되지 않아 읽기 커서가 그 자리에서
//       움직이지 못한다 — 이후 수신이 쌓이기만 하다 링버퍼가 가득 차 연결이
//       끊긴다). 그래서 데이터가 연속이면 링버퍼를 그대로 넘기고, 두 조각으로
//       나뉘어 있으면 사용 중인 전체를 연속 버퍼로 복사(Peek)해서 넘긴다.
//       누적된 모든 패킷을 소진할 때까지 while 루프를 순회합니다.
//***************************************************************************
void CIocpSession::ProcessRecv(int32 numOfBytes)
{
	// 이 completion 하나에 대응하는 outstanding 카운트는 함수가 끝날 때(OnRecv()와 다음 WSARecv
	// 게시까지 마친 뒤) 감소시키고 TryFinalizeDisconnect()를 호출한다. 스코프 가드라서 아래 여러
	// 갈래의 early-return 경로 전부에서 정확히 1회씩 실행된다. 이 completion을 처리하는 동안은
	// outstanding으로 남아 있으므로 다른 스레드가 OnDisconnected()를 통지하지 못하고, 다음
	// WSARecv는 RegisterRecv()가 이 카운트를 내리기 전에 증가시키므로 카운트가 순간적으로 0이
	// 되지 않는다. TryFinalizeDisconnect()는 조건을 스스로 재확인하므로 매번 호출해도 안전하다.
	// 내부 오류 경로의 Disconnect()는 IsConnected()일 때만 호출한다 — 이미 종료가 시작된 세션에서
	// 다시 호출하면 강제 정리 경로로 들어가 종료 사유를 덮어쓰고 조기 통지된다.
	auto releaseIo = CScopeExit([this]() { ReleaseIo(); });

	if( numOfBytes == 0 )
	{
		_recvEvent.owner = nullptr;

		// 이미 종료가 시작된 세션(Disconnect() 이후)에서 취소된 recv가 0바이트로 완료된 경우에는
		// Disconnect()를 다시 호출하지 않는다. 종료는 진행 중이고 OnDisconnected() 통지는 위의
		// TryFinalizeDisconnect()가 담당한다. 다시 호출하면 _connected가 이미 false라 강제 정리
		// 경로로 들어가 종료 사유를 덮어쓰고, DisconnectEx 완료 전에 소켓을 닫으며, outstanding
		// I/O가 남은 채 OnDisconnected()를 통지하게 된다.
		if( IsConnected() )
		{
			// 오류 없이 0바이트로 완료됐다면 상대가 연결을 정상 종료(FIN)한 것이다 (리셋 등은 errorCode가 채워진다).
			if( _recvEvent.errorCode == 0 )
				_gracefulClose.store(true, std::memory_order_release);

			Disconnect(Iocp::CloseReason::RemoteClosed);
		}
		return;
	}

	// Disconnect()가 이미 호출된 종료 진행 중 세션 — 수신 데이터를 상위 레이어로 올리지 않고
	// 새 WSARecv도 게시하지 않는다.
	if( IsConnected() == false )
	{
		_recvEvent.owner = nullptr;
		return;
	}

	// 1. 링버퍼 쓰기 커서 수신된 바이트 수만큼 이동
	if( _recvBuffer.MoveWriteBuffer(numOfBytes) == false )
	{
		_recvEvent.owner = nullptr;
		if( IsConnected() )
			Disconnect(Iocp::CloseReason::RingBufferOverflow);
		return;
	}

	// 2. 수신 버퍼 처리 루프 (누적된 패킷 소진)
	while( true )
	{
		// 이전 OnRecv()(또는 다른 스레드)가 Disconnect()를 호출했다면 버퍼에 남은 데이터를
		// 상위 레이어로 더 올리지 않는다. 이 시점 이후 새 WSARecv도 게시되지 않는다.
		if( IsConnected() == false )
			break;

		int64 dataSize = _recvBuffer.GetSizeUsed();
		if( dataSize <= 0 )
			break;

		// 콘텐츠 레이어에 넘길 연속 메모리 구간을 정한다. 데이터가 버퍼 안에서
		// 연속이면 링버퍼를 그대로 쓰고(복사 없음), 버퍼 끝을 넘어 두 조각으로
		// 나뉘어 있으면 전체를 연속 버퍼로 복사해서 쓴다 — 위 함수 설명 참고.
		// 스레드 로컬 버퍼는 OnRecv()가 동기적으로 끝날 때까지만 쓰이고, 같은
		// 스레드에서 ProcessRecv()가 중첩 호출되는 일은 없다.
		const int64 directSize = _recvBuffer.GetSizeDirectDequeueAble();
		BYTE* readPos = reinterpret_cast<BYTE*>(_recvBuffer.GetReadBuffer());
		int64 availableSize = directSize;

		thread_local std::vector<BYTE> linearBuffer;
		if( directSize < dataSize )
		{
			linearBuffer.resize(static_cast<size_t>(dataSize));

			int64 peekedSize = 0;
			if( !_recvBuffer.Peek(reinterpret_cast<char*>(linearBuffer.data()), dataSize, &peekedSize) || peekedSize != dataSize )
			{
				_recvEvent.owner = nullptr;
				if( IsConnected() )
					Disconnect(Iocp::CloseReason::InternalError);
				return;
			}

			readPos = linearBuffer.data();
			availableSize = dataSize;
		}

		// 콘텐츠 레이어로 데이터 전달 (이 호출 중 상위 레이어가 Disconnect()를
		// 호출할 수 있음 — 그 경우 RegisterRecv()는 자체적으로 IsConnected()를
		// 체크하므로 아래 루프 종료 후 안전하게 처리된다)
		int32 processLen = OnRecv(readPos, static_cast<int32>(availableSize));

		if( processLen < 0 || availableSize < processLen )
		{
			_recvEvent.owner = nullptr;
			if( IsConnected() )
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
			if( IsConnected() )
				Disconnect(Iocp::CloseReason::InternalError);
			return;
		}
	}

	_recvEvent.owner = nullptr; // OnRecv 처리 완료 후 수명 해제

	// 버퍼를 전부 소비했다면 읽기/쓰기 커서를 시작점으로 되돌린다. 이 세션은 WSARecv를 하나만 게시하고
	// 그 completion이 지금 처리되고 있으므로 진행 중인 수신이 없어 안전하다. 커서가 계속 앞으로만 가면
	// 버퍼 끝에서 데이터가 두 조각으로 갈라져 WSARecv에 WSABUF 2개를 넘기고, OnRecv()에는 연속 버퍼로
	// 복사해서 넘겨야 한다. 비우면서 되돌리면 소형 패킷 위주 트래픽은 항상 연속 구간 하나만 쓴다.
	if( _recvBuffer.GetSizeUsed() == 0 )
		_recvBuffer.Clear();

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
		_sendQueueBytes = 0;

		if( _sendEvent.sendBuffers.empty() )
		{
			// 전송할 데이터가 없다(종료/재초기화 경로와 겹친 경우) — 게시하지 않고 등록 상태를 해제한다.
			_sendEvent.owner = nullptr;
			_sendRegistered.store(false);
			return;
		}
	}

	// WSABUF 배열은 SendEvent가 보유한다. 다음 RegisterSend()는 이 WSASend의 완료 통지
	// (ProcessSend) 이후에만 실행되므로 완료 전에는 변경되지 않으며, 매 전송마다 지역
	// 배열을 할당하지 않는다.
	CVector<WSABUF>& wsaBufs = _sendEvent.wsaBufs;
	wsaBufs.clear();
	wsaBufs.reserve(_sendEvent.sendBuffers.size());

	for( CSendBufferRef& sendBuffer : _sendEvent.sendBuffers )
	{
		char* const buf = reinterpret_cast<char*>(sendBuffer->Buffer());
		const ULONG len = static_cast<ULONG>(sendBuffer->WriteSize());

		// 같은 스레드가 연달아 Send()한 조각은 thread_local 청크에서 이어 붙여 할당되므로 메모리가
		// 연속이다. 앞 WSABUF의 끝이 이번 버퍼의 시작과 맞닿으면 하나로 합쳐 WSASend의 버퍼 개수를
		// 줄인다. 두 버퍼 모두 sendBuffers가 완료까지 붙들고 있어 합쳐진 구간이 해제될 일은 없다.
		if( !wsaBufs.empty() && wsaBufs.back().buf + wsaBufs.back().len == buf && wsaBufs.back().len <= (std::numeric_limits<ULONG>::max)() - len )
		{
			wsaBufs.back().len += len;
			continue;
		}

		WSABUF wsaBuf;
		wsaBuf.buf = buf;
		wsaBuf.len = len;
		wsaBufs.push_back(wsaBuf);
	}

	DWORD numOfBytes = 0;

	// 실제 게시 직전에 증가시키고, 게시 자체가 즉시 실패하면(completion이 절대 안 옴)
	// 바로 롤백한다 — TryFinalizeDisconnect()의 설명 참고.
	_pendingIoCount.fetch_add(1, std::memory_order_seq_cst);

	if( ::WSASend(_socket, wsaBufs.data(), static_cast<DWORD>(wsaBufs.size()), OUT & numOfBytes, 0, static_cast<LPOVERLAPPED>(&_sendEvent), nullptr) == SOCKET_ERROR )
	{
		int32 errorCode = ::WSAGetLastError();
		if( errorCode != WSA_IO_PENDING )
		{
			_pendingIoCount.fetch_sub(1, std::memory_order_seq_cst); // 게시 실패 롤백 — completion이 안 옴

			// Disconnect()를 먼저 호출해 _connected를 즉시 false로 전환한다. 그래야 아래 정리
			// 코드와의 사이에 다른 스레드가 Send()에서 IsConnected()==true를 보고
			// _sendRegistered.exchange(true)를 통과해 같은 _sendEvent로 RegisterSend()를
			// 재진입하는 것을 막을 수 있다.
			Disconnect(Iocp::CloseReason::SocketError);

			std::lock_guard<std::mutex> guard(_lock);
			_sendEvent.owner = nullptr;
			_sendEvent.Reset();
			_sendQueue.clear();
			_sendQueueBytes = 0;
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
	// ProcessRecv()와 동일한 이유로, 이 completion의 outstanding 카운트는 함수가 끝날 때(OnSend()와
	// 다음 WSASend 게시까지 마친 뒤) 감소시키고 TryFinalizeDisconnect()를 호출한다.
	auto releaseIo = CScopeExit([this]() { ReleaseIo(); });

	_sendEvent.owner = nullptr; // Ref -1
	_sendEvent.Reset(); // 전송 끝난 SendBuffer 수명 해제

	if( numOfBytes == 0 )
	{
		// ProcessRecv()와 동일한 이유로, 이미 종료가 시작된 세션이면 Disconnect()를 다시 호출하지 않는다.
		if( IsConnected() )
			Disconnect(Iocp::CloseReason::SocketError);
		return;
	}

	// Disconnect()가 이미 호출된 종료 진행 중 세션 — OnSend()/재전송 없이, 아직 큐에 남아있는
	// 송신 데이터만 정리한다(_sendRegistered는 true로 두어 더 이상 RegisterSend()가 트리거되지
	// 않게 하며, 새 연결 사이클은 ProcessConnect()가 초기화한다).
	if( IsConnected() == false )
	{
		std::lock_guard<std::mutex> guard(_lock);
		_sendQueue.clear();
		_sendQueueBytes = 0;
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
// @details _disconnectCompleted만 세팅하고 통지는 TryFinalizeDisconnect()에 위임한다. 그 시점에
//          이미 게시돼 있던 WSARecv/WSASend가 아직 outstanding이면(다른 워커 스레드가 처리 중)
//          그 마지막 완료가 마저 통지한다. 자세한 배경은 헤더의 TryFinalizeDisconnect() 선언부
//          주석 참고.
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
// @details 상태에 따라 두 경로로 나뉜다.
//   - 연결 완료 상태(_connected == true): DisconnectEx를 게시한다. outstanding recv/send와
//     DisconnectEx의 완료가 모두 처리된 뒤 TryFinalizeDisconnect()가 OnDisconnected()를 통지한다.
//   - 그 외(_connected == false): Accept/ConnectEx가 진행 중이던 미연결 세션이거나 이미
//     종료가 시작된 세션이다. _disconnectNotified가 아직 false이면 FailConnect()와 같이
//     소켓을 직접 닫아 pending AcceptEx/ConnectEx를 취소시키고 OnDisconnected() 통지까지
//     즉시 완료한다. CNetService::Close()가 연결 완료 전에 등록된 세션에도 Disconnect()를
//     호출하므로, 이 경로가 있어야 그 세션 때문에 Close()가 무한 대기하지 않는다.
//     통지는 _disconnectNotified CAS로 최초 1회만 나가며 FailConnect()와 같은 가드를 공유한다.
//     (이미 종료가 시작된 세션에서 이 경로가 실행되면 outstanding I/O 완료 전에 통지될 수
//     있다. ProcessRecv/ProcessSend의 0바이트 완료는 IsConnected()일 때만 Disconnect()를
//     호출하므로 일반적인 종료 경로는 여기로 오지 않는다. 남는 경우는 CNetService::Close(),
//     Register* 실패 롤백, 사용자 코드의 중복 Disconnect() 호출이다 — 연결 상태를 명시적으로
//     구분하는 개선은 보류, TryFinalizeDisconnect() 설명 참고.)
//***************************************************************************
void CIocpSession::Disconnect(Iocp::CloseReason reason)
{
	if( _connected.exchange(false) == false )
	{
		// 미연결(Accept/ConnectEx 진행 중) 또는 이미 종료가 시작된 세션 — 소켓을 닫아 pending
		// AcceptEx/ConnectEx 취소를 유도하고 통지까지 즉시 마치는 강제 정리 경로(FailConnect()와 동일).
		FailConnect(reason);
		return;
	}

	_closeReason.store(reason, std::memory_order_release);

	// DisconnectEx(dwFlags = 0)로 연결을 종료한다. 소켓 핸들은 재사용하지 않고
	// 세션 소멸자가 닫는다.
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
//          세팅한다). 그래서 명시적으로
//          CNetAddress(_T("0.0.0.0"), 0)을 사용합니다.
//          이 함수의 모든 실패 경로는 FailConnect()를 호출해 OnDisconnected()까지
//          통지를 완료하므로, 호출부는 반환값이 false여도 별도로 정리할 것이
//          없습니다(FailConnect() 계약 — 헤더 주석 참고).
//***************************************************************************
bool CIocpSession::ConnectAsync(const CNetAddress& remoteAddr)
{
	// 서비스에 등록된 뒤 이 함수가 호출되기 전에 Close()가 Disconnect()로 이 세션을 이미 종료·통지했다면
	// (소켓은 닫혀 있다) 연결을 게시하지 않는다. 통지는 끝났으므로 호출부가 정리할 것은 없다.
	if( _disconnectNotified.load(std::memory_order_seq_cst) )
		return false;

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

	// 완료 자체가 실패로 통지됐으면(연결 거부/타임아웃/취소 등) 더 확인할 것 없이 실패다. 완료가 성공으로
	// 통지돼도 SO_ERROR로 한 번 더 확인한다.
	int32 sockError = 0;
	if( _connectEvent.errorCode != 0 || !CSocketUtils::GetSocketError(_socket, sockError) || sockError != 0 )
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
// @details Disconnect(Iocp::CloseReason)의 "미연결 상태 강제 종료" 경로와 _disconnectNotified CAS
//          가드를 공유하므로 어느 쪽이 먼저 실행되든 통지는 1회만 나간다. CNetService::Close()가
//          연결 완료 전인 세션에 먼저 Disconnect()를 호출해 소켓을 닫고 통지까지 마친 뒤, 취소된
//          ConnectEx의 완료가 뒤늦게 도착해 ProcessConnectEx()가 이 함수를 호출해도 중복 통지되지 않는다.
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