
//***************************************************************************
// RedisClient.h : interface for the CRedisClient class.
//
//***************************************************************************

#ifndef UC_REDISCLIENT_H
#define UC_REDISCLIENT_H

#include <Network/IOCP/IocpCore.h>
#include <Network/RingBuffer.h>
#include <Redis/RedisParser.h>

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

//***************************************************************************
// @brief IOCP와 연동되는 비동기 소켓 단위의 Redis 클라이언트 개체
// @details CIocpObject를 상속받아 WSAAsync IO 통지를 수신하며,
//          비동기 송신 후 완료 시 등록된 콜백을 실행시킵니다.
//
// @details 스레드 계약: 한 시점에 이 객체에 대해 미완료 SendCommand()가
//          동시에 하나만 존재한다고 가정합니다(_sendEvent/전송 오프셋을
//          객체당 하나만 두고 재사용하기 때문). CRedisConnectionPool은
//          커넥션을 대여한 뒤 응답을 받을 때까지 다시 대여되지 않도록
//          보장하는 방식으로 이 계약을 지키고 있으므로, 풀을 거치지 않고
//          이 클래스를 직접 여러 스레드에서 동시에 호출하지 않도록 합니다.
//
//          다만 Disconnect()만은 예외적으로 여러 스레드에서 동시에 호출될
//          수 있다고 가정합니다 — _recvEvent와 _sendEvent가 서로 다른 IOCP
//          워커 스레드에서 완료 통지를 받을 수 있고, 연결이 끊기는 상황에서는
//          둘 다 거의 동시에 numOfBytes==0으로 완료되어 Dispatch()가 두
//          스레드에서 동시에 Disconnect()를 호출할 수 있기 때문입니다.
//          Disconnect()는 _socket을 atomic exchange로 원자적으로 회수해
//          이 경우에도 소켓이 정확히 한 번만 닫히도록 보장합니다.
//
//          [수정 — 버그 수정 2차: 콜백 0회 실행 경합] 외부 리뷰로 두 차례에
//          걸쳐 발견된 문제. 1차 수정(콜백 이중 실행/UB)은 SendCommand()의
//          DoSend() 실패 처리를 Disconnect()에 위임하는 것으로 막았지만,
//          그것만으로는 부족한 경합이 하나 더 있었다:
//            1) Thread A(SendCommand): "_socket이 유효한지 확인" — 통과.
//               아직 콜백을 큐에 넣기 전.
//            2) Thread B(Disconnect, 예: 다른 IOCP 완료가 numOfBytes==0을
//               통지): 소켓 exchange 성공 → 이 시점엔 콜백 큐가 비어있어
//               드레인할 게 없음 → 아무 콜백도 호출 안 하고 끝남.
//            3) Thread A: 이제서야 콜백을 큐에 push → DoSend() 호출 →
//               이미 끊긴 소켓이라 실패 → Disconnect() 호출하지만, 이미
//               B가 exchange를 "이겼으므로" 즉시 리턴, 드레인 안 함.
//            4) 결과: 방금 push한 콜백이 큐에 영원히 남아 절대 호출되지
//               않는다 — 호출부가 응답을 영원히 못 받고 매달리는 결과로
//               이어진다.
//
//          이 경합의 근본 원인은 "소켓 확인 → 콜백 등록"이라는 SendCommand()의
//          상태 전이와 "소켓 회수 → 콜백 드레인"이라는 Disconnect()의 상태
//          전이가 서로 다른 동기화 영역에 있었다는 점이다. 이제 _commandLock
//          하나로 이 둘을 같은 임계구역으로 묶는다 — Disconnect()가 콜백을
//          드레인하는 시점과 SendCommand()가 콜백을 등록하는 시점이 절대
//          겹치지 않으므로(항상 어느 한쪽이 먼저 끝나고 나서야 다른 쪽이
//          시작함), 콜백은 정확히 한 쪽에서만 발견되어 정확히 한 번
//          호출된다.
//
//          이 계약이 "한 시점에 미완료 SendCommand 하나뿐"이라는 원래
//          전제와도 맞아떨어지므로, 콜백 큐(CQueue<RedisCallback>)도 단일
//          필드(_pendingCallback)로 바꿨다 — 큐일 필요가 애초에 없었다.
//
//          [수정 — 버그 수정: 파서 동시 접근] Disconnect()의 _parser.Reset()과
//          ProcessRecv()의 _parser.Feed()/TryParse()가 서로 다른 IOCP 워커
//          스레드에서 동시에 실행될 수 있었다(_socket의 atomic exchange는
//          Disconnect() 호출자끼리의 중복 실행만 막아줄 뿐, Disconnect()와
//          ProcessRecv() 사이의 동시 접근까지는 막아주지 못했다) — CRedisParser
//          내부 상태(std::string)에 대한 data race였다. 이제 _recvLock으로
//          이 둘을 직렬화한다.
//***************************************************************************
//***************************************************************************
// @brief Disconnect()가 호출된 사유를 구분하는 열거형.
// @details 대기 중이던 콜백은 이 사유에 대응하는 에러 문자열(RedisValue::strVal)을
//          받는다. 진짜 TCP 연결 끊김과, 수신 데이터를 RESP로 해석하지 못해
//          스스로 연결을 끊은 경우(ProtocolError)를 호출부가 구분할 수 있도록
//          사유를 나눴다. 문자열은 CRedisClient::DisconnectReasonToString()
//          한 곳에서만 만든다.
//***************************************************************************
enum class ERedisDisconnectReason
{
	ConnectionClosedRecv,		// recv completion이 0바이트로 도착 — 진짜 TCP 연결 끊김
	ConnectionClosedSend,		// send completion이 0바이트로 도착 — 진짜 TCP 연결 끊김
	SendFailed,					// WSASend() 게시 자체가 실패
	ProtocolError,				// RESP 파싱 실패 — 네트워크 문제가 아니라 수신 데이터 해석 실패
	RecvRegistrationFailed,		// 다음 WSARecv 재등록(RegisterRecv()) 실패
	Generic						// 그 외 일반적인 종료(소멸자, Connect() 실패 등 — 콜백이 없거나 사유 구분이 중요하지 않은 경우)
};

