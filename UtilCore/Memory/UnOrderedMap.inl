
//***************************************************************************
// UnOrderedMap.inl : implementation of the CUnOrderedMap class.
//
//***************************************************************************

#include "pch.h"
#include "UnOrderedMap.h"

//***************************************************************************
// Construction/Destruction 
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

template<typename T1, typename T2>
typename CUnOrderedMap<T1, T2>::ObjectMapIter CUnOrderedMap<T1, T2>::GetEnd()
{
	std::shared_lock lockGuard(_mutex);

	return _objectMap.end();
}

//***************************************************************************
//
template<typename T1, typename T2>
int32 CUnOrderedMap<T1, T2>::GetSize()
{
	std::shared_lock lockGuard(_mutex);

	return static_cast<int32>(_objectMap.size());
}

//***************************************************************************
//
template<typename T1, typename T2>
bool CUnOrderedMap<T1, T2>::IsEmpty()
{
	std::shared_lock lockGuard(_mutex);

	return _objectMap.empty();
}

//***************************************************************************
//
template<typename T1, typename T2>
bool CUnOrderedMap<T1, T2>::InsertObject(T1& key, T2& object)
{
	std::unique_lock lockGuard(_mutex);

	auto rst = _objectMap.insert(ObjectMap::value_type(key, object));
	return rst.second;
}

//***************************************************************************
//
template<typename T1, typename T2>
bool CUnOrderedMap<T1, T2>::InsertAndUpdateObject(T1& key, T2& object)
{
	std::unique_lock lockGuard(_mutex);

	auto iter = _objectMap.find(key);
	if( iter == _objectMap.end() )
	{
		auto rst = _objectMap.insert(ObjectMap::value_type(key, object));
		return rst.second;
	}

	memcpy(iter->second, object, sizeof(T2));

	return true;
}

//***************************************************************************
//
template<typename T1, typename T2>
typename CUnOrderedMap<T1, T2>::ObjectMapIter CUnOrderedMap<T1, T2>::FindObject(T1& key)
{
	std::shared_lock lockGuard(_mutex);

	return _objectMap.find(key);
}

//***************************************************************************
//
template<typename T1, typename T2>
bool CUnOrderedMap<T1, T2>::FindObject(T1& key, T2& object)
{
	std::shared_lock lockGuard(_mutex);

	auto iter = _objectMap.find(key);
	if( iter == _objectMap.end() )
		return false;

	object = iter->second;
	return true;
}

//***************************************************************************
//
template<typename T1, typename T2>
bool CUnOrderedMap<T1, T2>::EraseObject(T1& key)
{
	std::unique_lock lockGuard(_mutex);

	auto iter = _objectMap.find(key);
	if( iter == _objectMap.end() )
		return false;

	_objectMap.erase(iter);
	return true;
}

//***************************************************************************
//
template<typename T1, typename T2>
void CUnOrderedMap<T1, T2>::clearObjectMap(void)
{
	std::unique_lock lockGuard(_mutex);

	_objectMap.clear();
}
