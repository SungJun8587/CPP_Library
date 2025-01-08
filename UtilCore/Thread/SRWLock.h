
//***************************************************************************
// SRWLock.h: interface for the CSRWLock class.
//
//***************************************************************************

#ifndef __SRWLOCK_H__
#define __SRWLOCK_H__

//***************************************************************************
//
class CSRWLock  
{
public:
	CSRWLock(void);
	virtual ~CSRWLock(void);

	void	SharedLock(void);
	void	SharedUnLock(void);
	void	ExclusiveLock(void);
	void	ExclusiveUnLock(void);

protected:
	CSRWLock(const CSRWLock&);
	CSRWLock& operator=(const CSRWLock&);

	SRWLOCK		m_SRWLock;
};

//***************************************************************************
//
class CSharedLock
{
public:
	CSharedLock(CSRWLock* pRWLock);
	~CSharedLock();

private:
	CSRWLock		*m_pRWLock;
};

//***************************************************************************
//
class CExclusiveLock
{
public:
	CExclusiveLock(CSRWLock* pRWLock);
	~CExclusiveLock();

private:
	CSRWLock		*m_pRWLock;
};

//***************************************************************************
// CSharedLock Construction/Destruction
//***************************************************************************

inline CSharedLock::CSharedLock( CSRWLock * pRWLock ) : m_pRWLock( pRWLock )
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

inline CExclusiveLock::CExclusiveLock( CSRWLock * pRWLock ) : m_pRWLock( pRWLock )
{
	if( m_pRWLock != NULL ) m_pRWLock->ExclusiveLock();
}

inline CExclusiveLock::~CExclusiveLock()
{
	if( m_pRWLock != NULL ) m_pRWLock->ExclusiveUnLock();
}

#endif // ndef __SRWLOCK_H__