class CRedisClient : public CIocpObject, public std::enable_shared_from_this<CRedisClient>
{
public:
	CRedisClient(CIocpCoreRef iocpCore);
	virtual ~CRedisClient();

	//***************************************************************************
	// CIocpObject 순수 가상 함수 구현
	//***************************************************************************

	//***************************************************************************
	// @brief IOCP 바인딩용 소켓 핸들을 반환함
	// @return 소켓 핸들 포인터
	//***************************************************************************
	virtual HANDLE GetHandle() override { return reinterpret_cast<HANDLE>(_socket.load(std::memory_order_acquire)); }

	//***************************************************************************
	// @brief 자기 자신의 shared_ptr(CIocpObjectRef)을 반환합니다.
	// @details Aliasing Constructor를 활용하여 CRedisClient의 제어 블록 수명을 공유하면서,
	//          CIocpObject* 타입 포인터를 들고 있는 CIocpObjectRef를 안전하고 빠르게 생성합니다.
	// @return CIocpObjectRef 스마트 포인터
	//***************************************************************************
	virtual CIocpObjectRef GetIocpObjectPtr() override
	{
		return CIocpObjectRef(shared_from_this(), static_cast<CIocpObject*>(this));
	}

	virtual void Dispatch(class CIocpEvent* iocpEvent, int32 numOfBytes = 0) override;

