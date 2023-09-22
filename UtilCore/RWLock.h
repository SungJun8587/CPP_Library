
//***************************************************************************
// RWLock.h: interface for the CRWLock class.
//
//***************************************************************************

#ifndef __RWLOCK_H__
#define __RWLOCK_H__

//***************************************************************************
//
class CRWLock  
{
public:
	CRWLock(void);
	virtual ~CRWLock(void);

	void	SharedLock(void);
	void	SharedUnLock(void);
	void	ExclusiveLock(void);
	void	ExclusiveUnLock(void);

protected:
	CRWLock(const CRWLock&);
	CRWLock& operator=(const CRWLock&);

	SRWLOCK		m_SRWLock;
};

//***************************************************************************
//
class CSharedLock
{
public:
	CSharedLock(CRWLock* pRWLock);
	~CSharedLock();

private:
	CRWLock		*m_pRWLock;
};

//***************************************************************************
//
class CExclusiveLock
{
public:
	CExclusiveLock(CRWLock* pRWLock);
	~CExclusiveLock();

private:
	CRWLock		*m_pRWLock;
};

//***************************************************************************
// CSharedLock Construction/Destruction
//***************************************************************************

inline CSharedLock::CSharedLock( CRWLock * pRWLock ) : m_pRWLock( pRWLock )
{
	if( m_pRWLock != NULL ) m_pRWLock->SharedLock();
}

inline CSharedLock::~CSharedLock()
{
	if( m_pRWLock != NULL ) m_pRWLock->SharedUnLock();
}

//***************************************************************************
// CExclusiveLock Construction/Destruction
//***************************************************************************

inline CExclusiveLock::CExclusiveLock( CRWLock * pRWLock ) : m_pRWLock( pRWLock )
{
	if( m_pRWLock != NULL ) m_pRWLock->ExclusiveLock();
}

inline CExclusiveLock::~CExclusiveLock()
{
	if( m_pRWLock != NULL ) m_pRWLock->ExclusiveUnLock();
}

#endif // ndef __RWLOCK_H__
