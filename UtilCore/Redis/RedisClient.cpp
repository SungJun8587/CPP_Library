
//***************************************************************************
// RedisClient.cpp: implementation of the CRedisClient class.
//
//***************************************************************************

#include "pch.h"
#include "RedisClient.h"

// tcp_keepalive 구조체 / SIO_KEEPALIVE_VALS 정의 — Connect()의 keepalive
// 설정에서 사용. winsock2.h는 pch.h에서 이미 포함됐다고 가정한다(WSASocket
// 등을 이미 이 파일에서 쓰고 있으므로).
#include <mstcpip.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace
{
	// 대형 명령을 한 번 인코딩할 때 쓰는 스레드 로컬 임시 버퍼가 이 크기를 넘게 커지면 사용 후 해제한다.
	constexpr size_t kScratchRetainLimit = 1u << 20;

	//***************************************************************************
	// @brief RESP로 인코딩한 명령 하나를 송신 버퍼 하나 이상에 담습니다.
	// @param vecArgs Redis 명령어 인자 목록
	// @param nCmdSize CRedisCommandBuilder::CalcEncodedSize(vecArgs) 결과
	// @param outBuffers 명령 전체를 순서대로 이어 붙인 송신 버퍼들
	// @return 성공 여부 (송신 버퍼를 확보하지 못하면 false이고 outBuffers는 비워진다)
	// @details SendBuffer 하나는 청크(Iocp::SEND_BUFFER_CHUNK_SIZE)를 넘을 수 없다.
	//          명령이 그 이하이면 송신 버퍼에 바로 인코딩해 복사를 한 번 줄인다. 그보다 크면
	//          연속 메모리에 한 번 인코딩한 뒤 청크 크기 단위로 나눠 여러 SendBuffer에 담고,
	//          DoSend()가 Scatter-Gather로 한 번에 전송한다.
	//***************************************************************************
	bool BuildSendBuffers(const CVector<std::string>& vecArgs, uint32 nCmdSize, CVector<CSendBufferRef>& outBuffers)
	{
		outBuffers.clear();

		if( nCmdSize <= Iocp::SEND_BUFFER_CHUNK_SIZE )
		{
			CSendBufferRef sendBuffer = CSendBufferManager::Open(nCmdSize);
			if( sendBuffer == nullptr )
				return false;

			CRedisCommandBuilder::Encode(vecArgs, reinterpret_cast<char*>(sendBuffer->Buffer()));
			sendBuffer->Close(nCmdSize);
			outBuffers.push_back(std::move(sendBuffer));
			return true;
		}

		thread_local std::vector<char> scratch;
		scratch.resize(nCmdSize);
		CRedisCommandBuilder::Encode(vecArgs, scratch.data());

		bool success = true;
		uint32 offset = 0;
		while( offset < nCmdSize )
		{
			const uint32 pieceSize = (std::min)(nCmdSize - offset, Iocp::SEND_BUFFER_CHUNK_SIZE);

			CSendBufferRef sendBuffer = CSendBufferManager::Open(pieceSize);
			if( sendBuffer == nullptr )
			{
				success = false;
				break;
			}

			std::memcpy(sendBuffer->Buffer(), scratch.data() + offset, pieceSize);
			sendBuffer->Close(pieceSize);
			outBuffers.push_back(std::move(sendBuffer));
			offset += pieceSize;
		}

		if( scratch.capacity() > kScratchRetainLimit )
			std::vector<char>().swap(scratch);

		if( !success )
			outBuffers.clear();

		return success;
	}

	//***************************************************************************
	// @brief 송신 버퍼들 중 이미 보낸 offset 바이트를 건너뛴 나머지를 WSABUF 배열로 만듭니다.
	// @param buffers 명령 전체를 순서대로 이어 붙인 송신 버퍼들
	// @param offset 이미 보낸 바이트 수
	// @param outWsaBufs 결과 (offset 이후의 데이터를 가리키는 WSABUF들, 이전 내용은 지워진다)
	//***************************************************************************
	void BuildWsaBuffers(const CVector<CSendBufferRef>& buffers, uint32 offset, CVector<WSABUF>& outWsaBufs)
	{
		outWsaBufs.clear();
		outWsaBufs.reserve(buffers.size());

		uint32 skip = offset;
		for( const CSendBufferRef& buffer : buffers )
		{
			const uint32 len = buffer->WriteSize();
			if( skip >= len )
			{
				skip -= len;
				continue;
			}

			WSABUF wsaBuf;
			wsaBuf.buf = reinterpret_cast<char*>(buffer->Buffer()) + skip;
			wsaBuf.len = len - skip;
			skip = 0;
			outWsaBufs.push_back(wsaBuf);
		}
	}
}

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
// @param nDbIndex 접속 직후 SELECT로 고정할 Redis 논리 DB 인덱스
// @param nConnectTimeoutMs connect()/SELECT 응답 대기 최대 시간(ms)
// @return 성공 여부 (true: 성공, false: 실패)
//***************************************************************************
bool CRedisClient::Connect(const std::string& strIP, const uint16 nPort, const int32 nDbIndex, const int32 nConnectTimeoutMs)
{
	if( _socket.load(std::memory_order_acquire) != INVALID_SOCKET ) return false;

	// [추가 — 방어적 계약 검증] 이전 연결의 WSARecv/WSASend 완료 통지가
	// 아직 IOCP 큐에 남아있는 상태에서 Connect()가 같은 _recvEvent/
	// _sendEvent(OVERLAPPED 메모리)를 재사용하면, 그 stale completion이
	// 이번 새 연결의 것으로 오인될 위험이 있다(RedisClient.h 상단
	// "재연결 시 stale completion" 설명 참고). 정상적으로
	// CRedisConnectionPool::ReconnectLoop()을 거치면 HasNoOutstandingIo()를
	// 미리 확인한 뒤에만 이 함수를 부르므로 절대 걸리지 않아야 하지만,
	// 이 클래스 자체의 계약으로 명시해서 다른 경로로 잘못 호출되는 것도
	// 방지한다.
	if( !HasNoOutstandingIo() )
		return false;

	// 이전 연결의 잔여 수신 상태를 비운다. 이 시점에는 걸려있는 I/O가 없으므로(위 확인)
	// 어떤 스레드도 _recvBuffer/_parser를 건드리지 않는다. 이전 연결이 프로토콜 오류나
	// Disconnect()와 겹친 수신 처리로 끝났다면 파서/링버퍼에 그 연결의 미완성 바이트가
	// 남아 있을 수 있고, 그대로 두면 새 연결의 첫 응답 앞에 붙어 스트림이 어긋난다.
	{
		std::lock_guard<std::mutex> lock(_recvLock);
		_parser.Reset();
		_recvBuffer.Clear();
	}

	// [추가] 타임아웃 값 자체가 비정상(0 이하)이면 아래 select()의
	// TIMEVAL 계산이 "무한 대기"나 "즉시 타임아웃"처럼 의도치 않게
	// 동작할 수 있으므로 방어한다.
	if( nConnectTimeoutMs <= 0 )
		return false;

	SOCKET hSocket = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if( hSocket == INVALID_SOCKET ) return false;

	SOCKADDR_IN serverAddr = {};
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = ::htons(nPort);
	// [추가] inet_pton 반환값을 확인한다 — 예전엔 무시하고 있어서, 잘못된
	// IP 문자열(오타, 빈 문자열 등)이 들어와도 serverAddr.sin_addr가
	// 초기화 안 된 채(혹은 이전 호출의 잔여값) 그대로 connect()를
	// 시도해버릴 수 있었다. 1이 아니면 변환 실패다.
	if( ::inet_pton(AF_INET, strIP.c_str(), &serverAddr.sin_addr) != 1 )
	{
		::closesocket(hSocket);
		return false;
	}

	// connect() 동안 무한정 블로킹되는 것을 막기 위해, 소켓을 잠깐
	// 논블로킹으로 전환하고 select()로 상한 시간을 건다. 결과 확인 후
	// 다시 블로킹 모드로 복원한다(이후 실제 송수신은 Overlapped I/O이므로
	// 이 전환은 여기서만 의미를 가진다). 이 시점에는 아직 _socket에
	// 값을 채우지 않은 로컬 핸들이라 다른 스레드가 볼 수 없으므로,
	// atomic 없이 그냥 지역 변수로 다룬다.
	u_long ulNonBlocking = 1;
	if( ::ioctlsocket(hSocket, FIONBIO, &ulNonBlocking) != 0 )
	{
		// 논블로킹 전환 자체가 실패하면 이후 connect()가 블로킹 모드로
		// 호출되어 nConnectTimeoutMs를 무시하고 무한정 블로킹될 위험이
		// 있다 — 여기서 바로 실패 처리한다.
		::closesocket(hSocket);
		return false;
	}

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
	if( ::ioctlsocket(hSocket, FIONBIO, &ulBlocking) != 0 )
	{
		// 블로킹 모드 복원 실패 — 이 소켓을 그대로 쓰면 이후 WSARecv/WSASend
		// (Overlapped I/O이므로 블로킹/논블로킹 모드 자체엔 크게 안 좌우되긴
		// 하지만) 상태가 불확실해지므로 안전하게 실패 처리한다.
		::closesocket(hSocket);
		return false;
	}

	if( !bConnected )
	{
		::closesocket(hSocket);
		return false;
	}

	// 여기서부터 _socket에 실제 값을 채운다 — 이 이후에야 GetHandle()/
	// IsConnected()가 유효한 핸들을 보게 되고, Disconnect()도 이 값을
	// 회수 대상으로 인식한다.
	_socket.store(hSocket, std::memory_order_release);

	// TCP keepalive를 켠다. 풀의 커넥션은 요청이 없는 동안 오래 놀 수 있는데,
	// 그 사이 경로상의 장비(방화벽/NAT/로드밸런서 등)가 idle 연결을 조용히
	// 정리하면 양쪽 모두 모른 채로 있다가 다음 명령을 보낼 때에야 실패로
	// 발견한다. Windows 기본 keepalive는 2시간 idle 뒤에야 동작하므로
	// WSAIoctl(SIO_KEEPALIVE_VALS)로 30초 idle 후 10초 간격으로 프로브하도록
	// 재정의한다. 설정에 실패해도 연결 자체는 쓸 수 있으므로 치명적인
	// 에러로 취급하지 않는다.
	{
		tcp_keepalive keepaliveSettings{};
		keepaliveSettings.onoff = 1;
		keepaliveSettings.keepalivetime = 30000;		// 30초 idle 후 첫 프로브
		keepaliveSettings.keepaliveinterval = 10000;	// 이후 10초 간격으로 재프로브

		DWORD dwBytesReturned = 0;
		if( ::WSAIoctl(hSocket, SIO_KEEPALIVE_VALS, &keepaliveSettings, sizeof(keepaliveSettings),
			nullptr, 0, &dwBytesReturned, nullptr, nullptr) == SOCKET_ERROR )
		{
			LOG_ERROR(_T("CRedisClient::Connect: SIO_KEEPALIVE_VALS 설정 실패(에러=%d) — keepalive 없이 계속 진행"), ::WSAGetLastError());
		}
	}

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
// @brief 소켓 close와 파서 리셋을 함께 수행하는 내부 헬퍼.
// @details Disconnect()와 SendCommand()의 DoSend() 실패 처리 양쪽에서
//          공유한다 — RedisClient.h의 선언부 설명 참고.
//***************************************************************************
void CRedisClient::CleanupSocketAndParser(SOCKET hOldSocket)
{
	if( hOldSocket == INVALID_SOCKET )
		return;

	::closesocket(hOldSocket);

	std::lock_guard<std::mutex> lock(_recvLock);
	_parser.Reset();
}

//***************************************************************************
// @brief 소켓 연결을 종료하고 대기 중인 콜백 및 리소스를 정리함
// @return 성공 여부 (true: 성공)
//***************************************************************************
//***************************************************************************
// @brief ERedisDisconnectReason을 콜백에 전달할 문자열로 변환함
//***************************************************************************
const char* CRedisClient::DisconnectReasonToString(ERedisDisconnectReason reason)
{
	switch( reason )
	{
	case ERedisDisconnectReason::ConnectionClosedRecv:		return "ERR connection closed (recv 0 bytes)";
	case ERedisDisconnectReason::ConnectionClosedSend:		return "ERR connection closed (send 0 bytes)";
	case ERedisDisconnectReason::SendFailed:				return "ERR send failed";
	case ERedisDisconnectReason::ProtocolError:			return "ERR protocol error";
	case ERedisDisconnectReason::RecvRegistrationFailed:	return "ERR recv registration failed";
	case ERedisDisconnectReason::Generic:
	default:												return "ERR connection closed";
	}
}

bool CRedisClient::Disconnect(ERedisDisconnectReason reason)
{
	// _socket을 원자적으로 회수한다. exchange에서 실제로 유효한 핸들을
	// 받아온 단 하나의 호출만 아래 정리 작업을 수행하고, 동시에 호출된
	// 나머지(예: recv/send 완료가 거의 동시에 끊김을 통지한 경우, 또는
	// SendCommand()가 DoSend() 실패 시 직접 소켓을 회수한 경우)는 이미
	// INVALID_SOCKET을 보게 되어 아무 것도 하지 않고 바로 반환한다.
	SOCKET hOldSocket = _socket.exchange(INVALID_SOCKET, std::memory_order_acq_rel);
	if( hOldSocket == INVALID_SOCKET )
		return true;

	CleanupSocketAndParser(hOldSocket);

	// [수정 — 버그 수정] _commandLock으로 SendCommand()의 "소켓 확인 →
	// 콜백 등록" 임계구역과 여기의 "콜백 회수"를 같은 동기화 영역으로
	// 묶는다 — RedisClient.h 상단 "콜백 0회 실행 경합" 설명 참고. 락을
	// 잡은 채로 콜백을 호출하면 콜백 내부에서 이 객체를 다시 건드릴 경우
	// 데드락 위험이 있으므로, 콜백을 로컬로 옮겨온 뒤 락 밖에서 호출한다.
	RedisCallback fnPendingCallback;
	{
		std::lock_guard<std::mutex> lock(_commandLock);
		fnPendingCallback = std::move(_pendingCallback);
		_pendingCallback = nullptr;
		_pendingSendBuffers.clear();
		_sendOffset = 0;
		_sendTotalSize = 0;
		_nextSendBuffers.clear();
		_nextSendTotalSize = 0;
	}

	if( fnPendingCallback )
	{
		RedisValue disconnectedVal;
		disconnectedVal.eType = ERedisType::Error;
		disconnectedVal.strVal = DisconnectReasonToString(reason);
		fnPendingCallback(disconnectedVal);
	}

	return true;
}

//***************************************************************************
// @brief 명령어를 RESP 변환하여 비동기 전송하고 콜백을 등록함
// @param vecArgs Redis 명령어 인자 목록
// @param fnCallback 응답 처리 콜백 함수
// @return 전송 등록 성공 여부 (true: 성공, false: 실패)
// @details [수정 — 버그 수정] 예전엔 "소켓이 유효한지 확인"과 "콜백을
//          큐에 등록"이 서로 다른 시점이라, 그 사이에 다른 스레드의
//          Disconnect()가 끼어들면 등록될 콜백이 드레인 대상에서 빠져나가
//          영원히 호출되지 않는 경합이 있었다(RedisClient.h 상단
//          "콜백 0회 실행 경합" 설명 참고). 이제 "소켓 확인 → 콜백 등록 →
//          송신 상태 설정"을 전부 _commandLock 하나로 묶어서, Disconnect()의
//          콜백 회수와 절대 겹치지 않게 한다 — 콜백은 항상 어느 한쪽에서만
//          발견되어 정확히 한 번 호출된다.
//***************************************************************************
bool CRedisClient::SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnCallback)
{
	const char* pszErrorMsg = nullptr; // non-null이면 락 밖에서 이 메시지로 fnCallback을 에러 호출

	{
		std::lock_guard<std::mutex> lock(_commandLock);

		if( _socket.load(std::memory_order_acquire) == INVALID_SOCKET )
		{
			// 이 시점에 이미 끊겨있으면 콜백을 등록할 기회조차 없다(따라서
			// Disconnect()의 드레인 대상이 될 수도 없다) — 락을 빠져나간
			// 뒤 아래에서 직접, 정확히 한 번 호출한다.
			pszErrorMsg = "ERR connection closed";
		}
		else if( _pendingCallback )
		{
			// [추가 — 방어적 계약 검증] 이미 미완료 SendCommand()가
			// 존재하는 상태에서 또 호출됐다 — 클래스 문서 상단의
			// "한 시점에 미완료 SendCommand는 하나만 존재" 계약 위반이다.
			// 정상적으로 CRedisConnectionPool을 거치면(커넥션을 대여한
			// 뒤 응답을 받을 때까지 재대여 안 함) 절대 발생하지 않아야
			// 하지만, 여기서 명시적으로 막아둬야 실수로 이 계약을 어기는
			// 호출이 생겼을 때 기존 명령의 _pendingSendBuffers/_sendOffset
			// 등을 조용히 덮어쓰는 대신 바로 드러난다.
			LOG_ERROR(_T("CRedisClient::SendCommand: 이미 미완료 명령이 있는 상태에서 재호출됨 — 계약 위반"));
			pszErrorMsg = "ERR command already in flight";
		}
		else
		{
			// RESP 문자열을 std::string으로 조립한 뒤 송신 버퍼로 다시
			// 복사하는 대신, 정확한 크기를 먼저 계산해 송신 버퍼에 바로
			// 인코딩해 넣어 복사를 한 번 줄인다. 한 SendBuffer는 청크 크기
			// (Iocp::SEND_BUFFER_CHUNK_SIZE)를 넘을 수 없으므로, 그보다 큰
			// 명령은 BuildSendBuffers()가 여러 SendBuffer로 나눠 담는다.
			uint32 nCmdSize = CRedisCommandBuilder::CalcEncodedSize(vecArgs);
			CVector<CSendBufferRef> sendBuffers;
			if( !BuildSendBuffers(vecArgs, nCmdSize, sendBuffers) )
			{
				// 송신 버퍼를 확보하지 못했다 — 콜백을 등록하지 않았으므로 락 밖에서 직접 에러로 호출한다.
				pszErrorMsg = "ERR send buffer allocation failed";
			}
			else
			{
				_pendingCallback = fnCallback;

				// 직전 명령의 송신 완료 통지(WSASend completion)가 아직 처리되지 않았다면
				// (_pendingSendBuffers가 남아 있음) _sendEvent를 다시 쓸 수 없다. 응답이 송신 완료
				// 통지보다 먼저 처리되면 풀이 이 커넥션을 바로 반납·재대여할 수 있기 때문이다.
				// 이번 명령은 다음 명령으로 보관해 두었다가 그 송신 완료 처리가 이어서 보낸다.
				if( !_pendingSendBuffers.empty() )
				{
					_nextSendBuffers = std::move(sendBuffers);
					_nextSendTotalSize = nCmdSize;
					return true;
				}

				_pendingSendBuffers = std::move(sendBuffers);
				_sendOffset = 0;
				_sendTotalSize = nCmdSize;

				if( DoSend() )
					return true; // 정상 등록 완료 — 콜백은 응답 수신 또는 Disconnect() 경로에서 정확히 한 번 호출됨

				// DoSend() 실패 — 방금 등록한 콜백을 이 락 안에서 직접
				// 회수해 스스로 정리한다(아직 _commandLock 안이므로
				// Disconnect()가 끼어들어 이 콜백을 가져갈 수 없다).
				_pendingCallback = nullptr;
				_pendingSendBuffers.clear();
				_sendOffset = 0;
				_sendTotalSize = 0;

				// 소켓이 실제로 깨졌을 가능성이 높으니 직접 회수/정리한다.
				// Disconnect()를 그대로 다시 호출하면 이미 쥐고 있는
				// _commandLock을 그 함수가 또 잡으려다 데드락이 나므로,
				// 필요한 부분(소켓 회수 + 파서 리셋)만 CleanupSocketAndParser()로
				// 인라인 수행한다. 이미 다른 스레드가 먼저 끊었다면(hOldSocket
				// 이 INVALID_SOCKET) 아무 것도 하지 않는다.
				SOCKET hOldSocket = _socket.exchange(INVALID_SOCKET, std::memory_order_acq_rel);
				CleanupSocketAndParser(hOldSocket);

				pszErrorMsg = "ERR connection closed";
			}
		}
	}

	if( pszErrorMsg && fnCallback )
	{
		RedisValue disconnectedVal;
		disconnectedVal.eType = ERedisType::Error;
		disconnectedVal.strVal = pszErrorMsg;
		fnCallback(disconnectedVal);
	}

	return false;
}

