
//***************************************************************************
// NetworkFactory.cpp : implementation of the CNetworkFactory class.
//
//***************************************************************************

#include "pch.h"
#include "NetworkFactory.h"

// IOCP 관련 헤더
#include <Network/IOCP/IocpCore.h>
#include <Network/IOCP/IocpService.h>

// RIO 관련 헤더
#include <Network/RIO/RioCore.h>
#include <Network/RIO/RioService.h>

//***************************************************************************
// @brief 지정된 엔진 유형에 맞는 서버 서비스 객체를 동적으로 생성합니다.
// @param engineType 네트워크 엔진 타입 (IOCP 또는 RIO)
// @param address 서버 바인딩 주소 정보
// @param factory 세션 생성용 팩토리 콜백
// @param maxSessionCount 최대 수용 세션 수
// @param workerThreadCount 워커 스레드 개수 (기본값: 0)
// @param engineCoreRef 엔진 코어 포인터
// @return std::shared_ptr<CNetService> 생성된 서비스 객체 포인터 (실패 시 nullptr)
//***************************************************************************
CNetServiceRef CNetworkFactory::CreateServerService(
	ENetworkEngineType engineType,
	CNetAddress address,
	SessionFactory factory,
	int32 maxSessionCount,
	uint32_t workerThreadCount,
	void* engineCoreRef)
{
	switch( engineType )
	{
		case ENetworkEngineType::IOCP:
		{
			if( engineCoreRef == nullptr )
				return nullptr;

			auto iocpCore = *static_cast<CIocpCoreRef*>(engineCoreRef);
			return std::make_shared<CIocpServerService>(address, iocpCore, factory, maxSessionCount, workerThreadCount);
		}
		case ENetworkEngineType::RIO:
		{
			if( engineCoreRef == nullptr )
				return nullptr;

			auto rioCore = *static_cast<CRioCoreRef*>(engineCoreRef);
			return std::make_shared<CRioServerService>(address, rioCore, factory, maxSessionCount, workerThreadCount);
		}
	}
	return nullptr;
}

//***************************************************************************
// @brief 지정된 엔진 유형에 맞는 클라이언트 서비스 객체를 동적으로 생성합니다.
// @param engineType 네트워크 엔진 타입 (IOCP 또는 RIO)
// @param address 원격 서버 주소 정보
// @param factory 세션 생성용 팩토리 콜백
// @param maxSessionCount 생성할 세션 수
// @param workerThreadCount 워커 스레드 개수 (기본값: 0)
// @param engineCoreRef 엔진 코어 포인터
// @return std::shared_ptr<CNetService> 생성된 서비스 객체 포인터 (실패 시 nullptr)
//***************************************************************************
CNetServiceRef CNetworkFactory::CreateClientService(
	ENetworkEngineType engineType,
	CNetAddress address,
	SessionFactory factory,
	int32 maxSessionCount,
	uint32_t workerThreadCount,
	void* engineCoreRef)
{
	switch( engineType )
	{
		case ENetworkEngineType::IOCP:
		{
			if( engineCoreRef == nullptr )
				return nullptr;

			auto iocpCore = *static_cast<CIocpCoreRef*>(engineCoreRef);
			return std::make_shared<CIocpClientService>(address, iocpCore, factory, maxSessionCount, workerThreadCount);
		}
		case ENetworkEngineType::RIO:
		{
			if( engineCoreRef == nullptr )
				return nullptr;

			auto rioCore = *static_cast<CRioCoreRef*>(engineCoreRef);
			return std::make_shared<CRioClientService>(address, rioCore, factory, maxSessionCount, workerThreadCount);
		}
	}
	return nullptr;
}