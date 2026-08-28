
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

//***************************************************************************
// @brief IOCP와 연동되는 비동기 소켓 단위의 Redis 클라이언트 개체
// @details CIocpObject를 상속받아 WSAAsync IO 통지를 수신하며,
//          비동기 송신 후 완료 시 등록된 콜백 큐를 순차적으로 실행시킵니다.
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
	virtual HANDLE GetHandle() override { return reinterpret_cast<HANDLE>(_socket); }

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

	bool        Connect(const std::string& strIP, const uint16 nPort);
	bool        Disconnect();

	//***************************************************************************
	// @brief 소켓 연결 여부를 반환함
	// @return 소켓 연결 여부 (true: 연결됨, false: 미연결)
	//***************************************************************************
	bool        IsConnected() const { return _socket != INVALID_SOCKET; }

	bool        SendCommand(const CVector<std::string>& vecArgs, RedisCallback fnCallback);


private:
	bool        RegisterRecv();
	void        ProcessRecv(DWORD dwBytesTransferred);
	void        OnReceiptResponse(const RedisValue& value);

private:
	SOCKET                  _socket;				// 소켓 핸들
	CIocpCoreRef            _iocpCore;				// IOCP 코어 객체 참조
	CRingBuffer             _recvBuffer;			// 수신 링 버퍼

	RecvEvent				_recvEvent;				// Recv Overlapped 이벤트
	SendEvent				_sendEvent;				// Send Overlapped 이벤트

	CRedisParser            _parser;				// RESP 파서
	std::mutex              _queueLock;				// 콜백 큐 동기화 락
	CQueue<RedisCallback>	_pendingCallbacks;		// 전송 대기 콜백 큐
};

#endif // ndef UC_REDISCLIENT_H