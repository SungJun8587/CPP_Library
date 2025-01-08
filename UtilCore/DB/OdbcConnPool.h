
//***************************************************************************
// OdbcConnPool.h : interface for the COdbcConnPool class.
//
//***************************************************************************

#ifndef __ODBCCONNPOOL_H__
#define __ODBCCONNPOOL_H__

#ifndef	__OBJECTPOOL_H__
#include <ObjectPool.h>
#endif

#ifndef	__BASEODBC_H__
#include <BaseODBC.h>
#endif

class COdbcConnPool : public CPoolObj
{
public:
	COdbcConnPool(int32& nMaxPoolSize);
	virtual ~COdbcConnPool(void);

	bool Init(const EDBClass dbClass, const TCHAR* ptszDSN);
	CBaseODBC*	GetOdbcConn(int32 nType);

protected:
	void		Clear(void);

protected:
	COdbcConnPool(const COdbcConnPool& rhs);
	COdbcConnPool& operator=(const COdbcConnPool& rhs);

	EDBClass		_dbClass;						// DB 구분
	CBaseODBC**		_pOdbcConns;					// 커넥션 풀 배열
	bool*			_pInUseFlag;					// 사용 상태 플래그 배열
	TCHAR			_tszDSN[DATABASE_DSN_STRLEN];	// DSN 값
	int32			_nMaxPoolSize;					// 최대 풀 크기
};

#endif // ndef __ODBCCONNPOOL_H__