//***************************************************************************
// @brief _pendingSendBuffers의 _sendOffset 위치부터 나머지를 WSASend(Scatter-Gather)로 등록함
//***************************************************************************
bool CRedisClient::DoSend()
{
	_sendEvent.Init();
	_sendEvent.owner = shared_from_this();

	// WSASend가 pending인 동안 버퍼 수명을 보장하기 위해 사본을 이벤트에 보관한다 —
	// 완료 통지 전에 Disconnect()가 _pendingSendBuffers를 회수해도 이 사본이 유지한다.
	_sendEvent.sendBuffers = _pendingSendBuffers;

	// 이미 보낸 _sendOffset 바이트를 건너뛴 나머지를 Scatter-Gather로 구성한다.
	// WSABUF 배열은 이벤트가 보유하며 이 WSASend의 완료 통지 전에는 변경되지 않는다.
	BuildWsaBuffers(_pendingSendBuffers, _sendOffset, _sendEvent.wsaBufs);
	if( _sendEvent.wsaBufs.empty() )
	{
		// 보낼 데이터가 없다 — 호출 계약(_sendOffset < _sendTotalSize) 위반이므로 실패로 취급한다.
		_sendEvent.owner = nullptr;
		_sendEvent.Reset();
		return false;
	}

	SOCKET hSocket = _socket.load(std::memory_order_acquire);
	DWORD dwNumOfBytes = 0;

	_pendingIoCount.fetch_add(1, std::memory_order_seq_cst);

	if( ::WSASend(hSocket, _sendEvent.wsaBufs.data(), static_cast<DWORD>(_sendEvent.wsaBufs.size()), &dwNumOfBytes, 0, &_sendEvent, NULL) == SOCKET_ERROR )
	{
		if( ::WSAGetLastError() != WSA_IO_PENDING )
		{
			_pendingIoCount.fetch_sub(1, std::memory_order_seq_cst); // 게시 실패 롤백 — completion이 안 옴
			_sendEvent.owner = nullptr;
			_sendEvent.Reset();
			return false;
		}
	}

	return true;
}

