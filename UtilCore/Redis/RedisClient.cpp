
//***************************************************************************
// RedisClient.cpp: implementation of the CRedisClient class.
//
//***************************************************************************

#include "pch.h"
#include "RedisClient.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************

//***************************************************************************
// @brief CRedisClient 생성자
// @param iocpCore IOCP 코어 참조 객체
//***************************************************************************
CRedisClient::CRedisClient(CIocpCoreRef iocpCore)
	: _socket(INVALID_SOCKET), _iocpCore(iocpCore), _recvBuffer(DATABASE_BUFFER_SIZE)
{
}

//***************************************************************************
// @brief CRedisClient 소멸자
//***************************************************************************
CRedisClient::~CRedisClient()
{
	Disconnect();
}

//***************************************************************************
// @brief Redis 서버에 TCP 소켓 연결을 수행하고, IOCP 등록 및 논리 DB 선택까지 마침
// @param strIP 서버 IP 주소
// @param nPort 서버 포트 번호
// @param nDbIndex 접속 직후 SELECT로 고정할 Redis 논리 DB 인덱스(0이면 생략)
// @param nConnectTimeoutMs connect()/SELECT 응답 대기 최대 시간(ms)
// @return 성공 여부 (true: 성공, false: 실패)
//***************************************************************************
bool CRedisClient::Connect(const std::string& strIP, const uint16 nPort, const int32 nDbIndex, const int32 nConnectTimeoutMs)
{
	if( _socket.load(std::memory_order_acquire) != INVALID_SOCKET ) return false;

	SOCKET hSocket = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if( hSocket == INVALID_SOCKET ) return false;

	SOCKADDR_IN serverAddr = {};
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = ::htons(nPort);
	::inet_pton(AF_INET, strIP.c_str(), &serverAddr.sin_addr);

	// connect() 동안 무한정 블로킹되는 것을 막기 위해, 소켓을 잠깐
	// 논블로킹으로 전환하고 select()로 상한 시간을 건다. 결과 확인 후
	// 다시 블로킹 모드로 복원한다(이후 실제 송수신은 Overlapped I/O이므로
	// 이 전환은 여기서만 의미를 가진다). 이 시점에는 아직 _socket에
	// 값을 채우지 않은 로컬 핸들이라 다른 스레드가 볼 수 없으므로,
	// atomic 없이 그냥 지역 변수로 다룬다.
	u_long ulNonBlocking = 1;
	::ioctlsocket(hSocket, FIONBIO, &ulNonBlocking);

	bool bConnected = false;
	if( ::connect(hSocket, reinterpret_cast<SOCKADDR*>(&serverAddr), sizeof(serverAddr)) == 0 )
	{
		bConnected = true;
	}
	else if( ::WSAGetLastError() == WSAEWOULDBLOCK )
	{
		// Winsock 특성: 논블로킹 connect()가 실패로 끝나면 소켓이 writefds가
		// 아니라 exceptfds 쪽에 표시된다. exceptfds를 함께 넘기지 않으면
		// 즉시 거부(RST)되는 연결조차 select()가 타임아웃까지 잡아먹는다.
		fd_set writeSet;
		FD_ZERO(&writeSet);
		FD_SET(hSocket, &writeSet);

		fd_set exceptSet;
		FD_ZERO(&exceptSet);
		FD_SET(hSocket, &exceptSet);

		TIMEVAL tv;
		tv.tv_sec = nConnectTimeoutMs / 1000;
		tv.tv_usec = (nConnectTimeoutMs % 1000) * 1000;

		if( ::select(0, nullptr, &writeSet, &exceptSet, &tv) > 0 && !FD_ISSET(hSocket, &exceptSet) )
		{
			int32 nSockError = 0;
			int32 nOptLen = sizeof(nSockError);
			if( ::getsockopt(hSocket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&nSockError), &nOptLen) == 0
				&& nSockError == 0 )
			{
				bConnected = true;
			}
		}
	}

	u_long ulBlocking = 0;
	::ioctlsocket(hSocket, FIONBIO, &ulBlocking);

	if( !bConnected )
	{
		::closesocket(hSocket);
		return false;
	}

	// 여기서부터 _socket에 실제 값을 채운다 — 이 이후에야 GetHandle()/
	// IsConnected()가 유효한 핸들을 보게 되고, Disconnect()도 이 값을
	// 회수 대상으로 인식한다.
	_socket.store(hSocket, std::memory_order_release);

	if( !_iocpCore->Register(shared_from_this()) )
	{
		Disconnect();
		return false;
	}

	if( !RegisterRecv() )
	{
		Disconnect();
		return false;
	}

	// 접속 직후, 이 커넥션이 앞으로 사용할 논리 DB를 고정한다. nDbIndex가
	// 0(Redis 기본 DB)이면 SELECT를 보낼 필요가 없으므로 생략해 접속 지연을
	// 줄인다. 재연결(ReconnectLoop) 시에도 동일한 인자로 다시 호출되므로,
	// 재연결된 커넥션도 항상 같은 DB를 바라보게 된다.
	if( nDbIndex > 0 && !SelectDb(nDbIndex, nConnectTimeoutMs) )
	{
		Disconnect();
		return false;
	}

	return true;
}

