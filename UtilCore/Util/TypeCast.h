
//***************************************************************************
// TypeCast.h : interface for the Custom Type Casting System.
//
// @details
// TypeList를 기반으로 한 컴파일 타임 타입 변환 및 RTTI 대체 시스템입니다.
// 런타임에 dynamic_cast 대신 정적 테이블 조회를 사용하여 타입 캐스팅 성능을
// 최적화하며, 원시 포인터와 스마트 포인터(shared_ptr) 모두를 지원합니다.
//
// @note [언제 사용해야 이점이 있는가?]
//  1. 표준 RTTI(`dynamic_cast`) 성능 오버헤드가 부담될 때
//     - 컴파일 옵션으로 RTTI를 끄거나(`-fno-rtti`), 대규모 객체 계층 구조에서 
//       빈번한 `dynamic_cast`로 인한 성능 저하를 방지하고 싶을 때 유용합니다.
//  2. 사용자 정의형 고속 다운캐스팅/업캐스팅이 필요할 때
//     - 런타임 타입 검사를 정적 테이블 조회(`O(1)`)와 `static_cast`로 대체하여 
//       안전성을 유지하면서도 속도를 극대화할 수 있습니다.
//  3. 클래스 계층 구조가 TypeList로 정적으로 관리될 때
//     - 게임 엔진의 오브젝트 시스템이나 컴포넌트 시스템처럼 상속받는 타입들이 
//       사전에 명확히 정의되어 있고 고정되어 있는 경우에 적합합니다.
//************************************************---------------------------

#ifndef __TYPECAST_H__
#define __TYPECAST_H__

#ifndef	__BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

//************************************************---------------------------
// @struct TypeList
// @brief 컴파일 타임에 여러 타입을 묶어서 관리하기 위한 가변 템플릿 리스트입니다.
//************************************************---------------------------
template<typename... T>
struct TypeList;

template<typename T, typename U>
struct TypeList<T, U>
{
	using Head = T;
	using Tail = U;
};

template<typename T, typename... U>
struct TypeList<T, U...>
{
	using Head = T;
	using Tail = TypeList<U...>;
};

//************************************************---------------------------
// @struct Length
// @brief TypeList에 포함된 전체 타입의 개수를 컴파일 타임에 계산합니다.
//************************************************---------------------------
template<typename T>
struct Length;

template<>
struct Length<TypeList<>>
{
	enum { value = 0 };
};

template<typename T, typename... U>
struct Length<TypeList<T, U...>>
{
	enum { value = 1 + Length<TypeList<U...>>::value };
};

//************************************************---------------------------
// @struct TypeAt
// @brief TypeList 내에서 지정된 인덱스에 위치한 타입을 컴파일 타임에 조회합니다.
//************************************************---------------------------
template<typename TL, int32 index>
struct TypeAt;

template<typename Head, typename... Tail>
struct TypeAt<TypeList<Head, Tail...>, 0>
{
	using Result = Head;
};

template<typename Head, typename... Tail, int32 index>
struct TypeAt<TypeList<Head, Tail...>, index>
{
	using Result = typename TypeAt<TypeList<Tail...>, index - 1>::Result;
};

//************************************************---------------------------
// @struct IndexOf
// @brief TypeList 내에서 특정 타입의 인덱스를 컴파일 타임에 검색합니다.
//************************************************---------------------------
template<typename TL, typename T>
struct IndexOf;

template<typename... Tail, typename T>
struct IndexOf<TypeList<T, Tail...>, T>
{
	enum { value = 0 };
};

template<typename T>
struct IndexOf<TypeList<>, T>
{
	enum { value = -1 };
};

template<typename Head, typename... Tail, typename T>
struct IndexOf<TypeList<Head, Tail...>, T>
{
private:
	enum { temp = IndexOf<TypeList<Tail...>, T>::value };

public:
	enum { value = (temp == -1) ? -1 : temp + 1 };
};

//************************************************---------------------------
// @class Conversion
// @brief 두 타입 간의 상속 및 암시적 변환 가능 여부를 컴파일 타임에 판별합니다.
//************************************************---------------------------
template<typename From, typename To>
class Conversion
{
private:
	using Small = __int8;
	using Big = __int32;

	static Small Test(const To&) { return 0; }
	static Big Test(...) { return 0; }
	static From MakeFrom() { return 0; }

public:
	enum
	{
		exists = sizeof(Test(MakeFrom())) == sizeof(Small)
	};
};

template<int32 v>
struct Int2Type
{
	enum { value = v };
};

