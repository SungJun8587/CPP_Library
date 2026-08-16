
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
	// @param engineCoreRef 내부 네트워크 코어 참조 포인터 (CIocpCore* 또는 CRioCore*)
	// @return std::shared_ptr<CNetService> 생성된 서버 네트워크 서비스 스마트 포인터
	//***************************************************************************
	static CNetServiceRef CreateServerService(
		ENetworkEngineType engineType,
		CNetAddress address,
		SessionFactory factory,
		int32 maxSessionCount = 1,
		void* engineCoreRef = nullptr
	);

	//***************************************************************************
	// @brief 엔진 유형에 따라 클라이언트 서비스 객체를 생성합니다.
	// @param engineType 생성할 네트워크 엔진 유형 (IOCP 또는 RIO)
	// @param address 접속할 서버의 네트워크 주소 (IP/Port)
	// @param factory 세션 객체를 생성하는 팩토리 함수
	// @param maxSessionCount 생성할 클라이언트 세션 수 (기본값: 1)
	// @param engineCoreRef 내부 네트워크 코어 참조 포인터 (CIocpCore* 또는 CRioCore*)
	// @return std::shared_ptr<CNetService> 생성된 클라이언트 네트워크 서비스 스마트 포인터
	//***************************************************************************
	static CNetServiceRef CreateClientService(
		ENetworkEngineType engineType,
		CNetAddress address,
		SessionFactory factory,
		int32 maxSessionCount = 1,
		void* engineCoreRef = nullptr
	);
};

#endif // ndef __NETWORKFACTORY_H__