//***************************************************************************
// @brief 소켓 연결을 종료하고 대기 중인 콜백 및 리소스를 정리함
// @return 성공 여부 (true: 성공)
//***************************************************************************
bool CRedisClient::Disconnect()
{
	// _socket을 원자적으로 회수한다. exchange에서 실제로 유효한 핸들을
	// 받아온 단 하나의 호출만 아래 정리 작업(close, 콜백 드레인, 파서
	// 리셋)을 수행하고, 동시에 호출된 나머지(예: recv/send 완료가 거의
	// 동시에 끊김을 통지한 경우)는 이미 INVALID_SOCKET을 보게 되어 아무
	// 것도 하지 않고 바로 반환한다 — 소켓 이중 close 및 _parser/큐에 대한
	// 동시 수정을 원천적으로 막는다.
	SOCKET hOldSocket = _socket.exchange(INVALID_SOCKET, std::memory_order_acq_rel);
	if( hOldSocket == INVALID_SOCKET )
		return true;

	::closesocket(hOldSocket);

	// 대기 중이던 콜백들을 락 밖으로 꺼내온 뒤 에러 값으로 완료 처리한다.
	// 락을 잡은 채로 콜백을 호출하면 콜백 내부에서 이 객체를 다시 건드릴
	// 경우 데드락 위험이 있으므로, 큐를 비우는 작업과 콜백 실행을 분리한다.
	std::vector<RedisCallback> vecPendingCallbacks;
	{
		std::lock_guard<std::mutex> lock(_queueLock);
		while( !_pendingCallbacks.empty() )
		{
			vecPendingCallbacks.push_back(_pendingCallbacks.front());
			_pendingCallbacks.pop();
		}
	}

	RedisValue disconnectedVal;
	disconnectedVal.eType = ERedisType::Error;
	disconnectedVal.strVal = "ERR connection closed";

	for( auto& fnCallback : vecPendingCallbacks )
	{
		if( fnCallback )
			fnCallback(disconnectedVal);
	}

	_parser.Reset();
	return true;
}

//***************************************************************************
// @brief 명령어를 RESP 변환하여 비동기 전송하고 콜백을 큐에 등록함
// @param vecArgs Redis 명령어 인자 목록
// @param fnCallback 응답 처리 콜백 함수
// @return 전송 등록 성공 여부 (true: 성공, false: 실패)
//***************************************************************************
bool CRedisClient::SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnCallback)
{
	if( _socket.load(std::memory_order_acquire) == INVALID_SOCKET ) return false;

	// RESP 문자열을 std::string으로 조립한 뒤 송신 버퍼로 다시 복사하는 대신,
	// 정확한 크기를 먼저 계산해 송신 버퍼에 바로 인코딩해 넣어 복사를 한 번
	// 줄인다.
	uint32 nCmdSize = CRedisCommandBuilder::CalcEncodedSize(vecArgs);
	CSendBufferRef sendBuffer = CSendBufferManager::Open(nCmdSize);
	CRedisCommandBuilder::Encode(vecArgs, reinterpret_cast<char*>(sendBuffer->Buffer()));
	sendBuffer->Close(nCmdSize);

	{
		std::lock_guard<std::mutex> lock(_queueLock);
		_pendingCallbacks.push(fnCallback);
	}

	_pendingSendBuffer = sendBuffer;
	_sendOffset = 0;
	_sendTotalSize = nCmdSize;

	if( !DoSend() )
	{
		std::lock_guard<std::mutex> lock(_queueLock);
		_pendingCallbacks.pop();

		_pendingSendBuffer.reset();
		_sendOffset = 0;
		_sendTotalSize = 0;
		return false;
	}

	return true;
}

