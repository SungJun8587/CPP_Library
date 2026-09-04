
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
//          비동기 송신 후 완료 시 등록된 콜백 큐를 순차적으로 실행시킵니다.
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
// @code
// // 사용 예시:
// auto pClient = std::make_shared<CRedisClient>(pIocpCore);
// if (pClient->Connect("127.0.0.1", 6379)) {
//     pClient->SendCommand({"GET", "MyKey"}, [](const RedisValue& res) {
//         // 수신 처리
//     });
// }
// @endcode
//***************************************************************************
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
	// @details _socket을 atomic exchange로 회수해, 이 함수가 여러 스레드에서
	//          동시에 호출되어도(예: recv/send 완료가 거의 동시에 numOfBytes==0으로
	//          도착한 경우) 실제 정리(소켓 close, 콜백 드레인, 파서 리셋)는
	//          그 exchange에서 "이긴" 단 하나의 호출만 수행하도록 보장한다.
	//          응답을 기다리고 있던 콜백들은 이 명령이 다시 완료될 일이 없으므로,
	//          에러 값으로 즉시 완료 처리하여 호출측(예: 커넥션 풀의 래핑 콜백)이
	//          커넥션 반납 등 후속 처리를 정상적으로 진행할 수 있게 한다.
	// @return 성공 여부 (true: 성공)
	//***************************************************************************
	bool        Disconnect();

	//***************************************************************************
	// @brief 소켓 연결 여부를 반환함
	// @return 소켓 연결 여부 (true: 연결됨, false: 미연결)
	//***************************************************************************
	bool        IsConnected() const { return _socket.load(std::memory_order_acquire) != INVALID_SOCKET; }

	bool        SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnCallback);


private:
	bool        RegisterRecv();
	void        ProcessRecv(DWORD dwBytesTransferred);
	void        OnReceiptResponse(const RedisValue& value);

	//***************************************************************************
	// @brief _pendingSendBuffer의 _sendOffset 위치부터 나머지를 WSASend로 등록함
	// @details 한 번의 WSASend 완료가 요청한 바이트를 전부 보냈다는 보장은
	//          없으므로(부분 전송), Dispatch()의 송신 완료 처리에서 아직 남은
	//          바이트가 있으면 이 함수를 다시 호출해 이어서 전송한다.
	// @return 등록 성공 여부 (true: 성공, false: 실패)
	//***************************************************************************
	bool        DoSend();

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
	bool        SelectDb(const int32 nDbIndex, const int32 nTimeoutMs);

private:
	std::atomic<SOCKET>     _socket;				// 소켓 핸들 (동시 Disconnect() 호출 시 정확히 한 번만 close되도록 atomic exchange로 관리)
	CIocpCoreRef            _iocpCore;				// IOCP 코어 객체 참조
	CRingBuffer             _recvBuffer;			// 수신 링 버퍼

	RecvEvent				_recvEvent;				// Recv Overlapped 이벤트
	SendEvent				_sendEvent;				// Send Overlapped 이벤트

	CSendBufferRef			_pendingSendBuffer;		// 현재 전송 중인 명령의 송신 버퍼 (부분 전송 이어 보내기용)
	uint32					_sendOffset = 0;		// _pendingSendBuffer 중 이미 보낸 바이트 수
	uint32					_sendTotalSize = 0;		// _pendingSendBuffer의 전체 바이트 수

	CRedisParser            _parser;				// RESP 파서 (미완성 패킷 재조립의 유일한 소유자)
	std::mutex              _queueLock;				// 콜백 큐 동기화 락
	CQueue<RedisCallback>	_pendingCallbacks;		// 전송 대기 콜백 큐
};

#endif // ndef UC_REDISCLIENT_H