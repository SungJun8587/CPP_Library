
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

	EDBClass		_dbClass;						// DB ����
	CBaseODBC**		_pOdbcConns;					// Ŀ�ؼ� Ǯ �迭
	bool*			_pInUseFlag;					// ��� ���� �÷��� �迭
	TCHAR			_tszDSN[DATABASE_DSN_STRLEN];	// DSN ��
	int32			_nMaxPoolSize;					// �ִ� Ǯ ũ��
};

#endif // ndef __ODBCCONNPOOL_H__