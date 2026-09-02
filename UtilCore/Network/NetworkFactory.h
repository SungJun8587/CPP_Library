
//***************************************************************************
// NetworkFactory.h : interface for the CNetworkFactory class.
//
//***************************************************************************

#ifndef UC_NETWORKFACTORY_H
#define UC_NETWORKFACTORY_H

#include <Network/Session.h>						// SessionFactory 정의 (CSessionRef를 쓰는 std::function 별칭)
#include <Network/NetworkRedefineDataType.h>		// CNetServiceRef/CIocpCoreRef/CRioCoreRef 등 네트워크 계층 전체가 공유하는 shared_ptr 별칭 레지스트리

#include <memory>
#include <functional>
#include <variant>
#include <type_traits>

//***************************************************************************
// @enum ENetworkEngineType
// @brief 클라이언트/테스트 코드에서 사용할 네트워크 엔진 종류를 선택하는 열거형.
//***************************************************************************
enum class ENetworkEngineType
{
	IOCP,	// IOCP(I/O Completion Port) 기반 엔진
	RIO,	// RIO(Registered I/O) 기반 엔진
};

class CNetAddress;

//***************************************************************************
// @typedef EngineCoreRef
// @brief 서버/클라이언트 서비스를 생성할 때 넘길 엔진 코어 참조.
//
// @details
//      [재설계 — void* + ENetworkEngineType 조합에서 std::variant로 전환]
//      예전 버전은 `ENetworkEngineType engineType` 파라미터와
//      `void* engineCoreRef` 파라미터를 따로 받아, 호출부가 engineType에
//      맞는 실제 타입(CIocpCoreRef* 또는 CRioCoreRef*)으로 직접 캐스팅해서
//      넘겨야 했다. 이 둘은 서로 다른 소스(하나는 enum 값, 하나는 임의의
//      포인터)라 컴파일러가 둘의 일치를 전혀 검증할 수 없었고, 실제로 이
//      불일치 때문에 switch에 break가 빠지면 엉뚱한 타입으로 static_cast하는
//      버그가 있었다(진단 이력 참고).
//
//      std::variant<CIocpCoreRef, CRioCoreRef>로 통합하면 "어떤 엔진인지"와
//      "그 엔진의 코어가 무엇인지"가 하나의 값으로 합쳐져, 애초에 서로
//      어긋난 조합을 만들 수 없다 — 호출부는 IOCP 코어를 쓰려면
//      CIocpCoreRef를 그대로 담아 넘기면 되고, 어떤 엔진인지는 이 함수들
//      내부에서 std::visit()로 컴파일 타임에 분기한다. 별도의
//      ENetworkEngineType 인자는 이제 불필요해 파라미터 목록에서 제거했다
//      (variant의 활성 대안(alternative) 자체가 엔진 타입의 유일한
//      출처이므로, 두 값이 어긋날 여지가 없다).
//***************************************************************************
using EngineCoreRef = std::variant<CIocpCoreRef, CRioCoreRef>;

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
//      1. 전달받은 EngineCoreRef(IOCP/RIO 코어 중 하나를 담은 variant)에 맞는
//         서버 서비스 객체 생성
//      2. 전달받은 EngineCoreRef에 맞는 클라이언트 서비스 객체 생성
//***************************************************************************
class CNetworkFactory
{
public:
	//***************************************************************************
	// @brief 엔진 코어에 따라 서버 서비스 객체를 생성합니다.
	// @param engineCore IOCP 또는 RIO 코어 참조(둘 중 하나를 담은 variant).
	//        어느 쪽이 담겨 있는지가 곧 생성할 엔진 타입입니다 — 별도의
	//        엔진 타입 인자가 없습니다(불일치 가능성 자체를 제거).
	// @param address 서버가 바인딩할 네트워크 주소 (IP/Port)
	// @param factory 세션 객체를 생성하는 팩토리 함수
	// @param maxSessionCount 최대 허용 세션 수 (기본값: 1)
	// @param workerThreadCount 워커 스레드 개수 (기본값: 0=자동, 엔진별 산정 공식이
	//        다름 — 클래스 위 "[워커 스레드 개수 설정 가이드]" 참고)
	// @return CNetServiceRef 생성된 서버 네트워크 서비스 (담긴 코어가
	//         nullptr이거나, 해당 엔진이 이 빌드 구성(USE_NETWORK_IOCP/
	//         USE_NETWORK_RIO)에서 꺼져 있으면 nullptr)
	//***************************************************************************
	static CNetServiceRef CreateServerService(
		EngineCoreRef engineCore,
		CNetAddress address,
		SessionFactory factory,
		int32 maxSessionCount = 1,
		uint32 workerThreadCount = 0
	);

	//***************************************************************************
	// @brief 엔진 코어에 따라 클라이언트 서비스 객체를 생성합니다.
	// @param engineCore IOCP 또는 RIO 코어 참조(둘 중 하나를 담은 variant).
	// @param address 접속할 서버의 네트워크 주소 (IP/Port)
	// @param factory 세션 객체를 생성하는 팩토리 함수
	// @param maxSessionCount 생성할 클라이언트 세션 수 (기본값: 1)
	// @param workerThreadCount 워커 스레드 개수 (기본값: 0=자동, 엔진별 산정 공식이
	//        다름 — 클래스 위 "[워커 스레드 개수 설정 가이드]" 참고)
	// @return CNetServiceRef 생성된 클라이언트 네트워크 서비스 (실패 시 nullptr)
	//***************************************************************************
	static CNetServiceRef CreateClientService(
		EngineCoreRef engineCore,
		CNetAddress address,
		SessionFactory factory,
		int32 maxSessionCount = 1,
		uint32 workerThreadCount = 0
	);
};

#endif // ndef UC_NETWORKFACTORY_H