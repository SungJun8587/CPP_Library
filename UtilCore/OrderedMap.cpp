
//***************************************************************************
// OrderedMap.cpp : implementation of the COrderedMap class.
//
//***************************************************************************

#include "pch.h"
#include "OrderedMap.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

template<typename T1, typename T2>
COrderedMap<T1, T2>::COrderedMap(void)
{
	clearObjectMap();
}

template<typename T1, typename T2>
COrderedMap<T1, T2>::~COrderedMap(void)
{
	clearObjectMap();
}

template<typename T1, typename T2>
typename COrderedMap<T1, T2>::ObjectMapIter COrderedMap<T1, T2>::GetEnd()
{
	CLockGuard<CCriticalSection> lockGuard(m_Lock);
	return m_ObjectMap.end();
}

//***************************************************************************
//
template<typename T1, typename T2>
int32 COrderedMap<T1, T2>::GetSize()
{
	CLockGuard<CCriticalSection> lockGuard(m_Lock);
	return static_cast<int32>(m_ObjectMap.size());
}

//***************************************************************************
//
template<typename T1, typename T2>
bool COrderedMap<T1, T2>::IsEmpty()
{
	CLockGuard<CCriticalSection> lockGuard(m_Lock);
	return m_ObjectMap.empty();
}

//***************************************************************************
//
template<typename T1, typename T2>
bool COrderedMap<T1, T2>::InsertObject(T1& key, T2& object)
{
	CLockGuard<CCriticalSection> lockGuard(m_Lock);
	auto rst = m_ObjectMap.insert(ObjectMap::value_type(key, object));
	return rst.second;
}

//***************************************************************************
//
template<typename T1, typename T2>
bool COrderedMap<T1, T2>::InsertAndUpdateObject(T1& key, T2& object)
{
	CLockGuard<CCriticalSection> lockGuard(m_Lock);
	auto iter = m_ObjectMap.find(key);
	if( iter == m_ObjectMap.end() )
	{
		auto rst = m_ObjectMap.insert(ObjectMap::value_type(key, object));
		return rst.second;
	}

	memcpy(iter->second, object, sizeof(T2));

	return true;
}

//***************************************************************************
//
template<typename T1, typename T2>
typename COrderedMap<T1, T2>::ObjectMapIter COrderedMap<T1, T2>::FindObject(T1& key)
{
	CLockGuard<CCriticalSection> lockGuard(m_Lock);
	return m_ObjectMap.find(key);
}

//***************************************************************************
//
template<typename T1, typename T2>
bool COrderedMap<T1, T2>::FindObject(T1& key, T2& object)
{
	CLockGuard<CCriticalSection> lockGuard(m_Lock);
	auto iter = m_ObjectMap.find(key);
	if( iter == m_ObjectMap.end() )
		return false;

	object = iter->second;
	return true;
}

//***************************************************************************
//
template<typename T1, typename T2>
bool COrderedMap<T1, T2>::EraseObject(T1& key)
{
	CLockGuard<CCriticalSection> lockGuard(m_Lock);
	auto iter = m_ObjectMap.find(key);
	if( iter == m_ObjectMap.end() )
		return false;

	m_ObjectMap.erase(iter);
	return true;
}

//***************************************************************************
//
template<typename T1, typename T2>
void COrderedMap<T1, T2>::clearObjectMap(void)
{
	CLockGuard<CCriticalSection> lockGuard(m_Lock);
	m_ObjectMap.clear();
}
