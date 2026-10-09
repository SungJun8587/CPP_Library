
//***************************************************************************
// DBAsyncHandler.h : interface for the command##_handler class.
//
//***************************************************************************

#ifndef UC_DBASYNCHANDLER_H
#define UC_DBASYNCHANDLER_H

#ifndef UC_DBASYNCSRV_H
#include <DB/DBAsyncSrv.h>
#endif

#include <DB/DBAsyncRegistry.h>

//***************************************************************************
// [등록 시점] 핸들러 객체는 정적 초기화 시점에 만들어지지만, srvClass 식은 그때 평가되지
// 않는다. 서비스에 대한 Regist()는 DBAsyncRegistry에 예약되었다가 각 서비스의
// StartService()가 시작 직전에 DBAsyncRegistry::Flush()로 실행한다. srvClass가 평가되는
// 시점에는 BaseGlobal::Init()이 끝나 gpMemory를 쓸 수 있다.
//***************************************************************************

//***************************************************************************
// [범용] 위 세 개로 커버 안 되는 새 백엔드가 생기면 srvClass를 직접 명시.
// 사용 예: DECLARE_DBASYNC_HANDLER(CSomeNewAsyncSrv, command) { ... }
//***************************************************************************
#define DECLARE_DBASYNC_HANDLER_EX(srvClass, command) \
class command##_handler : public CDBAsyncSrvHandler \
{\
public:\
	command##_handler(){} \
	virtual ~command##_handler(){} \
	virtual EDBReturnType ProcessAsyncCall(st_DBAsyncRq* pStAsync); \
	\
	static std::shared_ptr<CDBAsyncSrvHandler> asyncHandler; \
}; \
	shared_ptr<CDBAsyncSrvHandler> command##_handler::asyncHandler = DBAsyncRegistry::Defer(std::make_shared<command##_handler>(), \
		[](const std::shared_ptr<CDBAsyncSrvHandler>& handler) { (srvClass).Regist(command, handler); }); \
	EDBReturnType command##_handler::ProcessAsyncCall(st_DBAsyncRq* pStAsync)

#endif // ndef UC_DBASYNCHANDLER_H