//************************************************---------------------------
// @class TypeConversion
// @brief TypeList에 등록된 모든 타입 조합 간의 변환 가능 여부를 캐싱하는 매트릭스 클래스입니다.
//************************************************---------------------------
template<typename TL>
class TypeConversion
{
public:
	enum
	{
		length = Length<TL>::value
	};

	TypeConversion()
	{
		MakeTable(Int2Type<0>(), Int2Type<0>());
	}

	template<int32 i, int32 j>
	static void MakeTable(Int2Type<i>, Int2Type<j>)
	{
		using FromType = typename TypeAt<TL, i>::Result;
		using ToType = typename TypeAt<TL, j>::Result;

		if( Conversion<const FromType*, const ToType*>::exists )
			s_convert[i][j] = true;
		else
			s_convert[i][j] = false;

		MakeTable(Int2Type<i>(), Int2Type<j + 1>());
	}

	template<int32 i>
	static void MakeTable(Int2Type<i>, Int2Type<length>)
	{
		MakeTable(Int2Type<i + 1>(), Int2Type<0>());
	}

	template<int j>
	static void MakeTable(Int2Type<length>, Int2Type<j>)
	{
	}

	//************************************************---------------------------
	// @brief 두 타입 ID 간의 캐스팅 가능 여부를 반환합니다.
	// @param from 원본 타입의 인덱스 ID
	// @param to 대상 타입의 인덱스 ID
	// @return true: 캐스팅 가능, false: 불가능
	//************************************************---------------------------
	static inline bool CanConvert(int32 from, int32 to)
	{
		static TypeConversion conversion;
		return s_convert[from][to];
	}

public:
	static bool s_convert[length][length];
};

template<typename TL>
bool TypeConversion<TL>::s_convert[length][length];

//************************************************---------------------------
// @brief 원시 포인터에 대한 안전한 타입 다운/업 캐스팅을 수행합니다.
// @param ptr 캐스팅할 원본 객체 포인터
// @return 캐스팅된 대상 포인터 (실패 시 nullptr)
//************************************************---------------------------
template<typename To, typename From>
To TypeCast(From* ptr)
{
	if( ptr == nullptr )
		return nullptr;

	using TL = typename From::TL;

	if( TypeConversion<TL>::CanConvert(ptr->_typeId, IndexOf<TL, remove_pointer_t<To>>::value) )
		return static_cast<To>(ptr);

	return nullptr;
}

//************************************************---------------------------
// @brief 스마트 포인터(shared_ptr)에 대한 안전한 타입 캐스팅을 수행합니다.
// @param ptr 캐스팅할 원본 스마트 포인터
// @return 캐스팅된 대상 스마트 포인터 (실패 시 빈 shared_ptr)
//************************************************---------------------------
template<typename To, typename From>
shared_ptr<To> TypeCast(shared_ptr<From> ptr)
{
	if( ptr == nullptr )
		return nullptr;

	using TL = typename From::TL;

	if( TypeConversion<TL>::CanConvert(ptr->_typeId, IndexOf<TL, remove_pointer_t<To>>::value) )
		return static_pointer_cast<To>(ptr);

	return nullptr;
}

//************************************************---------------------------
// @brief 원시 포인터의 타입 캐스팅 가능 여부를 확인합니다.
// @param ptr 확인할 원본 객체 포인터
// @return true: 캐스팅 가능, false: 불가능
//************************************************---------------------------
template<typename To, typename From>
bool CanCast(From* ptr)
{
	if( ptr == nullptr )
		return false;

	using TL = typename From::TL;
	return TypeConversion<TL>::CanConvert(ptr->_typeId, IndexOf<TL, remove_pointer_t<To>>::value);
}

//************************************************---------------------------
// @brief 스마트 포인터(shared_ptr)의 타입 캐스팅 가능 여부를 확인합니다.
// @param ptr 확인할 원본 스마트 포인터
// @return true: 캐스팅 가능, false: 불가능
//************************************************---------------------------
template<typename To, typename From>
bool CanCast(shared_ptr<From> ptr)
{
	if( ptr == nullptr )
		return false;

	using TL = typename From::TL;
	return TypeConversion<TL>::CanConvert(ptr->_typeId, IndexOf<TL, remove_pointer_t<To>>::value);
}

#define DECLARE_TL		using TL = TL; int32 _typeId;
#define INIT_TL(Type)	_typeId = IndexOf<TL, Type>::value;

#endif // ndef __TYPECAST_H__