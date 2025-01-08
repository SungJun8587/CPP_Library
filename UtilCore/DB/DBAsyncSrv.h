
//***************************************************************************
// DBAsyncSrv.h : interface for the CDBAsyncSrv class.
//
//***************************************************************************

#ifndef __DBASYNCSRV__H__
#define __DBASYNCSRV__H__

#pragma pack(push, 1)

#include <queue>
#include <map>

struct st_DBAsyncRq : public CPoolObj		// DB request �⺻ ����ü
{
	st_DBAsyncRq()
	{
		callIdent = 0;
		bReTry = false;
	}
	~st_DBAsyncRq() {
		//LOG_ERROR(_T("Delete st_DBAsyncRq")); 
	}

	BYTE	callIdent;						// �ڵ鷯 �ĺ���
	bool	bReTry;							// ��õ� ����
};

struct st_DBAsyncRp : public CPoolObj // DB response �⺻ ����ü
{
	st_DBAsyncRp()
	{
		callIdent = 0;
		pthis = nullptr;
	}
	~st_DBAsyncRp() {
		LOG_ERROR(_T("Delete st_DBAsyncRp"));
	}

	BYTE	callIdent;						// �ڵ鷯 �ĺ���	
	st_DBAsyncRp* pthis;
};

#pragma pack(pop)

class CDBAsyncSrvHandler
{
public:
	CDBAsyncSrvHandler() {}
	virtual ~CDBAsyncSrvHandler() {}

	virtual EDBReturnType ProcessAsyncCall(st_DBAsyncRq* pStAsync) = 0;
};

#endif // ndef __DBASYNCSRV__H__

