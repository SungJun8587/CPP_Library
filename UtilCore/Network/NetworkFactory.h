
//***************************************************************************
// NetworkFactory.h : interface for the CNetworkFactory class.
//
//***************************************************************************

#ifndef __NETWORKFACTORY_H__
#define __NETWORKFACTORY_H__

#include <memory>
#include <functional>

//***************************************************************************
// @enum ENetworkEngineType
// @brief 네트워크 엔진 유형을 정의하는 열거형
//***************************************************************************
enum class ENetworkEngineType
{
	IOCP, // Windows IOCP (Input/Output Completion Port) 엔진
	RIO   // Windows RIO (Registered I/O) 엔진
};

class CNetService;
class CSession;
class CNetAddress;

using SessionFactory = std::function<CSessionRef()>;

//***************************************************************************
// [워커 스레드 개수(workerThreadCount) 설정 가이드]
//
// workerThreadCount == 0 이면 각 엔진이 자체 정책으로 자동 산정합니다.
// 자동 산정 공식은 서버 기준값이며 엔진별로 다릅니다.
//
// ── 서버(CreateServerService) ──
//   - IOCP: 0 → hardware_concurrency()(조회 실패 시 2), 즉 논리 코어 수만큼.
//   - RIO : 0 → max(1, hardware_concurrency()/2), 즉 논리 코어 수의 절반만.
//     (RIO는 Recv/Send CQ 락 분리 + 가벼운 완료 처리로 워커 1개당
//      처리량이 더 높다는 전제의 설계값)
//   - 0(자동) 대신 직접 지정할 때: 부하 테스트로 워커 수를 통제 변수로
//     둘 때, 다른 서비스와 코어를 나눠 써야 할 때, 서버+클라이언트를
//     같은 프로세스에서 같이 띄워 자동 산정치 합이 코어 수를 넘길 때.
//
// ── 클라이언트(CreateClientService) ──
//   - 서버용 자동값(코어 수 또는 그 절반)을 그대로 쓰면 세션 수가
//     maxSessionCount로 이미 제한된 클라이언트엔 워커가 과도하게 뜨고,
//     같은 CQ/IOCP 포트를 두고 워커끼리 경쟁만 하게 됨(RIO는
//     _recvCqMutex/_sendCqMutex 경합까지 추가) — 0 대신 명시 지정 권장.
//   - 일반적인 단일/소수 세션 클라이언트: 1(많아야 2).
//   - maxSessionCount가 큰 부하생성기 클라이언트: 실측 프로파일링으로
//     결정(임의로 늘리면 남는 코어가 없어 손해).
//   - 이런 클라이언트를 여러 인스턴스 동시에 띄우는 부하테스트는 특히
//     주의: 인스턴스마다 0(자동)을 쓰면 스레드 수가 인스턴스 개수만큼
//     곱해져 폭증함 — 인스턴스당 고정 소수값을 반드시 명시할 것.
//
// 공통: 과도하게 많은 워커는 처리량 향상 없이 컨텍스트 스위칭 비용만
// 늘릴 수 있으므로, 자동값보다 늘릴 때는 실측 근거를 확인할 것.
//***************************************************************************

//***************************************************************************
// @class CNetworkFactory
// @brief 런타임에 IOCP 또는 RIO 네트워크 서비스 및 코어를 생성하는 팩토리 클래스
// @details
// 역할:
//      1. 설정된 네트워크 엔진 타입(IOCP/RIO)에 따라 알맞은 서버 서비스 객체 생성
//      2. 설정된 네트워크 엔진 타입(IOCP/RIO)에 따라 알맞은 클라이언트 서비스 객체 생성
//***************************************************************************
class CNetworkFactory
{
public:
	//***************************************************************************
	// @brief 엔진 유형에 따라 서버 서비스 객체를 생성합니다.
	// @param engineType 생성할 네트워크 엔진 유형 (IOCP 또는 RIO)
	// @param address 서버가 바인딩할 네트워크 주소 (IP/Port)
	// @param factory 세션 객체를 생성하는 팩토리 함수
	// @param maxSessionCount 최대 허용 세션 수 (기본값: 1)
	// @param workerThreadCount 워커 스레드 개수 (기본값: 0=자동, 엔진별 산정 공식이
	//        다름 — 클래스 위 "[워커 스레드 개수 설정 가이드]" 참고)
	// @param engineCoreRef 내부 네트워크 코어 참조 포인터 (CIocpCore* 또는 CRioCore*)
	// @return std::shared_ptr<CNetService> 생성된 서버 네트워크 서비스 스마트 포인터
	//***************************************************************************
	static CNetServiceRef CreateServerService(
		ENetworkEngineType engineType,
		CNetAddress address,
		SessionFactory factory,
		int32 maxSessionCount = 1,
		uint32_t workerThreadCount = 0,
		void* engineCoreRef = nullptr
	);

	//***************************************************************************
	// @brief 엔진 유형에 따라 클라이언트 서비스 객체를 생성합니다.
	// @param engineType 생성할 네트워크 엔진 유형 (IOCP 또는 RIO)
	// @param address 접속할 서버의 네트워크 주소 (IP/Port)
	// @param factory 세션 객체를 생성하는 팩토리 함수
	// @param maxSessionCount 생성할 클라이언트 세션 수 (기본값: 1)
	// @param workerThreadCount 워커 스레드 개수 (기본값: 0=자동, 엔진별 산정 공식이
	//        다름 — 클래스 위 "[워커 스레드 개수 설정 가이드]" 참고)
	// @param engineCoreRef 내부 네트워크 코어 참조 포인터 (CIocpCore* 또는 CRioCore*)
	// @return std::shared_ptr<CNetService> 생성된 클라이언트 네트워크 서비스 스마트 포인터
	//***************************************************************************
	static CNetServiceRef CreateClientService(
		ENetworkEngineType engineType,
		CNetAddress address,
		SessionFactory factory,
		int32 maxSessionCount = 1,
		uint32_t workerThreadCount = 0,
		void* engineCoreRef = nullptr
	);
};

#endif // ndef __NETWORKFACTORY_H__