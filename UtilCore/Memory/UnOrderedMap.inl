
//***************************************************************************
// UnOrderedMap.inl : implementation of the CUnOrderedMap class.
//
//***************************************************************************

#pragma once

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

template<typename T1, typename T2>
int32 CUnOrderedMap<T1, T2>::GetSize()
{
	std::shared_lock<std::shared_mutex> lockGuard(_mutex);
	return static_cast<int32>(_objectMap.size());
}

template<typename T1, typename T2>
bool CUnOrderedMap<T1, T2>::IsEmpty()
{
	std::shared_lock<std::shared_mutex> lockGuard(_mutex);
	return _objectMap.empty();
}

template<typename T1, typename T2>
bool CUnOrderedMap<T1, T2>::InsertObject(const T1& key, const T2& object)
{
	std::unique_lock<std::shared_mutex> lockGuard(_mutex);
	auto rst = _objectMap.insert(typename ObjectMap::value_type(key, object));
	return rst.second;
}

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

	// [수정] 위험한 memcpy 대신 안전한 대입 연산자 사용
	iter->second = object;
	return true;
}

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

template<typename T1, typename T2>
void CUnOrderedMap<T1, T2>::clearObjectMap(void)
{
	std::unique_lock<std::shared_mutex> lockGuard(_mutex);
	_objectMap.clear();
}