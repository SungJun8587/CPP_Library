
//***************************************************************************
// RedisDefine.h: Redis 모듈의 열거형, 상수, 에러 메시지를 모아둔 헤더.
//
//***************************************************************************

#ifndef UC_REDISDEFINE_H
#define UC_REDISDEFINE_H

#include <BaseRedefineDataType.h>

//***************************************************************************
// @brief RESP 프로토콜의 데이터 타입을 나타내는 열거형
// @details Redis 서버와 통신할 때 사용하는 5가지 기본 RESP 데이터 타입을 정의합니다.
//***************************************************************************
enum class ERedisType
{
	Unknown,
	SimpleString,  // +
	Error,         // -
	Integer,       // :
	BulkString,    // $
	Array          // *
};

//***************************************************************************
enum class ERedisParseResult
{
	Success,		// RESP 값 하나를 완전히 파싱함
	NeedMoreData,	// 데이터가 부족하여 추가 수신이 필요함 — 정상적인 상황
	ProtocolError	// RESP 형식이 잘못됨 — 호출부는 이 연결을 끊어야 한다
};

//***************************************************************************
enum class ERedisDisconnectReason
{
	ConnectionClosedRecv,		// recv completion이 0바이트로 도착 — 진짜 TCP 연결 끊김
	ConnectionClosedSend,		// send completion이 0바이트로 도착 — 진짜 TCP 연결 끊김
	SendFailed,					// WSASend() 게시 자체가 실패
	ProtocolError,				// RESP 파싱 실패 — 네트워크 문제가 아니라 수신 데이터 해석 실패
	RecvRegistrationFailed,		// 다음 WSARecv 재등록(RegisterRecv()) 실패
	CommandTimeout,				// 명령 응답이 제한 시간 안에 오지 않아 풀이 연결을 끊음(서버 정지/반열림 연결)
	Generic						// 그 외 일반적인 종료(소멸자, Connect() 실패 등 — 콜백이 없거나 사유 구분이 중요하지 않은 경우)
};


namespace Redis
{
	//***************************************************************************
	// 파서 한계값 (CRedisParser)
	//***************************************************************************

	// 미완성 RESP 스트림의 최대 누적 크기. 프로젝트에서 다루는 Redis 응답의
	// 최대 크기(예: 대화 기록 HMGET 응답 등)에 맞춰 조정 가능.
	inline constexpr size_t kMaxPendingBufferSize = 16 * 1024 * 1024;

	// 하나의 배열이 가질 수 있는 최대 원소 수 — 악의적/손상된 스트림의 대량 reserve() 시도를 막는다.
	inline constexpr int64 kMaxArrayElements = 4096;

	// 중첩 배열의 최대 재귀 깊이 — 스택 오버플로 방지.
	inline constexpr int32 kMaxParseDepth = 64;

	//***************************************************************************
	// 클라이언트 (CRedisClient)
	//***************************************************************************

	// 수신 링 버퍼 크기. 한 번의 WSARecv가 받는 최대 바이트이므로, 작으면 큰 응답(대화 기록 등)이
	// 수십~수백 번의 완료 통지와 파서 호출로 쪼개진다.
	inline constexpr int64 kRecvBufferSize = 16 * 1024;

	// 대형 명령을 한 번 인코딩할 때 쓰는 스레드 로컬 임시 버퍼가 이 크기를 넘게 커지면 사용 후 해제한다.
	inline constexpr size_t kScratchRetainLimit = 1u << 20;

	// 명령 하나(RESP로 인코딩한 전체 크기)의 상한. Redis 서버의 인자당 기본 상한(proto-max-bulk-len, 512MB)에 맞춘다.
	// CRedisCommandBuilder::CalcEncodedSize()가 이 값을 넘으면 0을 돌려주고, 클라이언트는 전송하지 않는다.
	inline constexpr size_t kMaxCommandSize = 512u * 1024 * 1024;

	// TCP keepalive: 유휴 시간(첫 프로브까지)과 프로브 재전송 간격. Windows 기본값(2시간)으로는 방화벽/NAT가
	// 조용히 버린 유휴 연결을 제때 발견하지 못한다.
	inline constexpr uint32 kKeepAliveIdleMs = 30000;
	inline constexpr uint32 kKeepAliveIntervalMs = 10000;

	// 접속(이름 해석 포함) 기본 제한 시간.
	inline constexpr int32 kConnectTimeoutMs = 3000;

	// 동시에 살아 있을 수 있는 이름 해석(getaddrinfo) 도우미 스레드 수의 프로세스 전체 상한.
	inline constexpr int32 kMaxActiveResolvers = 4;

