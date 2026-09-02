
//***************************************************************************
// NetworkFactory.cpp : implementation of the CNetworkFactory class.
//
//***************************************************************************

#include "pch.h"
#include "NetworkFactory.h"

//***************************************************************************
// @brief 지정된 엔진 코어에 맞는 서버 서비스 객체를 동적으로 생성합니다.
// @param engineCore IOCP 또는 RIO 코어 참조(variant) — 담긴 대안이 곧 엔진 타입
// @param address 서버 바인딩 주소 정보
// @param factory 세션 생성용 팩토리 콜백
// @param maxSessionCount 최대 수용 세션 수
// @param workerThreadCount 워커 스레드 개수 (기본값: 0)
// @return CNetServiceRef 생성된 서비스 객체 포인터 (실패 시 nullptr)
// @details std::visit()가 컴파일 타임에 engineCore에 실제로 담긴 타입
//          (CIocpCoreRef 또는 CRioCoreRef)만으로 분기하므로, 예전처럼
//          "enum 값과 실제 포인터 타입이 서로 다를 수 있는" 경로 자체가
//          존재하지 않는다 — switch에 break가 빠지는 식의 fallthrough로
//          엉뚱한 타입을 static_cast하는 버그 클래스가 통째로 제거됨.
//***************************************************************************
CNetServiceRef CNetworkFactory::CreateServerService(
	EngineCoreRef engineCore,
	CNetAddress address,
	SessionFactory factory,
	int32 maxSessionCount,
	uint32 workerThreadCount)
{
	return std::visit([&](auto&& core) -> CNetServiceRef
		{
			using TCoreRef = std::decay_t<decltype(core)>;

			if( core == nullptr )
				return nullptr;

			if constexpr( std::is_same_v<TCoreRef, CIocpCoreRef> )
			{
#if defined(USE_NETWORK_IOCP)
				return std::make_shared<CIocpServerService>(address, core, factory, maxSessionCount, workerThreadCount);
#else
				return nullptr; // 이 빌드 구성에서는 IOCP 엔진이 꺼져 있음
#endif
			}
			else if constexpr( std::is_same_v<TCoreRef, CRioCoreRef> )
			{
#if defined(USE_NETWORK_RIO)
				return std::make_shared<CRioServerService>(address, core, factory, maxSessionCount, workerThreadCount);
#else
				return nullptr; // 이 빌드 구성에서는 RIO 엔진이 꺼져 있음
#endif
			}
		}, engineCore);
}

//***************************************************************************
// @brief 지정된 엔진 코어에 맞는 클라이언트 서비스 객체를 동적으로 생성합니다.
// @param engineCore IOCP 또는 RIO 코어 참조(variant) — 담긴 대안이 곧 엔진 타입
// @param address 원격 서버 주소 정보
// @param factory 세션 생성용 팩토리 콜백
// @param maxSessionCount 생성할 세션 수
// @param workerThreadCount 워커 스레드 개수 (기본값: 0)
// @return CNetServiceRef 생성된 서비스 객체 포인터 (실패 시 nullptr)
//***************************************************************************
CNetServiceRef CNetworkFactory::CreateClientService(
	EngineCoreRef engineCore,
	CNetAddress address,
	SessionFactory factory,
	int32 maxSessionCount,
	uint32 workerThreadCount)
{
	return std::visit([&](auto&& core) -> CNetServiceRef
		{
			using TCoreRef = std::decay_t<decltype(core)>;

			if( core == nullptr )
				return nullptr;

			if constexpr( std::is_same_v<TCoreRef, CIocpCoreRef> )
			{
#if defined(USE_NETWORK_IOCP)
				return std::make_shared<CIocpClientService>(address, core, factory, maxSessionCount, workerThreadCount);
#else
				return nullptr;
#endif
			}
			else if constexpr( std::is_same_v<TCoreRef, CRioCoreRef> )
			{
#if defined(USE_NETWORK_RIO)
				return std::make_shared<CRioClientService>(address, core, factory, maxSessionCount, workerThreadCount);
#else
				return nullptr;
#endif
			}
		}, engineCore);
}