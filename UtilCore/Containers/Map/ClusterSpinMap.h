
//***************************************************************************
// ClusterSpinMap.h : interface for the CClusterSpinMap class.
//
//***************************************************************************

#ifndef __CLUSTERSPINMAP_H__
#define __CLUSTERSPINMAP_H__

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef __PLATFORMLOCK_H__
#include <Thread/PlatformLock.h>
#endif

template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock = true>
class CClusterSpinMap : public BaseAllocator
{
public:
	typedef	CMap< T1, T2 >					ObjectMap;
	typedef	std::pair< const T1, T2 >		ObjectMapPair;

	CClusterSpinMap(void);
	virtual	~CClusterSpinMap(void);

public:
	INT32		getSize();
	bool		InsertObject(T1 key, T2 object);
	T2			FindObject(T1 key);
	bool		FindObject(T1 key, T2& object);
	bool		EraseObject(T1 key);

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

	void		readLock(__int32 nClusterIdx, const char* name = nullptr) {
		if( bInnerLock ) m_ObjectLocks[nClusterIdx].ReadLock(name);
	}
	void		readUnlock(__int32 nClusterIdx, const char* name = nullptr) {
		if( bInnerLock ) m_ObjectLocks[nClusterIdx].ReadUnlock(name);
	}
	void		writeLock(__int32 nClusterIdx, const char* name = nullptr) {
		if( bInnerLock ) m_ObjectLocks[nClusterIdx].WriteLock(name);
	}
	void		writeUnlock(__int32 nClusterIdx, const char* name = nullptr) {
		if( bInnerLock ) m_ObjectLocks[nClusterIdx].WriteUnlock(name);
	}
	void		clearObjectMap(void);

public:
	ObjectMap			m_ObjectMaps[nClusterCnt];    // 클러스터별로 데이터를 저장하는 해시맵 배열 (총 nClusterCnt개)
	PRWLock				m_ObjectLocks[nClusterCnt];   // 각 클러스터의 동시성 제어를 위한 읽기/쓰기 락(RWLock) 배열
};

#endif // ndef __CLUSTERSPINMAP_H__