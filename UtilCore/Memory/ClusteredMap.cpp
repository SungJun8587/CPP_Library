
//***************************************************************************
// ClusteredMap.cpp : implementation of the CClusteredMap class.
//
//***************************************************************************

#include "pch.h"
#include "ClusteredMap.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock>
CClusteredMap<T1, T2, nClusterCnt, bInnerLock>::CClusteredMap(void)
{
	clearObjectMap();
}

template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock>
CClusteredMap<T1, T2, nClusterCnt, bInnerLock>::~CClusteredMap(void)
{
	clearObjectMap();
}

//***************************************************************************
//
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock>
INT32 CClusteredMap<T1, T2, nClusterCnt, bInnerLock>::getSize(void)
{
	INT32 nSize = 0;

	int i = 0;
	for( i = 0; i < nClusterCnt; ++i )
	{
		readLock(i);
		nSize += m_ObjectMaps[i].size();
		readLock(i);
	}
	return nSize;
}

//***************************************************************************
//
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock>
bool CClusteredMap<T1, T2, nClusterCnt, bInnerLock>::InsertObject(T1 key, T2 object)
{
	__int32 nClusterIdx = getClusterIdx(key);

	writeLock(nClusterIdx);
	auto rst = m_ObjectMaps[nClusterIdx].insert(ObjectMapPair(key, object));
	writeUnlock(nClusterIdx);

	return rst.second;
}

//***************************************************************************
//
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock>
T2 CClusteredMap<T1, T2, nClusterCnt, bInnerLock>::FindObject(T1 key)
{
	__int32 nClusterIdx = getClusterIdx(key);
	T2 object;

	readLock(nClusterIdx);
	auto it = m_ObjectMaps[nClusterIdx].find(key);
	if( it != m_ObjectMaps[nClusterIdx].end() )
		object = it->second;
	readUnlock(nClusterIdx);

	return object;
}

//***************************************************************************
//
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock>
bool CClusteredMap<T1, T2, nClusterCnt, bInnerLock>::FindObject(T1 key, T2& object)
{
	__int32 nClusterIdx = getClusterIdx(key);
	bool	nRet = false;
	readLock(nClusterIdx);
	auto it = m_ObjectMaps[nClusterIdx].find(key);
	if( it != m_ObjectMaps[nClusterIdx].end() )
	{
		object = it->second;
		nRet = true;
	}
	readUnlock(nClusterIdx);

	return nRet;
}

//***************************************************************************
//
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock>
bool CClusteredMap<T1, T2, nClusterCnt, bInnerLock>::EraseObject(T1 key)
{
	__int32 nClusterIdx = getClusterIdx(key);
	bool	nRet = false;
	writeLock(nClusterIdx);
	auto iter = m_ObjectMaps[nClusterIdx].find(key);
	if( iter != m_ObjectMaps[nClusterIdx].end() )
	{
		m_ObjectMaps[nClusterIdx].erase(iter);
		nRet = true;
	}

	writeUnlock(nClusterIdx);
	return nRet;
}

//***************************************************************************
//
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock>
void CClusteredMap<T1, T2, nClusterCnt, bInnerLock>::clearObjectMap(void)
{
	for( __int32 i = 0; i < nClusterCnt; ++i )
	{
		writeLock(i);
		m_ObjectMaps[i].clear();
		writeUnlock(i);
	}
}