//***************************************************************************
// @brief _pendingSendBuffer의 _sendOffset 위치부터 나머지를 WSASend로 등록함
//***************************************************************************
bool CRedisClient::DoSend()
{
	_sendEvent.Init();
	_sendEvent.owner = shared_from_this();

	// SendEvent 구조체의 sendBuffers CVector 적용
	_sendEvent.sendBuffers.clear();
	_sendEvent.sendBuffers.push_back(_pendingSendBuffer);

	WSABUF wsaBuf;
	wsaBuf.buf = reinterpret_cast<char*>(_pendingSendBuffer->Buffer()) + _sendOffset;
	wsaBuf.len = _sendTotalSize - _sendOffset;

	SOCKET hSocket = _socket.load(std::memory_order_acquire);
	DWORD dwNumOfBytes = 0;
	if( ::WSASend(hSocket, &wsaBuf, 1, &dwNumOfBytes, 0, &_sendEvent, NULL) == SOCKET_ERROR )
	{
		if( ::WSAGetLastError() != WSA_IO_PENDING )
		{
			_sendEvent.owner = nullptr;
			_sendEvent.sendBuffers.clear();
			return false;
		}
	}

	return true;
}

//***************************************************************************
// @brief 비동기 수신(WSARecv)을 IOCP에 등록함
// @return 등록 성공 여부 (true: 성공, false: 실패)
//***************************************************************************
bool CRedisClient::RegisterRecv()
{
	SOCKET hSocket = _socket.load(std::memory_order_acquire);
	if( hSocket == INVALID_SOCKET ) return false;

	_recvEvent.Init();
	_recvEvent.owner = shared_from_this();

	// CRingBuffer에서 IOCP용 WSABUF 배열 수거 (최대 2개 청크)
	WSABUF wsaBufs[2];
	int32 bufferCount = _recvBuffer.GetWSARecvBuffers(wsaBufs);

	if( bufferCount == 0 ) return false;

	DWORD dwBytesTransferred = 0;
	DWORD dwFlags = 0;

	if( ::WSARecv(hSocket, wsaBufs, bufferCount, &dwBytesTransferred, &dwFlags, &_recvEvent, NULL) == SOCKET_ERROR )
	{
		if( ::WSAGetLastError() != WSA_IO_PENDING )
		{
			_recvEvent.owner = nullptr;
			return false;
		}
	}

	return true;
}

//***************************************************************************
// @brief IOCP 완료 통지가 왔을 때 IocpCore가 호출하는 가상 함수 구현부입니다.
// @param iocpEvent 완료된 IOCP 이벤트 포인터
// @param numOfBytes 전송된 바이트 수 (0인 경우 연결 끊김)
//***************************************************************************
void CRedisClient::Dispatch(CIocpEvent* iocpEvent, int32 numOfBytes)
{
	if( iocpEvent == &_recvEvent )
	{
		if( numOfBytes == 0 )
		{
			Disconnect();
			return;
		}
		ProcessRecv(numOfBytes);
	}
	else if( iocpEvent == &_sendEvent )
	{
		if( numOfBytes == 0 )
		{
			Disconnect();
			return;
		}

		_sendOffset += static_cast<uint32>(numOfBytes);

		if( _sendOffset < _sendTotalSize )
		{
			// 부분 전송 — WSASend 완료가 요청한 바이트를 전부 보냈다는 보장은
			// 없으므로, 아직 안 보낸 나머지를 이어서 전송한다.
			if( !DoSend() )
				Disconnect();

			return;
		}

		_sendEvent.owner = nullptr;
		_sendEvent.sendBuffers.clear();
		_pendingSendBuffer.reset();
		_sendOffset = 0;
		_sendTotalSize = 0;
	}
}

