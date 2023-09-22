
//***************************************************************************
// ClusteredMap.h : interface for the CClusteredMap class.
//
//***************************************************************************

#ifndef __CLUSTEREDMAP_H__
#define __CLUSTEREDMAP_H__

template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock = true>
class CClusteredMap
{
public:
	typedef	CMap< T1, T2 >							ObjectMap;
	typedef	std::pair< const T1, T2 >				ObjectMapPair;

	CClusteredMap(void);
	virtual	~CClusteredMap(void);

public:
	INT32		getSize();
	bool		InsertObject(T1 key, T2 object);
	T2			FindObject(T1 key);
	bool		FindObject(T1 key, T2& object);
	bool		EraseObject(T1 key);

	void		ReadLock(T1& key) {
		readLock(getClusterIdx(key));
	}
	void		ReadUnlock(T1& key) {
		readUnlock(getClusterIdx(key));
	}
	void		WriteLock(T1& key) {
		writeLock(getClusterIdx(key));
	}
	void		WriteUnlock(T1& key) {
		writeUnlock(getClusterIdx(key));
	}
	ObjectMap& GetObjectMap(T1& key) {
		return m_ObjectMaps[getClusterIdx(key)];
	}
	__int32		GetClusterCnt(void) {
		return nClusterCnt;
	}

	void		ClearObjectMap(void) {
		clearObjectMap();
	}

protected:
	__int32		getClusterIdx(T1& key) {
		return static_cast<__int32>(key % nClusterCnt);
	}
	void		readLock(__int32 nClusterIdx) {
		if( bInnerLock ) m_ObjectLocks[nClusterIdx].ReadLock();
	}
	void		readUnlock(__int32 nClusterIdx) {
		if( bInnerLock ) m_ObjectLocks[nClusterIdx].ReadUnlock();
	}
	void		writeLock(__int32 nClusterIdx) {
		if( bInnerLock ) m_ObjectLocks[nClusterIdx].WriteLock();
	}
	void		writeUnlock(__int32 nClusterIdx) {
		if( bInnerLock ) m_ObjectLocks[nClusterIdx].WriteUnlock();
	}
	void		clearObjectMap(void);

public:
	ObjectMap					m_ObjectMaps[nClusterCnt];
	CRWLock						m_ObjectLocks[nClusterCnt];
};

#endif // ndef __CLUSTEREDMAP_H__
