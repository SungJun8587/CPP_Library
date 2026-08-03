
//***************************************************************************
// UnOrderedMap.inl : implementation of the CUnOrderedMap class.
//
//***************************************************************************

template<typename T1, typename T2>
CUnOrderedMap<T1, T2>::CUnOrderedMap(void)
{
	clearObjectMap();
}

template<typename T1, typename T2>
CUnOrderedMap<T1, T2>::~CUnOrderedMap(void)
{
	clearObjectMap();
}

//***************************************************************************
// GetSize : 읽기 락을 사용하여 해시 맵 크기 반환
// @return int32 데이터 개수
//***************************************************************************
template<typename T1, typename T2>
int32 CUnOrderedMap<T1, T2>::GetSize()
{
	std::shared_lock<std::shared_mutex> lockGuard(_mutex);
	return static_cast<int32>(_objectMap.size());
}

//***************************************************************************
// IsEmpty : 읽기 락을 사용하여 해시 맵이 비어있는지 확인
// @return bool 비어있으면 true
//***************************************************************************
template<typename T1, typename T2>
bool CUnOrderedMap<T1, T2>::IsEmpty()
{
	std::shared_lock<std::shared_mutex> lockGuard(_mutex);
	return _objectMap.empty();
}

//***************************************************************************
// InsertObject : 쓰기 락을 사용하여 데이터 삽입
// @param key 삽입할 데이터의 키
// @param object 삽입할 객체 값
// @return bool 삽입 성공 시 true, 이미 존재하면 false
//***************************************************************************
template<typename T1, typename T2>
bool CUnOrderedMap<T1, T2>::InsertObject(const T1& key, const T2& object)
{
	std::unique_lock<std::shared_mutex> lockGuard(_mutex);
	auto rst = _objectMap.insert(typename ObjectMap::value_type(key, object));
	return rst.second;
}

//***************************************************************************
// InsertAndUpdateObject : 쓰기 락을 사용하여 키가 없으면 삽입, 있으면 갱신
// @param key 대상 키
// @param object 갱신하거나 삽입할 객체 값
// @return bool 성공 시 true
//***************************************************************************
template<typename T1, typename T2>
bool CUnOrderedMap<T1, T2>::InsertAndUpdateObject(const T1& key, const T2& object)
{
	std::unique_lock<std::shared_mutex> lockGuard(_mutex);
	auto iter = _objectMap.find(key);
	if( iter == _objectMap.end() )
	{
		auto rst = _objectMap.insert(typename ObjectMap::value_type(key, object));
		return rst.second;
	}

	iter->second = object;
	return true;
}

//***************************************************************************
// FindObject : 읽기 락을 사용하여 데이터 검색
// @param key 검색할 데이터의 키
// @param outObject [out] 결과를 담을 참조자
// @return bool 검색 성공 시 true, 존재하지 않으면 false
//***************************************************************************
template<typename T1, typename T2>
bool CUnOrderedMap<T1, T2>::FindObject(const T1& key, T2& outObject)
{
	std::shared_lock<std::shared_mutex> lockGuard(_mutex);
	auto iter = _objectMap.find(key);
	if( iter == _objectMap.end() )
		return false;

	outObject = iter->second;
	return true;
}

//***************************************************************************
// EraseObject : 쓰기 락을 사용하여 데이터 삭제
// @param key 삭제할 데이터의 키
// @return bool 삭제 성공 시 true, 존재하지 않으면 false
//***************************************************************************
template<typename T1, typename T2>
bool CUnOrderedMap<T1, T2>::EraseObject(const T1& key)
{
	std::unique_lock<std::shared_mutex> lockGuard(_mutex);
	auto iter = _objectMap.find(key);
	if( iter == _objectMap.end() )
		return false;

	_objectMap.erase(iter);
	return true;
}

//***************************************************************************
// clearObjectMap : 쓰기 락을 사용하여 내부 해시 맵 비우기
//***************************************************************************
template<typename T1, typename T2>
void CUnOrderedMap<T1, T2>::clearObjectMap(void)
{
	std::unique_lock<std::shared_mutex> lockGuard(_mutex);
	_objectMap.clear();
}