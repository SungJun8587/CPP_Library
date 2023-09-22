
//***************************************************************************
// DBAsyncSrv.h : interface for the CDBAsyncSrv class.
//
//***************************************************************************

#ifndef __DBASYNCSRV__H__
#define __DBASYNCSRV__H__

#pragma pack(push, 1)

#include <queue>
#include <map>

struct st_DBAsyncRq : public CPoolObj		// DB request 기본 구조체
{
	st_DBAsyncRq()
	{
		callIdent = 0;
		bReTry = false;
	}
	~st_DBAsyncRq() {
		// LOG_WRITE(LOG_TYPE_ERROR, _T("delete st_DBAsyncRq")); 
	}

	BYTE	callIdent;						// 핸들러 식별자
	bool	bReTry;							// 재시도 여부
};

struct st_DBAsyncRp : public CPoolObj // DB response 기본 구조체
{
	st_DBAsyncRp()
	{
		callIdent = 0;
		pthis = nullptr;
	}
	~st_DBAsyncRp() {
		// LOG_WRITE(LOG_TYPE_ERROR, _T("delete st_DBAsyncRp")); 
	}

	BYTE	callIdent;						// 핸들러 식별자	
	st_DBAsyncRp* pthis;
};

#pragma pack(pop)

class CDBAsyncSrvHandler
{
public:
	CDBAsyncSrvHandler() {}
	virtual ~CDBAsyncSrvHandler() {}

	virtual int ProcessAsyncCall(st_DBAsyncRq* pStAsync) = 0;
};

#endif // ndef __DBASYNCSRV__H__