	//***************************************************************************
	// @brief Redis 서버에 TCP 소켓 연결을 수행하고, IOCP 등록 및 논리 DB 선택까지 마침
	// @param strIP 서버 IP 주소
	// @param nPort 서버 포트 번호
	// @param nDbIndex 접속 직후 SELECT로 고정할 Redis 논리 DB 인덱스. 0(기본 DB)이면
	//        SELECT를 생략해 접속 지연을 줄인다. 재연결 시에도 동일 인자로 다시
	//        호출되므로, 재연결된 커넥션도 항상 같은 DB를 바라보게 된다
	// @param nConnectTimeoutMs connect() 완료 및 (nDbIndex > 0인 경우) SELECT 응답을
	//        기다릴 최대 시간(ms). connect() 자체는 Overlapped I/O 대상이 아닌 동기
	//        호출이므로, 소켓을 일시적으로 논블로킹으로 전환해 이 시간만큼만 대기한
	//        뒤 결과를 확인한다. 이후의 실제 송수신은 WSASend/WSARecv 기반
	//        Overlapped I/O로 처리되므로 이 전환은 connect 단계에만 영향을 준다
	// @return 성공 여부 (true: 성공, false: 실패 또는 타임아웃)
	//***************************************************************************
	bool        Connect(const std::string& strIP, const uint16 nPort, const int32 nDbIndex = 0, const int32 nConnectTimeoutMs = 3000);

	//***************************************************************************
	// @brief 소켓 연결을 종료하고 대기 중인 콜백 및 리소스를 정리함
	// @param reason 대기 중인 콜백에 에러 값으로 전달할 사유(클래스 상단
	//        ERedisDisconnectReason 설명 참고).
	// @details _socket을 atomic exchange로 회수해, 이 함수가 여러 스레드에서
	//          동시에 호출되어도(예: recv/send 완료가 거의 동시에 numOfBytes==0으로
	//          도착한 경우) 실제 정리(소켓 close, 콜백 드레인, 파서 리셋)는
	//          그 exchange에서 "이긴" 단 하나의 호출만 수행하도록 보장한다.
	//          응답을 기다리고 있던 콜백은 이 명령이 다시 완료될 일이 없으므로,
	//          에러 값으로 즉시 완료 처리하여 호출측(예: 커넥션 풀의 래핑 콜백)이
	//          커넥션 반납 등 후속 처리를 정상적으로 진행할 수 있게 한다.
	//          [주의] 이 함수가 실제로 소켓을 회수해 정리를 수행하는 건
	//          _socket exchange에서 "이긴" 단 한 번뿐이다 — 동시에 호출된
	//          다른 스레드는 이미 INVALID_SOCKET을 보고 조용히 반환하므로,
	//          그쪽 호출부가 넘긴 reason은 쓰이지 않을 수 있다(정상 —
	//          콜백은 어차피 정확히 한 번만 불려야 하므로, 먼저 회수에
	//          성공한 쪽의 사유가 채택된다).
	// @return 성공 여부 (true: 성공)
	//***************************************************************************
	bool        Disconnect(ERedisDisconnectReason reason = ERedisDisconnectReason::Generic);

	//***************************************************************************
	// @brief 소켓 연결 여부를 반환함
	// @return 소켓 연결 여부 (true: 연결됨, false: 미연결)
	//***************************************************************************
	bool        IsConnected() const { return _socket.load(std::memory_order_acquire) != INVALID_SOCKET; }

	//***************************************************************************
	// @brief 명령어를 RESP 변환하여 비동기 전송하고 콜백을 등록함.
	// @details 반환값과 무관하게 fnCallback은 정확히 한 번 호출됨이 보장된다 —
	//          클래스 문서 상단의 "버그 수정" 설명 참고. 반환값은 "즉시
	//          전송까지 성공적으로 갔는지"를 알려주는 참고 정보일 뿐, 호출부가
	//          실패 시 별도로 콜백을 또 불러줄 필요는 없다.
	// @return 즉시 전송 등록 성공 여부 (true: 성공, false: 실패 — 이 경우에도
	//         fnCallback은 이미 또는 곧 에러 값으로 호출된다)
	//***************************************************************************
	//***************************************************************************
	// @brief [추가] 이 클라이언트에 지금 걸려있는 모든 비동기 I/O(WSARecv +
	//        WSASend)의 완료 통지를 전부 받았는지 여부를 반환합니다.
	// @details CRedisConnectionPool::ReconnectLoop()가 이 값을 확인해서,
	//          true일 때만 Connect()를 다시 호출해도 안전하다고 판단합니다 —
	//          아래 _pendingIoCount 설명 참고. Disconnect() 직후 이 값이
	//          아직 0이 아니라면(소켓은 닫혔지만 그 소켓에 걸려있던
	//          WSARecv/WSASend의 완료 통지가 아직 IOCP 큐를 다 빠져나오지
	//          않은 상태), 재연결로 _recvEvent/_sendEvent를 재사용하는 건
	//          안전하지 않습니다.
	//***************************************************************************
	bool        HasNoOutstandingIo() const { return _pendingIoCount.load(std::memory_order_seq_cst) == 0; }

