//***************************************************************************
// ClusteredMap.h : interface for the CClusteredMap class.
//
//***************************************************************************

#ifndef __CLUSTEREDMAP_H__
#define __CLUSTEREDMAP_H__

#ifndef __SPINLOCK_H__
#include <Thread/SpinLock.h>
#endif

// 임의의 전방 선언 대응 (CMap 환경에 맞춰 필요 시 활성화)
// template<typename K, typename V> class CMap; 

template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock = true>
class CClusteredMap
{
public:
	typedef	CMap< T1, T2 >					ObjectMap;
	typedef	std::pair< const T1, T2 >		ObjectMapPair;

	CClusteredMap(void);
	virtual	~CClusteredMap(void);

public:
	INT32		getSize();
	bool		InsertObject(T1 key, T2 object);
	T2			FindObject(T1 key);
	bool		FindObject(T1 key, T2& object);
	bool		EraseObject(T1 key);

	// 외부 노출 락 API에도 __FUNCTION__을 받을 수 있도록 디폴트 인자 구성
	void		ReadLock(T1& key, const char* name = nullptr) {
		readLock(getClusterIdx(key), name);
	}
	void		ReadUnlock(T1& key, const char* name = nullptr) {
		readUnlock(getClusterIdx(key), name);
	}
	void		WriteLock(T1& key, const char* name = nullptr) {
		writeLock(getClusterIdx(key), name);
	}
	void		WriteUnlock(T1& key, const char* name = nullptr) {
		writeUnlock(getClusterIdx(key), name);
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

	// 프로파일러 전달용 name 매개변수 추가
	void		readLock(__int32 nClusterIdx, const char* name = nullptr) {
		if (bInnerLock) m_ObjectLocks[nClusterIdx].ReadLock(name);
	}
	void		readUnlock(__int32 nClusterIdx, const char* name = nullptr) {
		if (bInnerLock) m_ObjectLocks[nClusterIdx].ReadUnlock(name);
	}
	void		writeLock(__int32 nClusterIdx, const char* name = nullptr) {
		if (bInnerLock) m_ObjectLocks[nClusterIdx].WriteLock(name);
	}
	void		writeUnlock(__int32 nClusterIdx, const char* name = nullptr) {
		if (bInnerLock) m_ObjectLocks[nClusterIdx].WriteUnlock(name);
	}
	void		clearObjectMap(void);

public:
	ObjectMap			m_ObjectMaps[nClusterCnt];
	RWSpinLockDefault	m_ObjectLocks[nClusterCnt];
};

#endif // ndef __CLUSTEREDMAP_H__