//***************************************************************************
// @brief 수신된 데이터를 파서에 넘기고, 완성된 RESP 값을 모두 꺼내 처리함
// @param dwBytesTransferred 수신받은 바이트 크기
//***************************************************************************
void CRedisClient::ProcessRecv(DWORD dwBytesTransferred)
{
	// 링 버퍼 쓰기 커서 이동
	_recvBuffer.MoveWriteBuffer(dwBytesTransferred);

	// 수신 링 버퍼는 소켓에서 갓 도착한 바이트를 IOCP 완료 시점까지만
	// 보관하는 역할이다. 그 바이트를 파서에 Feed()로 넘기고 나면 미완성
	// 패킷 재조립 책임은 CRedisParser 하나로 완전히 넘어가므로, 넘긴
	// 만큼은 곧바로 읽기 커서를 이동시켜 같은 바이트가 다음 수신 완료
	// 때 다시 파서로 들어가는 일이 없도록 한다.
	int64 nReadSize = _recvBuffer.GetSizeUsed();
	if( nReadSize > 0 )
	{
		_parser.Feed(_recvBuffer.GetReadBuffer(), static_cast<int32>(nReadSize));
		_recvBuffer.MoveReadBuffer(static_cast<int32>(nReadSize));
	}

	RedisValue parsedVal;
	while( _parser.TryParse(parsedVal) )
	{
		OnReceiptResponse(parsedVal);
	}

	// 다음 수신을 등록하지 못하면 이 소켓은 더 이상 어떤 응답도 받을 수
	// 없다 — 대기 중인 콜백이 영영 완료되지 않고 매달리는 것을 막기 위해
	// 연결을 즉시 끊어 Disconnect()의 에러 완료 경로로 정리되게 한다.
	if( !RegisterRecv() )
	{
		Disconnect();
	}
}

//***************************************************************************
// @brief 파싱 완료된 응답을 대기 중인 콜백에 전달하여 실행함
// @param value 파싱 완료된 Redis 응답 데이터
//***************************************************************************
void CRedisClient::OnReceiptResponse(const RedisValue& value)
{
	RedisCallback fnCallback = nullptr;

	{
		std::lock_guard<std::mutex> lock(_queueLock);
		if( !_pendingCallbacks.empty() )
		{
			fnCallback = _pendingCallbacks.front();
			_pendingCallbacks.pop();
		}
	}

	if( fnCallback )
	{
		fnCallback(value);
	}
}

//***************************************************************************
// @brief SelectDb()가 SendCommand()의 비동기 콜백 결과를 동기 대기로 바꾸기
//        위해 사용하는 완료 신호 상태
//***************************************************************************
namespace
{
	struct SSelectDbState
	{
		std::mutex              lock;
		std::condition_variable cv;
		bool                    bDone = false;
		bool                    bSuccess = false;
	};
}

//***************************************************************************
// @brief 접속 직후 이 커넥션이 사용할 논리 DB를 SELECT 명령으로 고정함
//***************************************************************************
bool CRedisClient::SelectDb(const int32 nDbIndex, const int32 nTimeoutMs)
{
	auto pState = std::make_shared<SSelectDbState>();

	CVector<std::string> args;
	args.push_back("SELECT");
	args.push_back(std::to_string(nDbIndex));

	bool bSent = SendCommand(args, [pState](const RedisValue& res) {
		{
			std::lock_guard<std::mutex> lock(pState->lock);
			pState->bSuccess = (res.eType != ERedisType::Error);
			pState->bDone = true;
		}
		pState->cv.notify_all();
		});

	if( !bSent ) return false;

	std::unique_lock<std::mutex> guard(pState->lock);
	pState->cv.wait_for(guard, std::chrono::milliseconds(nTimeoutMs), [pState] { return pState->bDone; });

	return pState->bDone && pState->bSuccess;
}