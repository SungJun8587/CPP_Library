
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
		return m_ObjectMap;
	}
	CCriticalSection& GetLock(void) {
		return m_Lock;
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
	ObjectMap			m_ObjectMap;
	CCriticalSection	m_Lock;
};

#endif // ndef __ORDEREDMAP_H__