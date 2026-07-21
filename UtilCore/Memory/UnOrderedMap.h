
//***************************************************************************
// UnOrderedMap.h : interface for the CUnOrderedMap class.
//
//***************************************************************************

#ifndef __UNORDEREDMAP_H__
#define __UNORDEREDMAP_H__

#ifndef	__ALLOCATOR_H__
#include <Memory/Allocator.h>
#endif

#include <unordered_map>
#include <shared_mutex>

template<typename T1, typename T2>
class CUnOrderedMap : public BaseAllocator
{
public:
	typedef std::unordered_map<T1, T2> ObjectMap;

public:
	CUnOrderedMap(void);
	virtual	~CUnOrderedMap(void);

	bool				InsertObject(const T1& key, const T2& object);
	bool				InsertAndUpdateObject(const T1& key, const T2& object);

	bool				FindObject(const T1& key, T2& outObject);
	bool				EraseObject(const T1& key);

	int32				GetSize();
	bool				IsEmpty();

	void				Clear(void) {
		clearObjectMap();
	}

protected:
	void				clearObjectMap(void);

protected:
	ObjectMap			_objectMap;
	std::shared_mutex	_mutex;
};

#include "UnOrderedMap.inl"

#endif // ndef __UNORDEREDMAP_H__