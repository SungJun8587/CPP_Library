
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
// @brief Redis 서버에 TCP 소켓 연결을 수행하고 IOCP에 등록함
// @param strIP 서버 IP 주소
// @param nPort 서버 포트 번호
// @return 성공 여부 (true: 성공, false: 실패)
//***************************************************************************
bool CRedisClient::Connect(const std::string& strIP, const uint16 nPort)
{
	if( _socket != INVALID_SOCKET ) return false;

	_socket = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if( _socket == INVALID_SOCKET ) return false;

	SOCKADDR_IN serverAddr = {};
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = ::htons(nPort);
	::inet_pton(AF_INET, strIP.c_str(), &serverAddr.sin_addr);

	if( ::connect(_socket, reinterpret_cast<SOCKADDR*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR )
	{
		Disconnect();
		return false;
	}

	if( !_iocpCore->Register(shared_from_this()) )
	{
		Disconnect();
		return false;
	}

	return RegisterRecv();
}

//***************************************************************************
// @brief 소켓 연결을 종료하고 대기 중인 콜백 및 리소스를 정리함
// @return 성공 여부 (true: 성공)
//***************************************************************************
bool CRedisClient::Disconnect()
{
	if( _socket != INVALID_SOCKET )
	{
		::closesocket(_socket);
		_socket = INVALID_SOCKET;
	}

	std::lock_guard<std::mutex> lock(_queueLock);
	while( !_pendingCallbacks.empty() )
		_pendingCallbacks.pop();

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
	if( _socket == INVALID_SOCKET ) return false;

	std::string strCmd = CRedisCommandBuilder::Build(vecArgs);

	{
		std::lock_guard<std::mutex> lock(_queueLock);
		_pendingCallbacks.push(fnCallback);
	}

	CSendBufferRef sendBuffer = CSendBufferManager::Open(static_cast<uint32>(strCmd.size()));
	::memcpy(sendBuffer->Buffer(), strCmd.data(), strCmd.size());
	sendBuffer->Close(static_cast<uint32>(strCmd.size()));

	_sendEvent.Init();
	_sendEvent.owner = shared_from_this();

	// SendEvent 구조체의 sendBuffers CVector 적용
	_sendEvent.sendBuffers.clear();
	_sendEvent.sendBuffers.push_back(sendBuffer);

	WSABUF wsaBuf;
	wsaBuf.buf = reinterpret_cast<char*>(sendBuffer->Buffer());
	wsaBuf.len = sendBuffer->WriteSize();

	DWORD dwNumOfBytes = 0;
	if( ::WSASend(_socket, &wsaBuf, 1, &dwNumOfBytes, 0, &_sendEvent, NULL) == SOCKET_ERROR )
	{
		if( ::WSAGetLastError() != WSA_IO_PENDING )
		{
			std::lock_guard<std::mutex> lock(_queueLock);
			_pendingCallbacks.pop();

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
	if( _socket == INVALID_SOCKET ) return false;

	_recvEvent.Init();
	_recvEvent.owner = shared_from_this();

	// CRingBuffer에서 IOCP용 WSABUF 배열 수거 (최대 2개 청크)
	WSABUF wsaBufs[2];
	int32 bufferCount = _recvBuffer.GetWSARecvBuffers(wsaBufs);

	if( bufferCount == 0 ) return false;

	DWORD dwBytesTransferred = 0;
	DWORD dwFlags = 0;

	if( ::WSARecv(_socket, wsaBufs, bufferCount, &dwBytesTransferred, &dwFlags, &_recvEvent, NULL) == SOCKET_ERROR )
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
		_sendEvent.owner = nullptr;
		_sendEvent.sendBuffers.clear();
	}
}

//***************************************************************************
// @brief 수신된 데이터를 버퍼링하고 RESP 파싱을 수행함
// @param dwBytesTransferred 수신받은 바이트 크기
//***************************************************************************
void CRedisClient::ProcessRecv(DWORD dwBytesTransferred)
{
	// 링 버퍼 쓰기 커서 이동
	_recvBuffer.MoveWriteBuffer(dwBytesTransferred);

	int32 nConsumedBytes = 0;
	RedisValue parsedVal;

	while( true )
	{
		int64 nReadSize = _recvBuffer.GetSizeUsed();
		if( nReadSize <= 0 ) break;

		// GetReadBuffer()를 사용해 직접 읽기 커서의 데이터를 파싱
		if( _parser.Parse(_recvBuffer.GetReadBuffer(), static_cast<int32>(nReadSize), nConsumedBytes, parsedVal) )
		{
			_recvBuffer.MoveReadBuffer(nConsumedBytes);
			OnReceiptResponse(parsedVal);
		}
		else
		{
			break;
		}
	}

	RegisterRecv();
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