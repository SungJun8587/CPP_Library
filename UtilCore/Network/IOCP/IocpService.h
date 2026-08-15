
//***************************************************************************
// IocpService.h : interface for the CIocpServerService & CIocpClientService.
//
//***************************************************************************

#ifndef __IOCPSERVICE_H__
#define __IOCPSERVICE_H__

#ifndef __NET_SERVICE_H__
#include <Network/NetService.h>
#endif

#ifndef __IOCPCORE_H__
#include <Network/IOCP/IocpCore.h>
#endif

#ifndef __IOCPLISTENER_H__
#include <Network/IOCP/IocpListener.h>
#endif

//***************************************************************************
// @brief IOCP 기반의 서버 전용 서비스 클래스
// @note CIocpListener를 내부에 두고 비동기 AcceptEx 작업을 총괄 제어합니다.
//***************************************************************************
class CIocpServerService : public CNetService
{
public:
	//***************************************************************************
	// @brief CIocpServerService 생성자
	// @param address 서버 바인딩 주소
	// @param iocpCore 연동할 IOCP 코어 객체
	// @param factory 세션 생성 팩토리
	// @param maxSessionCount 최대 동시 접속 수
	//***************************************************************************
	CIocpServerService(CNetAddress address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount = 1);

	//***************************************************************************
	// @brief CIocpServerService 소멸자
	//***************************************************************************
	virtual ~CIocpServerService() = default;

	//***************************************************************************
	// @brief IOCP 서버 구동 (Listener 생성 및 AcceptEx 개시)
	// @return bool 성공 여부
	//***************************************************************************
	virtual bool	Start() override;

	//***************************************************************************
	// @brief 서버 서비스 종료 (Listener 소켓 정리 및 세션 해제)
	//***************************************************************************
	virtual void	Close() override;

	//***************************************************************************
	// @brief 소속된 IOCP Core 객체를 반환합니다.
	// @return CIocpCoreRef IOCP Core 참조
	//***************************************************************************
	CIocpCoreRef	GetIocpCore() const { return _iocpCore; }

private:
	CIocpCoreRef	_iocpCore = nullptr;
	CIocpListenerRef _listener = nullptr;
};

//***************************************************************************
// @brief IOCP 기반의 클라이언트 전용 서비스 클래스
// @note 지정된 개수만큼 세션을 생성하여 IOCP Core에 등록하고 원격 서버 접속을 준비합니다.
//***************************************************************************
class CIocpClientService : public CNetService
{
public:
	//***************************************************************************
	// @brief CIocpClientService 생성자
	// @param address 접속할 원격 서버 주소
	// @param iocpCore 연동할 IOCP 코어 객체
	// @param factory 세션 생성 팩토리
	// @param maxSessionCount 생성할 세션 개수
	//***************************************************************************
	CIocpClientService(CNetAddress address, CIocpCoreRef iocpCore, SessionFactory factory, int32 maxSessionCount = 1);

	//***************************************************************************
	// @brief CIocpClientService 소멸자
	//***************************************************************************
	virtual ~CIocpClientService() = default;

	//***************************************************************************
	// @brief IOCP 클라이언트 구동 (세션 생성 및 IOCP 등록)
	// @return bool 성공 여부
	//***************************************************************************
	virtual bool	Start() override;

	//***************************************************************************
	// @brief 소속된 IOCP Core 객체를 반환합니다.
	// @return CIocpCoreRef IOCP Core 참조
	//***************************************************************************
	CIocpCoreRef	GetIocpCore() const { return _iocpCore; }

private:
	CIocpCoreRef	_iocpCore = nullptr;
};

#endif // ndef __IOCPSERVICE_H__