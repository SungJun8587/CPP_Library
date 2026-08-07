
//***************************************************************************
// UnOrderedMap.h : interface for the CUnOrderedMap class.
//
//***************************************************************************

#ifndef __UNORDEREDMAP_H__
#define __UNORDEREDMAP_H__

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef	__CONTAINERS_H__
#include <Memory/Containers.h>
#endif

#include <shared_mutex>

template<typename T1, typename T2>
class CUnOrderedMap : public BaseAllocator
{
public:
	typedef CUnorderedMap<T1, T2> ObjectMap;

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
	ObjectMap			_objectMap;    // 키-값 데이터를 순서 없이 저장하는 내부 해시맵 객체 (CUnorderedMap)
	std::shared_mutex	_mutex;        // 스레드 안전한 접근을 보장하기 위한 공유 뮤텍스 (읽기/쓰기 락 지원)
};

#include "UnOrderedMap.inl"

#endif // ndef __UNORDEREDMAP_H__