//***************************************************************************
// Session.h : interface for the CSession class.
//
//***************************************************************************

#ifndef __SESSION_H__
#define __SESSION_H__

#include <functional>
#include <memory>
#include <WinSock2.h>

class CSession;
using CSessionRef = std::shared_ptr<CSession>;
using SessionFactory = std::function<CSessionRef()>;
using DisconnectHandler = std::function<void(CSessionRef)>;

//***************************************************************************
// @class CSession
// @brief 모든 네트워크 세션의 최상위 추상 기반 클래스 (IOCP/RIO 공통 인터페이스)
//***************************************************************************
class CSession : public std::enable_shared_from_this<CSession>
{
public:
	CSession() = default;
	virtual ~CSession() = default;

	// 상위 서비스 및 세션 관리를 위한 순수 가상 함수
	virtual void			Disconnect(const TCHAR* cause) = 0;

	//***************************************************************************
	// @brief 세션의 현재 연결 상태를 반환합니다.
	// @return bool 연결되어 있으면 true, 아니면 false
	//***************************************************************************
	virtual bool			IsConnected() const = 0;

	//***************************************************************************
	// @brief 통신에 사용되는 소켓 핸들을 반환합니다.
	// @return SOCKET 소켓 핸들
	//***************************************************************************
	virtual SOCKET			GetSocket() const = 0;

	virtual bool			Send(const void* data, uint16_t size) noexcept = 0;

	// 연결 해제 이벤트 콜백 등록
	void					SetDisconnectHandler(DisconnectHandler handler) { _onDisconnected = handler; }

protected:
	// 연결 해제 발생 시 하위 구현체(CIocpSession/CRioSession)에서 호출
	void					OnDisconnected()
	{
		if( _onDisconnected )
		{
			_onDisconnected(shared_from_this()); // CSessionRef 형태로 전달
		}
	}

private:
	DisconnectHandler		_onDisconnected = nullptr;
};

#endif // ndef __SESSION_H__