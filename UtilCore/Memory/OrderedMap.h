
//***************************************************************************
// OrderedMap.h : interface for the COrderedMap class.
//
//***************************************************************************

#ifndef __ORDEREDMAP_H__
#define __ORDEREDMAP_H__

template<typename T1, typename T2>
class COrderedMap : public CPoolObj
{
public:
	typedef std::map<T1, T2>				ObjectMap;
	typedef typename ObjectMap::iterator	ObjectMapIter;

public:
	COrderedMap(void);
	virtual	~COrderedMap(void);

	bool				InsertObject(T1& key, T2& object);
	bool				InsertAndUpdateObject(T1& key, T2& object);

	ObjectMapIter		FindObject(T1& key);
	bool				FindObject(T1& key, T2& object);

	bool				EraseObject(T1& key);

	ObjectMap& GetObjectMap(void) {
		return _objectMap;
	}
	std::shared_mutex& GetLock(void) {
		return _mutex;
	}
	int					GetSize();
	ObjectMapIter		GetEnd();
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

#include "OrderedMap.inl"

#endif // ndef __ORDEREDMAP_H__