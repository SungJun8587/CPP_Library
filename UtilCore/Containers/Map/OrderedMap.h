
//***************************************************************************
// OrderedMap.h : interface for the COrderedMap class.
//
//***************************************************************************

#ifndef UC_ORDEREDMAP_H
#define UC_ORDEREDMAP_H

#include <BaseRedefineDataType.h>
#include <Memory/Containers.h>

#include <shared_mutex>

template<typename T1, typename T2>
class COrderedMap : public BaseAllocator
{
public:
	typedef CMap<T1, T2> ObjectMap;

public:
	COrderedMap(void);
	virtual	~COrderedMap(void);

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
	ObjectMap			_objectMap;    // 키-값 데이터를 저장하는 내부 맵 객체
	std::shared_mutex	_mutex;        // 스레드 안전한 접근을 보장하기 위한 공유 뮤텍스 (읽기/쓰기 락 지원)
};

#include "OrderedMap.inl"

#endif // ndef UC_ORDEREDMAP_H