	bool        SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnCallback);


private:
	bool        RegisterRecv();
	void        ProcessRecv(DWORD dwBytesTransferred);
	void        OnReceiptResponse(const RedisValue& value);

	//***************************************************************************
	// @brief _pendingSendBuffers의 _sendOffset 위치부터 나머지를 WSASend(Scatter-Gather)로 등록함
	// @details 한 번의 WSASend 완료가 요청한 바이트를 전부 보냈다는 보장은
	//          없으므로(부분 전송), Dispatch()의 송신 완료 처리에서 아직 남은
	//          바이트가 있으면 이 함수를 다시 호출해 이어서 전송한다.
	// @details [추가] 호출자는 반드시 _commandLock을 획득한 상태에서
	//          호출해야 한다 — _pendingSendBuffers/_sendOffset/_sendTotalSize
	//          및 _sendEvent를 직접 참조/수정하기 때문이다. 현재 호출부는
	//          SendCommand()(자체적으로 _commandLock을 쥐고 호출)와
	//          Dispatch()의 송신 완료 처리(마찬가지로 _commandLock을 쥔
	//          채로 호출) 두 곳뿐이며, 둘 다 이 규칙을 지킨다.
	// @return 등록 성공 여부 (true: 성공, false: 실패)
	//***************************************************************************
	bool        DoSend();

	//***************************************************************************
	// @brief [추가] 소켓 close와 파서 리셋을 함께 수행하는 내부 헬퍼.
	// @details Disconnect()와 SendCommand()의 DoSend() 실패 처리(재진입 없이
	//          직접 처리하는 경로) 양쪽에서 공유한다. _commandLock은 잡지
	//          않는다(호출부가 각자의 문맥에서 이미 처리했거나 처리할
	//          것이므로) — 오직 소켓 close와 _recvLock으로 보호되는 파서
	//          리셋만 담당한다.
	// @param hOldSocket 이미 atomic exchange로 회수한 소켓 핸들
	//        (INVALID_SOCKET이면 아무 것도 하지 않는다 — 이미 다른 경로가
	//        처리했다는 뜻).
	//***************************************************************************
	void        CleanupSocketAndParser(SOCKET hOldSocket);

	//***************************************************************************
	// @brief 접속 직후 이 커넥션이 사용할 논리 DB를 SELECT 명령으로 고정함
	// @details SendCommand()는 비동기이므로, Connect()가 "연결 및 DB 선택까지
	//          끝난 뒤"의 결과를 그대로 돌려줄 수 있도록 내부적으로
	//          condition_variable로 응답을 기다려 동기 호출처럼 동작시킨다.
	//          IOCP 완료 통지(Dispatch)는 별도의 IOCP 워커 스레드에서 오므로,
	//          이 대기가 스스로를 블로킹하는 데드락은 발생하지 않는다.
	// @param nDbIndex 선택할 Redis 논리 DB 인덱스
	// @param nTimeoutMs 응답을 기다릴 최대 시간(ms)
	// @return 성공 여부 (true: SELECT 성공, false: 실패 또는 타임아웃)
	//***************************************************************************
	//***************************************************************************
	// @brief [추가] ERedisDisconnectReason을 콜백에 전달할 RedisValue::strVal
	//        문자열로 변환함.
	// @details Disconnect()가 이 함수 하나만 거쳐서 문자열을 만들도록
	//          일원화한다 — 문구를 바꿀 일이 생겨도 여기 한 곳만 고치면
	//          되고, 호출부마다 문자열을 직접 만들지 않으므로 오타/불일치
	//          여지가 없다.
	//***************************************************************************
	static const char* DisconnectReasonToString(ERedisDisconnectReason reason);

	bool        SelectDb(const int32 nDbIndex, const int32 nTimeoutMs);