	//***************************************************************************
	// 커넥션 풀 (CRedisConnectionPool)
	//***************************************************************************

	// 재연결 점검 주기 기본값(초).
	inline constexpr int32 kDefaultReconnectIntervalSec = 3;

	// 대기 큐에 쌓을 수 있는 요청 상한. 넘으면 새 요청은 거부된다.
	inline constexpr size_t kMaxPendingCommands = 2000;

	// 대기 큐에서 이 시간을 넘긴 요청은 연결 오류로 끝낸다(점검 주기 단위로 확인하므로 실제로는 최대 한 주기 더 걸릴 수 있다).
	inline constexpr int64 kPendingTimeoutMs = 5000;

	// 명령을 보낸 뒤 이 시간 안에 응답이 없으면 커넥션을 끊는다. 풀 경로에는 KEYS/긴 스크립트처럼
	// 수 초를 넘기는 명령을 쓰지 않는다는 전제의 넉넉한 값이다.
	inline constexpr int64 kPoolCommandTimeoutMs = 10000;

	// Init()과 재연결 스윕이 커넥션을 동시에 맺는 최대 워커 수.
	inline constexpr int32 kMaxConnectWorkers = 16;

	// 재연결 스레드가 "이전 연결의 I/O 완료 통지가 아직 남아 있어" 건너뛴 클라이언트를 다시 점검하기까지의 짧은 대기(ms).
	// 연결이 끊긴 직후에는 완료 통지가 곧 비워지므로, 일반 점검 주기(수 초)를 기다리지 않고 빠르게 다시 본다.
	inline constexpr int32 kReconnectRecheckMs = 100;

	//***************************************************************************
	// 재연결 지수 백오프 (풀과 구독 연결 공용)
	// 이 값들은 "이 시각이 지나기 전엔 재시도 자체를 안 함"이라는 최소 대기 시간일 뿐이다 —
	// 실제 재시도 시점은 재연결 스윕 주기에 맞춰서만 확인된다.
	//***************************************************************************
	inline constexpr int64 kReconnectBackoffBaseMs = 500;		// 최초 재시도 최소 대기 시간
	inline constexpr int64 kReconnectBackoffMaxMs = 30000;		// 재시도 최소 대기 시간 상한선
	inline constexpr int32 kReconnectBackoffMaxShift = 6;		// 백오프 지수 증가 횟수 상한
	inline constexpr int32 kReconnectBackoffJitterMs = 250;		// 동시 재시도 충돌 방지를 위한 지터 최대값(풀)

	//***************************************************************************
	// 구독 연결 (CRedisSubscriber)
	//***************************************************************************
	inline constexpr int32 kSubscriberReconnectIntervalSec = 1;	// 구독 연결의 재연결·생존 점검 주기(초)
	inline constexpr int64 kSubscriberPingIntervalMs = 5000;	// 유휴 연결에 PING을 보내는 간격
	inline constexpr int64 kSubscriberCommandTimeoutMs = 5000;	// 진행 중인 명령(PING 포함)이 무응답일 때 연결을 끊는 한도

	//***************************************************************************
	// 클라이언트가 직접 만드는 에러 응답 문자열
	// IsRedisConnectionError()는 kErrConnectionClosed/kErrSendFailed/kErrRecvRegistrationFailed/
	// kErrProtocolError로 시작하는 에러를 "연결 문제"로 본다(재시도 대상).
	//***************************************************************************
	inline constexpr const char* kErrConnectionClosed = "ERR connection closed";
	inline constexpr const char* kErrConnectionClosedRecv = "ERR connection closed (recv 0 bytes)";
	inline constexpr const char* kErrConnectionClosedSend = "ERR connection closed (send 0 bytes)";
	inline constexpr const char* kErrCommandTimeout = "ERR connection closed (command timeout)";
	inline constexpr const char* kErrPoolCleared = "ERR connection closed (pool cleared)";
	inline constexpr const char* kErrPendingTimeout = "ERR connection closed (pending timeout)";
	inline constexpr const char* kErrSendFailed = "ERR send failed";
	inline constexpr const char* kErrRecvRegistrationFailed = "ERR recv registration failed";
	inline constexpr const char* kErrProtocolError = "ERR protocol error";
	inline constexpr const char* kErrCommandInFlight = "ERR command already in flight";
	inline constexpr const char* kErrSendRejected = "SendCommand rejected";	// 풀 없음/대기 큐 포화로 전송이 거절됨
	inline constexpr const char* kErrSendBufferAllocation = "ERR send buffer allocation failed";
	inline constexpr const char* kErrCommandTooLarge = "ERR command too large";
}

#endif // ndef UC_REDISDEFINE_H