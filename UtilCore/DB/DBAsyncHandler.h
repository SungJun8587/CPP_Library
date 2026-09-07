
//***************************************************************************
// DBAsyncHandler.h : interface for the command##_handler class.
//
//***************************************************************************

#ifndef UC_DBASYNCHANDLER_H
#define UC_DBASYNCHANDLER_H

#ifndef UC_DBASYNCSRV_H
#include <DB/DBAsyncSrv.h>
#endif

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
	shared_ptr<CDBAsyncSrvHandler> command##_handler::asyncHandler = (srvClass).Regist(command, std::make_shared<command##_handler>()); \
	EDBReturnType command##_handler::ProcessAsyncCall(st_DBAsyncRq* pStAsync)

#endif // ndef UC_DBASYNCHANDLER_H