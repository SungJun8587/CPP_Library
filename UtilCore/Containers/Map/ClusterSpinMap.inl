
//***************************************************************************
// ClusterSpinMap.inl : implementation of the CClusterSpinMap class.
//
//***************************************************************************

//***************************************************************************
// @brief 모든 클러스터의 사이즈를 합산합니다.
// @details 각 클러스터 읽기는 그 순간 안전하지만, 전체 합산이 하나의 순간을
//          나타내는 atomic snapshot은 아닙니다 (클러스터별 순차 락/언락).
// @return size_t 전체 데이터 개수
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock, typename Hash>
size_t CClusterSpinMap<T1, T2, nClusterCnt, bInnerLock, Hash>::getSize(void)
{
	size_t size = 0;

	for( __int32 i = 0; i < nClusterCnt; ++i )
	{
		InnerReadGuard guard(*this, i, __FUNCTION__);
		size += m_ObjectMaps[i].size();
	}
	return size;
}

//***************************************************************************
// @brief 클러스터별 쓰기 락을 이용하여 데이터 삽입
// @param key 삽입할 데이터의 키 (const 참조 — 불필요한 복사 방지)
// @param object 삽입할 객체 값 (값 전달 후 move로 맵에 삽입 — 이미 만들어진
//        parameter object를 맵 내부에 다시 복사하지 않고 이동으로 구성)
// @return bool 삽입 성공 시 true, 이미 존재하면 false
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock, typename Hash>
bool CClusterSpinMap<T1, T2, nClusterCnt, bInnerLock, Hash>::InsertObject(const T1& key, T2 object)
{
	__int32 nClusterIdx = getClusterIdx(key);

	// RAII 가드: try_emplace()가 예외(비교 연산자, 노드 할당 실패 등)를 던져도
	// 스코프 종료 시 반드시 writeUnlock이 호출됩니다.
	InnerWriteGuard guard(*this, nClusterIdx, __FUNCTION__);

	// try_emplace: 키가 이미 존재하면 object를 건드리지 않고 즉시 실패 반환.
	// emplace와 달리 존재하는 키에 대해 불필요한 pair 구성/소멸이 없음.
	auto rst = m_ObjectMaps[nClusterIdx].try_emplace(key, std::move(object));

	return rst.second;
}

//***************************************************************************
// @brief 클러스터별 쓰기 락을 이용하여 데이터 삽입 (rvalue key 오버로드)
// @details T1이 무거운 타입일 때 key 복사 없이 이동으로 노드를 구성합니다.
//          getClusterIdx(key)는 key를 소비하지 않는 const 참조 호출이므로
//          std::move(key) 이전에 안전하게 클러스터 인덱스를 계산할 수 있습니다.
// @param key 삽입할 데이터의 키 (rvalue — 이동 생성)
// @param object 삽입할 객체 값 (값 전달 후 move로 맵에 삽입)
// @return bool 삽입 성공 시 true, 이미 존재하면 false
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock, typename Hash>
bool CClusterSpinMap<T1, T2, nClusterCnt, bInnerLock, Hash>::InsertObject(T1&& key, T2 object)
{
	__int32 nClusterIdx = getClusterIdx(key);	// key는 아직 유효 (move 이전)

	InnerWriteGuard guard(*this, nClusterIdx, __FUNCTION__);

	auto rst = m_ObjectMaps[nClusterIdx].try_emplace(std::move(key), std::move(object));

	return rst.second;
}

//***************************************************************************
// @brief 단건 반환 형태의 조회
// @param key 검색할 데이터의 키
// @return T2 찾은 객체 값 (T2는 기본 생성 가능해야 함)
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock, typename Hash>
T2 CClusterSpinMap<T1, T2, nClusterCnt, bInnerLock, Hash>::FindObject(const T1& key)
{
	__int32 nClusterIdx = getClusterIdx(key);
	T2 object{};	// 값 초기화 — 미발견 시 미초기화 값(garbage) 반환 방지

	// RAII 가드: object = it->second (T2::operator=)가 예외를 던져도
	// 스코프 종료 시 반드시 readUnlock이 호출됩니다.
	InnerReadGuard guard(*this, nClusterIdx, __FUNCTION__);
	auto it = m_ObjectMaps[nClusterIdx].find(key);
	if( it != m_ObjectMaps[nClusterIdx].end() )
		object = it->second;

	return object;
}

//***************************************************************************
// @brief 참조자 대입 형태의 조회 (성공 여부 반환)
// @param key 검색할 데이터의 키
// @param object [out] 객체가 복사될 참조자
// @return bool 검색 성공 시 true, 존재하지 않으면 false
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock, typename Hash>
bool CClusterSpinMap<T1, T2, nClusterCnt, bInnerLock, Hash>::FindObject(const T1& key, T2& object)
{
	__int32 nClusterIdx = getClusterIdx(key);
	bool nRet = false;

	InnerReadGuard guard(*this, nClusterIdx, __FUNCTION__);
	auto it = m_ObjectMaps[nClusterIdx].find(key);
	if( it != m_ObjectMaps[nClusterIdx].end() )
	{
		object = it->second;
		nRet = true;
	}

	return nRet;
}

//***************************************************************************
// @brief 클러스터별 쓰기 락을 이용하여 데이터 삭제
// @param key 삭제할 데이터의 키
// @return bool 삭제 성공 시 true, 존재하지 않으면 false
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock, typename Hash>
bool CClusterSpinMap<T1, T2, nClusterCnt, bInnerLock, Hash>::EraseObject(const T1& key)
{
	__int32 nClusterIdx = getClusterIdx(key);
	bool nRet = false;

	InnerWriteGuard guard(*this, nClusterIdx, __FUNCTION__);
	auto iter = m_ObjectMaps[nClusterIdx].find(key);
	if( iter != m_ObjectMaps[nClusterIdx].end() )
	{
		m_ObjectMaps[nClusterIdx].erase(iter);
		nRet = true;
	}

	return nRet;
}

//***************************************************************************
// @brief 전체 클러스터를 순회하며 맵 데이터 비우기
// @details 클러스터별로는 안전하게 비우지만, 전체를 아우르는 하나의 순간에
//          atomic하게 비우는 것은 아닙니다 (다른 스레드가 이미 처리된
//          클러스터 이후 클러스터에 동시에 insert할 수 있음).
//***************************************************************************
template<typename T1, typename T2, __int32 nClusterCnt, bool bInnerLock, typename Hash>
void CClusterSpinMap<T1, T2, nClusterCnt, bInnerLock, Hash>::clearObjectMap(void)
{
	for( __int32 i = 0; i < nClusterCnt; ++i )
	{
		InnerWriteGuard guard(*this, i, __FUNCTION__);
		m_ObjectMaps[i].clear();
	}
}