//***************************************************************************
// @brief 비동기 수신(WSARecv)을 IOCP에 등록함
// @return 등록 성공 여부 (true: 성공, false: 실패)
// @details 링버퍼의 쓰기 가능 영역을 최대 2개의 연속 구간(WSABUF)으로 나눠
//          한 번의 WSARecv에 넘긴다. _pendingIoCount는 게시 직전에 증가시키고,
//          게시가 즉시 실패해 completion이 오지 않는 경우에만 롤백한다 —
//          HasNoOutstandingIo()/RedisClient.h의 _pendingIoCount 설명 참고.
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

	if( bufferCount == 0 )
	{
		// 게시를 포기하는 경로에서도 owner를 정리해야 자기 참조가 남지 않는다.
		_recvEvent.owner = nullptr;
		return false;
	}

	DWORD dwBytesTransferred = 0;
	DWORD dwFlags = 0;

	_pendingIoCount.fetch_add(1, std::memory_order_seq_cst);

	if( ::WSARecv(hSocket, wsaBufs, bufferCount, &dwBytesTransferred, &dwFlags, &_recvEvent, NULL) == SOCKET_ERROR )
	{
		if( ::WSAGetLastError() != WSA_IO_PENDING )
		{
			_pendingIoCount.fetch_sub(1, std::memory_order_seq_cst); // 게시 실패 롤백 — completion이 안 옴
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
// @details [수정 -- 버그 수정: owner 누수] IocpEvent.h의 설계 의도대로라면
//          "I/O 완료 시 owner = nullptr"이어야 하는데, RecvEvent는 그 정리가
//          어디에도 없었고(RegisterRecv()가 성공할 때마다 owner를 다시
//          채우기만 함), SendEvent도 numOfBytes==0(연결 끊김) 경로에서는
//          정리가 빠져 있었다(성공 경로만 정리하고 있었음). 그 결과
//          _recvEvent.owner(=shared_from_this())가 CRedisClient 자기
//          자신을 계속 참조하는 순환이 되어, CRedisConnectionPool::Clear()가
//          _vecAllClients를 비워도 이 자기참조 때문에 객체가 소멸하지
//          않는 누수였다. 이제 이 함수가 호출됐다는 것 자체가 "이 이벤트에
//          걸려있던 I/O 완료가 지금 막 도착했다"는 뜻이므로, 그 즉시
//          owner를 정리해도 안전하다(같은 I/O 포스트에 대해 미래에 또
//          완료 통지가 올 일이 없다 -- 재등록하면 RegisterRecv()/DoSend()가
//          owner를 새로 채운다). Dispatch() 자체는 프레임워크 쪽에서
//          owner의 로컬 shared_ptr 사본을 쥔 채로 호출하는 구조이므로
//          (IocpEvent.h의 owner 설계 주석 참고), 이 함수 실행 도중에
//          owner를 비워도 this가 무효화되지 않는다 -- 기존 SendEvent
//          성공 경로가 이미 이 패턴을 쓰고 있었다.
// @details [수정 -- 버그 수정: Send 경로 락 누락] 외부 리뷰로 발견된 문제 --
//          RedisClient.h는 _commandLock이 _pendingSendBuffers/_sendOffset/
//          _sendTotalSize를 보호한다고 명시하는데, 실제로는 이 함수의
//          송신 완료 처리(부분 전송 이어 보내기 포함)가 그 필드들을 락
//          없이 직접 건드리고 있었다 -- Disconnect()가 같은 필드들을
//          _commandLock 안에서 리셋하는 것과 data race였다. 이제 송신
//          완료 처리 전체를 _commandLock으로 감싼다. Disconnect()가 이미
//          이 상태를 회수해간 경우(_pendingSendBuffers가 비어있음)는
//          조용히 무시한다 -- 늦게 도착한 완료 통지가 이미 정리된 상태를
//          다시 건드리지 않도록. DoSend() 실패 시에는 락을 놓은 뒤에
//          Disconnect()를 호출한다(Disconnect()가 같은 _commandLock을
//          다시 잡으므로, 락을 쥔 채로 부르면 재진입 데드락이다).
//***************************************************************************
void CRedisClient::Dispatch(CIocpEvent* iocpEvent, int32 numOfBytes)
{
	if( iocpEvent == &_recvEvent )
	{
		_recvEvent.owner = nullptr;

		if( numOfBytes == 0 )
		{
			// [수정 -- 버그 수정] 여기서 먼저 카운트를 감소시키지 않는다.
			// Disconnect()가 트리거하는 콜백 -> Pool -> ReconnectLoop() 경로가
			// 이 completion 처리가 다 끝나기 전에 재연결을 시작하지 못하도록,
			// Disconnect() 처리가 전부 끝난 뒤에야 outstanding에서 뺀다.
			Disconnect(ERedisDisconnectReason::ConnectionClosedRecv);
			_pendingIoCount.fetch_sub(1, std::memory_order_seq_cst);
			return;
		}

		// [수정 -- 버그 수정: 카운트 감소 시점] 외부 리뷰로 발견된 문제 --
		// 예전엔 ProcessRecv()를 부르기 *전에* 카운트를 감소시켰다. 그런데
		// ProcessRecv()는 끝나기 직전에 RegisterRecv()로 *같은* _recvEvent에
		// 대해 다음 WSARecv를 다시 게시한다 -- 즉 "이 completion이 완전히
		// 처리 끝났다(=_recvEvent를 재사용해도 안전하다)"는 시점은 사실
		// RegisterRecv()가 다시 +1 하는 그 순간까지다. 먼저 -1부터 해버리면,
		// ProcessRecv()가 아직 실행 중인(RegisterRecv() 호출 전) 그 좁은
		// 창에서 *다른* 이벤트(예: 같은 클라이언트의 send 쪽이 별도로 끊기는
		// 경우)로 인해 이 클라이언트가 격리 큐로 들어가면, ReconnectLoop()의
		// HasNoOutstandingIo() 체크가 "0"으로 오판해 Connect()를 걸 수
		// 있었다 -- 그 시점에 이 스레드는 곧 RegisterRecv()로 같은
		// _recvEvent를 또 쓰려는 참이라, 결국 막으려던 "같은 OVERLAPPED를
		// 두 연결이 동시에 씀" 경합이 재현될 수 있었다.
		//
		// 이제 ProcessRecv()가 완전히 리턴한 뒤에 -1 한다. ProcessRecv()
		// 안에서 RegisterRecv()가 성공하면 그 안에서 먼저 +1 하므로:
		//   기존 recv completion  : 1
		//   RegisterRecv() 성공    : +1 -> 2
		//   이 함수의 -1           : -1 -> 1
		// 순서가 되어 중간에 0이 되는 구간이 없다.
		ProcessRecv(numOfBytes);
		_pendingIoCount.fetch_sub(1, std::memory_order_seq_cst);
		return;
	}

	if( iocpEvent != &_sendEvent )
		return;

	if( numOfBytes == 0 )
	{
		_sendEvent.owner = nullptr;

		// [수정 -- 버그 수정] recv의 numOfBytes==0 경로와 동일한 이유로,
		// Disconnect()가 완전히 끝난 뒤에 카운트를 감소시킨다.
		Disconnect(ERedisDisconnectReason::ConnectionClosedSend);
		_pendingIoCount.fetch_sub(1, std::memory_order_seq_cst);
		return;
	}

	bool bSendFailed = false;
	{
		std::lock_guard<std::mutex> lock(_commandLock);

		// Disconnect()가 그 사이 이미 송신 상태를 회수해간 경우(예: 이
		// 완료 통지가 도착하기 직전 다른 스레드가 먼저 끊은 경우) -- 더
		// 이상 건드릴 상태가 없으므로 조용히 무시한다. 이 경우도 이번
		// completion 자체는 확실히 끝난 것이므로 카운트는 감소시킨다.
		if( _pendingSendBuffers.empty() || _sendTotalSize == 0 )
		{
			// 이 completion으로 송신 I/O가 완전히 끝났으므로 이벤트가 쥔 자기 참조(owner)와
			// 송신 버퍼 사본도 여기서 풀어준다.
			_sendEvent.owner = nullptr;
			_sendEvent.Reset();
			_pendingIoCount.fetch_sub(1, std::memory_order_seq_cst);
			return;
		}

		_sendOffset += static_cast<uint32>(numOfBytes);

		if( _sendOffset < _sendTotalSize )
		{
			// 부분 전송 -- WSASend 완료가 요청한 바이트를 전부 보냈다는
			// 보장은 없으므로, 아직 안 보낸 나머지를 이어서 전송한다.
			// DoSend()는 호출자가 _commandLock을 쥔 상태를 요구한다
			// (_pendingSendBuffers/_sendOffset/_sendTotalSize/_sendEvent를
			// 직접 참조하므로) -- 지금 이 지점이 바로 그 조건을 만족한다.
			// [순서 중요] 다음 WSASend(DoSend() 내부에서 먼저 +1)를 걸고
			// 나서 이번 completion의 -1을 수행한다 -- 그래야 두 completion
			// 사이에서 _pendingIoCount가 순간적으로 0이 되는 구간이 없다
			// (recv 쪽과 동일한 이유 -- 위 주석 참고).
			if( !DoSend() )
				bSendFailed = true;

			_pendingIoCount.fetch_sub(1, std::memory_order_seq_cst);
		}
		else
		{
			_sendEvent.owner = nullptr;
			_sendEvent.Reset();
			_pendingSendBuffers.clear();
			_sendOffset = 0;
			_sendTotalSize = 0;

			// SendCommand()가 이 송신 완료 전에 보관해 둔 다음 명령이 있으면 이어서 보낸다.
			// DoSend()가 먼저 카운트를 올린 뒤 이번 completion의 -1을 수행하므로 중간에 0이 되지 않는다.
			if( !_nextSendBuffers.empty() )
			{
				_pendingSendBuffers = std::move(_nextSendBuffers);
				_nextSendBuffers.clear();
				_sendOffset = 0;
				_sendTotalSize = _nextSendTotalSize;
				_nextSendTotalSize = 0;

				if( !DoSend() )
					bSendFailed = true;
			}

			_pendingIoCount.fetch_sub(1, std::memory_order_seq_cst);
		}
	}

	if( bSendFailed )
	{
		// _commandLock 밖에서 호출해야 한다 -- Disconnect()가 같은
		// _commandLock을 다시 잡으므로, 락을 쥔 채로 부르면 데드락이다.
		Disconnect(ERedisDisconnectReason::SendFailed);
	}
}

//***************************************************************************
// @brief 수신된 데이터를 파서에 넘기고, 완성된 RESP 값을 모두 꺼내 처리함
// @param dwBytesTransferred 수신받은 바이트 크기
// @details [수정] 파서 접근(_parser.Feed()/TryParse())을 _recvLock으로
//          감싼다 — Disconnect()의 _parser.Reset()과 동시에 실행될 수
//          있었던 data race를 막기 위함(RedisClient.h 상단 "파서 동시
//          접근" 설명 참고). 콜백(OnReceiptResponse())은 락 밖에서
//          실행한다 — 콜백 내부에서 이 객체를 재진입할 가능성을 배제할
//          수 없으므로, 파싱된 값들을 먼저 다 모아둔 뒤 락을 놓고 나서
//          하나씩 처리한다.
//***************************************************************************
void CRedisClient::ProcessRecv(DWORD dwBytesTransferred)
{
	// 링 버퍼 쓰기 커서 이동
	_recvBuffer.MoveWriteBuffer(dwBytesTransferred);

	std::vector<RedisValue> parsedValues;
	bool bProtocolError = false;
	{
		std::lock_guard<std::mutex> lock(_recvLock);

		// 수신 링 버퍼는 소켓에서 갓 도착한 바이트를 IOCP 완료 시점까지만
		// 보관하는 역할이다. 그 바이트를 파서에 Feed()로 넘기고 나면
		// 미완성 패킷 재조립 책임은 CRedisParser 하나로 완전히 넘어가므로,
		// 넘긴 만큼은 곧바로 읽기 커서를 이동시켜 같은 바이트가 다음 수신
		// 완료 때 다시 파서로 들어가는 일이 없도록 한다.
		// 링버퍼는 write < read인 wrap 상태에서 데이터가 [read, 버퍼끝) +
		// [버퍼시작, write) 두 개의 불연속 구간으로 나뉜다. GetReadBuffer()는
		// 읽기 커서 하나만 돌려주므로, 한 번에 GetSizeUsed()만큼 읽으면 첫
		// 구간의 끝을 넘어 버퍼 밖을 읽게 된다. CIocpSession::ProcessRecv()와
		// 동일하게 GetSizeDirectDequeueAble() 만큼의 연속 구간만 파서에 넘기고
		// 읽기 커서를 옮기는 과정을, 사용 중인 데이터가 없어질 때까지 반복한다
		// (wrap 상태면 두 번, 아니면 한 번 돈다). 파서가 미완성 조각을 자체
		// 버퍼(_pendingBuffer)에 보관하므로 링버퍼에는 처리 안 된 데이터가
		// 남지 않는다. Feed()가 상한 초과로 실패하면 스트림을 신뢰할 수 없으므로
		// 프로토콜 오류로 취급해 연결을 끊는다.
		while( true )
		{
			// 사용 중인 데이터를 다 소진하면 루프 종료
			if( _recvBuffer.GetSizeUsed() <= 0 )
				break;

			const int64 nDirectSize = _recvBuffer.GetSizeDirectDequeueAble();
			if( nDirectSize <= 0 )
				break;

			if( !_parser.Feed(_recvBuffer.GetReadBuffer(), static_cast<int32>(nDirectSize)) )
			{
				bProtocolError = true;
				break;
			}

			if( !_recvBuffer.MoveReadBuffer(nDirectSize) )
			{
				bProtocolError = true;
				break;
			}
		}

		if( !bProtocolError )
		{
			// [수정 — 버그 수정] TryParse()가 이제 bool이 아니라
			// ERedisParseResult를 반환한다 — 예전엔 "데이터 부족"과
			// "RESP 형식이 실제로 깨짐"을 구분 못 해서, 후자의 경우에도
			// 그냥 다음 수신을 기다리기만 하고 연결을 영원히 안 끊는
			// 문제가 있었다(RedisParser.h 상단 설명 참고). 이제
			// ProtocolError면 이 루프를 빠져나온 뒤 연결을 끊는다.
			RedisValue parsedVal;
			for( ;; )
			{
				const ERedisParseResult result = _parser.TryParse(parsedVal);
				if( result == ERedisParseResult::Success )
				{
					parsedValues.push_back(std::move(parsedVal));
					continue;
				}
				if( result == ERedisParseResult::ProtocolError )
					bProtocolError = true;
				break; // NeedMoreData(정상) 또는 ProtocolError — 어느 쪽이든 이번 처리는 끝
			}
		}
	}

	for( const RedisValue& val : parsedValues )
	{
		OnReceiptResponse(val);
	}

	if( bProtocolError )
	{
		// 이 경로는 TCP 연결이 끊긴 게 아니라 수신한 바이트를 RESP로 해석하지
		// 못한 경우다. 연결 끊김과 구분되도록 별도 사유(ProtocolError)를
		// 넘기면 호출부가 RedisValue::strVal로 원인을 구분할 수 있다.
		Disconnect(ERedisDisconnectReason::ProtocolError);
		return;
	}

	// 다음 수신을 등록하지 못하면 이 소켓은 더 이상 어떤 응답도 받을 수
	// 없다 — 대기 중인 콜백이 영영 완료되지 않고 매달리는 것을 막기 위해
	// 연결을 즉시 끊어 Disconnect()의 에러 완료 경로로 정리되게 한다.
	if( !RegisterRecv() )
	{
		Disconnect(ERedisDisconnectReason::RecvRegistrationFailed);
	}
}

//***************************************************************************
// @brief 파싱 완료된 응답을 대기 중인 콜백에 전달하여 실행함
// @param value 파싱 완료된 Redis 응답 데이터
// @details [수정] 큐가 아니라 단일 _pendingCallback을 _commandLock으로
//          보호된 상태에서 회수한다 — RedisClient.h 상단 설명 참고.
//***************************************************************************
void CRedisClient::OnReceiptResponse(const RedisValue& value)
{
	RedisCallback fnCallback;

	{
		std::lock_guard<std::mutex> lock(_commandLock);
		fnCallback = std::move(_pendingCallback);
		_pendingCallback = nullptr;
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

	// [수정] SendCommand()는 이제 반환값과 무관하게 콜백을 정확히 한 번
	// 호출한다(즉시 실패든, 나중에 응답으로든, Disconnect()로 인한
	// 에러로든) — 그래서 반환값을 보고 "콜백이 아예 안 불릴 테니 기다릴
	// 필요도 없다"고 조기 판단할 필요가 없다. 항상 완료를 기다리면
	// 충분하다.
	SendCommand(args, [pState](const RedisValue& res) {
		{
			std::lock_guard<std::mutex> lock(pState->lock);
			pState->bSuccess = (res.eType != ERedisType::Error);
			pState->bDone = true;
		}
		pState->cv.notify_all();
		});

	std::unique_lock<std::mutex> guard(pState->lock);
	pState->cv.wait_for(guard, std::chrono::milliseconds(nTimeoutMs), [pState] { return pState->bDone; });

	return pState->bDone && pState->bSuccess;
}