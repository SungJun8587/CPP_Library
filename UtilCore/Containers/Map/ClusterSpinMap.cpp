
//***************************************************************************
// ClusterSpinMap.cpp : implementation of the CClusterSpinMap class.
//
//***************************************************************************

#include "pch.h"
#include "ClusterSpinMap.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

//***************************************************************************
// @brief CClusterSpinMap 생성자
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock>
CClusterSpinMap<T1, T2, nClusterCnt, bInnerLock>::CClusterSpinMap(void)
{
	clearObjectMap();
}

//***************************************************************************
// @brief CClusterSpinMap 소멸자
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock>
CClusterSpinMap<T1, T2, nClusterCnt, bInnerLock>::~CClusterSpinMap(void)
{
	clearObjectMap();
}

//***************************************************************************
// @brief 모든 클러스터의 사이즈를 합산합니다.
// @return INT32 전체 데이터 개수
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock>
INT32 CClusterSpinMap<T1, T2, nClusterCnt, bInnerLock>::getSize(void)
{
	INT32 nSize = 0;

	int i = 0;
	for( i = 0; i < nClusterCnt; ++i )
	{
		readLock(i, __FUNCTION__);
		nSize += m_ObjectMaps[i].size();
		readUnlock(i, __FUNCTION__);
	}
	return nSize;
}

//***************************************************************************
// @brief 클러스터별 쓰기 락을 이용하여 데이터 삽입
// @param key 삽입할 데이터의 키
// @param object 삽입할 객체 값
// @return bool 삽입 성공 시 true, 이미 존재하면 false
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock>
bool CClusterSpinMap<T1, T2, nClusterCnt, bInnerLock>::InsertObject(T1 key, T2 object)
{
	__int32 nClusterIdx = getClusterIdx(key);

	writeLock(nClusterIdx, __FUNCTION__);
	auto rst = m_ObjectMaps[nClusterIdx].insert(ObjectMapPair(key, object));
	writeUnlock(nClusterIdx, __FUNCTION__);

	return rst.second;
}

//***************************************************************************
// @brief 단건 반환 형태의 조회
// @param key 검색할 데이터의 키
// @return T2 찾은 객체 값
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock>
T2 CClusterSpinMap<T1, T2, nClusterCnt, bInnerLock>::FindObject(T1 key)
{
	__int32 nClusterIdx = getClusterIdx(key);
	T2 object;

	readLock(nClusterIdx, __FUNCTION__);
	auto it = m_ObjectMaps[nClusterIdx].find(key);
	if( it != m_ObjectMaps[nClusterIdx].end() )
		object = it->second;
	readUnlock(nClusterIdx, __FUNCTION__);

	return object;
}

//***************************************************************************
// @brief 참조자 대입 형태의 조회 (성공 여부 반환)
// @param key 검색할 데이터의 키
// @param object [out] 객체가 복사될 참조자
// @return bool 검색 성공 시 true, 존재하지 않으면 false
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock>
bool CClusterSpinMap<T1, T2, nClusterCnt, bInnerLock>::FindObject(T1 key, T2& object)
{
	__int32 nClusterIdx = getClusterIdx(key);
	bool	nRet = false;
	readLock(nClusterIdx, __FUNCTION__);
	auto it = m_ObjectMaps[nClusterIdx].find(key);
	if( it != m_ObjectMaps[nClusterIdx].end() )
	{
		object = it->second;
		nRet = true;
	}
	readUnlock(nClusterIdx, __FUNCTION__);

	return nRet;
}

//***************************************************************************
// @brief 클러스터별 쓰기 락을 이용하여 데이터 삭제
// @param key 삭제할 데이터의 키
// @return bool 삭제 성공 시 true, 존재하지 않으면 false
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock>
bool CClusterSpinMap<T1, T2, nClusterCnt, bInnerLock>::EraseObject(T1 key)
{
	__int32 nClusterIdx = getClusterIdx(key);
	bool	nRet = false;
	writeLock(nClusterIdx, __FUNCTION__);
	auto iter = m_ObjectMaps[nClusterIdx].find(key);
	if( iter != m_ObjectMaps[nClusterIdx].end() )
	{
		m_ObjectMaps[nClusterIdx].erase(iter);
		nRet = true;
	}

	writeUnlock(nClusterIdx, __FUNCTION__);
	return nRet;
}

//***************************************************************************
// @brief 전체 클러스터를 순회하며 맵 데이터 비우기
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock>
void CClusterSpinMap<T1, T2, nClusterCnt, bInnerLock>::clearObjectMap(void)
{
	for( __int32 i = 0; i < nClusterCnt; ++i )
	{
		writeLock(i, __FUNCTION__);
		m_ObjectMaps[i].clear();
		writeUnlock(i, __FUNCTION__);
	}
}