private:
	std::atomic<SOCKET>     _socket;				// 소켓 핸들 (동시 Disconnect() 호출 시 정확히 한 번만 close되도록 atomic exchange로 관리)
	CIocpCoreRef            _iocpCore;				// IOCP 코어 객체 참조
	CRingBuffer             _recvBuffer;			// 수신 링 버퍼

	// [추가 — 버그 수정: 재연결 시 stale completion] outstanding WSARecv+
	// WSASend 완료 대기 수. CIocpSession의 _pendingIoCount와 동일한 패턴 —
	// closesocket() 이후에도 그 소켓에 걸려있던 I/O의 완료 통지가 IOCP
	// 큐에 남아있을 수 있는데, 그 상태에서 재연결이 같은 _recvEvent/
	// _sendEvent(OVERLAPPED 메모리)를 재사용해버리면 나중에 도착하는 stale
	// completion이 새 연결의 것으로 오인되어 처리될 위험이 있었다(외부
	// 리뷰로 발견 — RedisConnectionPool.cpp의 ReconnectLoop() 설명 참고).
	// RegisterRecv()/DoSend()가 실제 게시 직전에 +1, Dispatch()가 그
	// 완료를 처리할 때 -1 한다 — 이 값이 0이어야만 이전 연결의 I/O가
	// 전부 커널 레벨에서 끝났다고 볼 수 있다.
	std::atomic<int32>      _pendingIoCount{ 0 };

	RecvEvent				_recvEvent;				// Recv Overlapped 이벤트
	SendEvent				_sendEvent;				// Send Overlapped 이벤트

	// [수정] _commandLock 하나가 "소켓 확인 → 콜백 등록 → 송신 상태 설정"
	// (SendCommand)과 "콜백 회수"(Disconnect)를 같은 임계구역으로 묶는다 —
	// 클래스 문서 상단의 "콜백 0회 실행 경합" 설명 참고. 큐가 아니라 단일
	// 필드인 이유도 같은 설명 참고(한 시점에 미완료 SendCommand는 하나뿐).
	std::mutex              _commandLock;			// _pendingCallback/_pendingSendBuffers/_sendOffset/_sendTotalSize 동기화
	RedisCallback			_pendingCallback;		// 응답을 기다리는 중인 콜백 (한 시점에 최대 1개)

	CVector<CSendBufferRef>	_pendingSendBuffers;	// 현재 전송 중인 명령의 송신 버퍼들 (순서대로 이어 붙인 전체가 명령 하나, 부분 전송 이어 보내기용) — _commandLock으로 보호
	uint32					_sendOffset = 0;		// _pendingSendBuffers 전체(명령 하나) 중 이미 보낸 바이트 수 — _commandLock으로 보호
	uint32					_sendTotalSize = 0;		// _pendingSendBuffers의 전체 바이트 수 — _commandLock으로 보호

	// [수정] _recvLock으로 파서 접근(ProcessRecv())과 파서 리셋(Disconnect())을
	// 직렬화한다 — 클래스 문서 상단의 "파서 동시 접근" 설명 참고.
	std::mutex				_recvLock;				// _parser 동기화
	CRedisParser            _parser;				// RESP 파서 (미완성 패킷 재조립의 유일한 소유자) — _recvLock으로 보호
};

#endif // ndef UC_REDISCLIENT_H