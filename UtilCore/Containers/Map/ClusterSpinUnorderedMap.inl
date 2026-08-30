
//***************************************************************************
// ClusterSpinUnorderedMap.inl : implementation of the CClusterSpinUnorderedMap class.
//
//***************************************************************************

//***************************************************************************
// @brief CClusterSpinUnorderedMap 생성자
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock, typename Hash>
CClusterSpinUnorderedMap<T1, T2, nClusterCnt, bInnerLock, Hash>::CClusterSpinUnorderedMap(void)
{
	clearObjectMap();
}

//***************************************************************************
// @brief CClusterSpinUnorderedMap 소멸자
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock, typename Hash>
CClusterSpinUnorderedMap<T1, T2, nClusterCnt, bInnerLock, Hash>::~CClusterSpinUnorderedMap(void)
{
	clearObjectMap();
}

//***************************************************************************
// @brief 모든 클러스터의 사이즈를 합산합니다.
// @return size_t 전체 데이터 개수
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock, typename Hash>
size_t CClusterSpinUnorderedMap<T1, T2, nClusterCnt, bInnerLock, Hash>::getSize(void)
{
	size_t size = 0;

	for( int i = 0; i < nClusterCnt; ++i )
	{
		readLock(i, __FUNCTION__);
		size += m_ObjectMaps[i].size();
		readUnlock(i, __FUNCTION__);
	}
	return size;
}

//***************************************************************************
// @brief 클러스터별 쓰기 락을 이용하여 데이터 삽입
// @param key 삽입할 데이터의 키
// @param object 삽입할 객체 값
// @return bool 삽입 성공 시 true, 이미 존재하면 false
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock, typename Hash>
bool CClusterSpinUnorderedMap<T1, T2, nClusterCnt, bInnerLock, Hash>::InsertObject(T1 key, T2 object)
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
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock, typename Hash>
T2 CClusterSpinUnorderedMap<T1, T2, nClusterCnt, bInnerLock, Hash>::FindObject(T1 key)
{
	__int32 nClusterIdx = getClusterIdx(key);
	T2 object{};

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
// @param object 검색된 객체를 전달받을 참조자
// @return bool 검색 성공 시 true, 존재하지 않으면 false
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock, typename Hash>
bool CClusterSpinUnorderedMap<T1, T2, nClusterCnt, bInnerLock, Hash>::FindObject(T1 key, T2& object)
{
	__int32 nClusterIdx = getClusterIdx(key);
	bool nRet = false;

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
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock, typename Hash>
bool CClusterSpinUnorderedMap<T1, T2, nClusterCnt, bInnerLock, Hash>::EraseObject(T1 key)
{
	__int32 nClusterIdx = getClusterIdx(key);
	bool nRet = false;

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
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock, typename Hash>
void CClusterSpinUnorderedMap<T1, T2, nClusterCnt, bInnerLock, Hash>::clearObjectMap(void)
{
	for( __int32 i = 0; i < nClusterCnt; ++i )
	{
		writeLock(i, __FUNCTION__);
		m_ObjectMaps[i].clear();
		writeUnlock(i, __FUNCTION__);
	}
}