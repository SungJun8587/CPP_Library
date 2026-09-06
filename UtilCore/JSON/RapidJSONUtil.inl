
//***************************************************************************
// RapidJSONUtil.inl : implementation of the CRapidJSONUtil class.
//
//***************************************************************************

//***************************************************************************
// @brief 지정한 키와 객체를 JSON 문자열로 직렬화합니다.
// @param key JSON 문서에 저장될 키 이름
// @param obj 직렬화할 C++ 객체 (기본 자료형, STL 컨테이너, 사용자 정의 객체 등)
// @param pretty 출력 시 보기 좋게 들여쓰기(포맷팅)를 적용할지 여부
// @return 직렬화된 결과 JSON 문자열
// @detail 템플릿 타입을 컴파일 타임에 판별(`if constexpr`)하여 기본 자료형, 벡터, 맵, 그리고 ToJSON 메서드를 지원하는 사용자 정의 객체에 맞는 적절한 직렬화 함수를 호출합니다.
//         지원하지 않는 타입이 전달되면 컴파일 타임에 static_assert로 에러를 발생시켜
//         조용히 아무 것도 하지 않고 빈 결과를 반환하는 것을 방지합니다.
//***************************************************************************
template <typename T>
inline _tstring CRapidJSONUtil::Serialize(const _tstring& key, const T& obj, const bool pretty)
{
	using CleanT = std::decay_t<T>;

	if constexpr( is_vector<CleanT>::value )
	{
		using ValueType = typename CleanT::value_type;
		if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
		{
			AddVector<CleanT, ValueType>(key, obj);        // 기본 자료형 벡터
		}
		else
		{
			AddObjectVector<CleanT, ValueType>(key, obj);  // 사용자 정의 클래스 벡터
		}
	}
	else if constexpr( is_map<CleanT>::value )
	{
		// 맵 처리
		using KeyType = typename CleanT::key_type;
		using ValueType = typename CleanT::mapped_type;
		if constexpr( std::is_same_v<KeyType, _tstring> )
		{
			// 키가 _tstring인 경우만 처리
			if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
			{
				AddMap(key, obj);            // 기본 자료형 맵
			}
			else
			{
				AddObjectMap(key, obj);      // 사용자 정의 클래스 맵
			}
		}
		else
		{
			static_assert(dependent_false<CleanT>::value, "Serialize: map key type must be _tstring");
		}
	}
	else if constexpr( has_tojson_method<CleanT>::value )
	{
		AddObject(key, obj);
	}
	else if constexpr( std::is_arithmetic_v<CleanT> )
	{
		AddValue(key, obj);
	}
	else if constexpr( std::is_same_v<CleanT, TCHAR*> || std::is_same_v<CleanT, const TCHAR*> )
	{
		AddValue(key, obj);
	}
	else if constexpr( std::is_same_v<CleanT, _tstring> )
	{
		AddValue(key, obj);
	}
	else
	{
		static_assert(dependent_false<CleanT>::value, "Serialize: unsupported type");
	}

	return ToString(pretty);
}

//***************************************************************************
// @brief 지정한 키의 JSON 데이터를 특정 C++ 타입으로 역직렬화합니다.
// @param key 가져올 값이 위치한 JSON 문서의 키 이름
// @return 역직렬화된 C++ 객체 (지정된 타입 T)
// @detail 템플릿 타입 T를 분석하여 기본 자료형, 컨테이너(벡터, 맵), 또는 FromJSON 메서드를 지원하는 사용자 정의 객체 형태로 JSON 데이터를 파싱하여 반환합니다.
//         지원하지 않는 타입이 전달되면 컴파일 타임에 static_assert로 에러를 발생시킵니다.
//***************************************************************************
template <typename T>
inline T CRapidJSONUtil::Deserialize(const _tstring& key) const
{
	using CleanT = std::decay_t<T>;

	if constexpr( is_vector<CleanT>::value )
	{
		using ValueType = typename CleanT::value_type;
		if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
		{
			return GetVector<CleanT, ValueType>(key);         // 기본 자료형 벡터
		}
		else
		{
			return GetObjectVector<CleanT, ValueType>(key);   // 사용자 정의 클래스 벡터
		}
	}
	else if constexpr( is_map<CleanT>::value )
	{
		// 맵 처리
		using KeyType = typename CleanT::key_type;
		using ValueType = typename CleanT::mapped_type;
		if constexpr( std::is_same_v<KeyType, _tstring> )
		{
			// 키가 _tstring인 경우만 처리
			if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
			{
				return GetMap<CleanT>(key);            // 기본 자료형 맵
			}
			else
			{
				return GetObjectMap<CleanT>(key);      // 사용자 정의 클래스 맵
			}
		}
		else
		{
			static_assert(dependent_false<CleanT>::value, "Deserialize: map key type must be _tstring");
		}
	}
	else if constexpr( has_tojson_method<CleanT>::value )
	{
		return GetObject<CleanT>(key);
	}
	else if constexpr( std::is_arithmetic_v<CleanT> )
	{
		return GetValue<CleanT>(key, 0);
	}
	else if constexpr( std::is_same_v<CleanT, TCHAR*> || std::is_same_v<CleanT, const TCHAR*> )
	{
		return GetValue<CleanT>(key, _T(""));
	}
	else if constexpr( std::is_same_v<CleanT, _tstring> )
	{
		return GetValue<CleanT>(key, _T(""));
	}
	else
	{
		static_assert(dependent_false<CleanT>::value, "Deserialize: unsupported type");
	}

	return T();
}

//***************************************************************************
// @brief JSON 문서에 단일 값을 멤버로 설정합니다.
// @param key 설정할 JSON 멤버의 키 이름
// @param value 설정할 데이터 값
// @return 없음
// @detail 동일 키의 멤버가 이미 존재하면 먼저 제거한 뒤, 전달된 값을 JSON 지원 타입으로
//         변환(`ConvertToJSONValue`)하여 새 멤버로 추가합니다. 이 방식으로 "추가"가 아닌
//         "설정(덮어쓰기)" 의미를 가지므로, operator[]/Proxy를 통해 같은 키에 반복 대입해도
//         중복 멤버가 생기지 않습니다.
//***************************************************************************
template <typename T>
inline void CRapidJSONUtil::AddValue(const _tstring& key, const T& value)
{
	RemoveMemberIfExists(key);
	auto& allocator = _document.GetAllocator();
	_tValue jsonKey(key.c_str(), (rapidjson::SizeType)key.length(), allocator);
	_document.AddMember(jsonKey, ConvertToJSONValue(value), allocator);
}

//***************************************************************************
// @brief JSON 문서 내의 특정 멤버 값을 갱신하거나, 존재하지 않을 경우 새로 추가합니다.
// @param key 갱신할 JSON 멤버의 키 이름
// @param value 새로 설정할 데이터 값
// @return 없음
// @detail AddValue()가 이미 "존재하면 제거 후 추가"하는 안전한 설정 동작을 수행하므로
//         그대로 위임합니다. (기존 코드는 `_document[key].~_tValue()`로 소멸자를 직접 호출한 뒤
//         operator=로 대입하여, operator= 내부에서 소멸자가 다시 호출되는 이중 소멸 UB가 있었습니다.)
//***************************************************************************
template <typename T>
inline void CRapidJSONUtil::UpdateValue(const _tstring& key, const T& value)
{
	AddValue(key, value);
}

//***************************************************************************
// @brief JSON 배열에 새로운 요소를 추가합니다. (기존 배열이 없으면 생성)
// @param key 요소를 추가할 JSON 배열의 키 이름
// @param value 배열에 추가할 데이터 값
// @return 없음
// @detail 지정한 키의 배열이 아직 존재하지 않는 경우 빈 배열 객체를 먼저 생성한 뒤, 해당 배열의 끝에 새로운 값을 푸시합니다.
//         FindMember로 한 번만 탐색한 이터레이터를 재사용하여 존재 확인과 값 접근을 함께 처리합니다.
//         키가 이미 존재하지만 배열이 아닌 값(예: 이전에 AddValue로 저장된 정수/문자열 등)일 경우,
//         rapidjson의 PushBack()은 내부적으로 IsArray()를 전제하므로 그대로 호출하면 릴리스
//         빌드에서 정의되지 않은 동작이 발생할 수 있습니다. 이를 막기 위해 배열이 아니면
//         기존 값을 정리하고 빈 배열로 재설정한 뒤 추가합니다(기존 값은 버려짐).
//***************************************************************************
template <typename T>
inline void CRapidJSONUtil::AddArray(const _tstring& key, const T& value)
{
	auto& allocator = _document.GetAllocator();
	auto itr = _document.FindMember(key.c_str());
	if( itr == _document.MemberEnd() )
	{
		_tValue jsonArray(rapidjson::kArrayType);
		_document.AddMember(_tValue(key.c_str(), (rapidjson::SizeType)key.length(), allocator), jsonArray, allocator);
		itr = _document.FindMember(key.c_str());
	}
	else if( !itr->value.IsArray() )
	{
		RecursiveRemove(itr->value);
		itr->value.SetArray();
	}
	itr->value.PushBack(ConvertToJSONValue(value), allocator);
}

//***************************************************************************
// @brief JSON 배열의 특정 인덱스에 위치한 요소를 수정합니다.
// @param key 수정할 JSON 배열의 키 이름
// @param index 값을 변경할 배열의 인덱스 위치
// @param value 새로 대입할 데이터 값
// @return 없음
// @detail 지정한 키가 존재하고 배열 타입이며, 인덱스가 배열 크기 범위 내에 있는 경우에만 값을 안전하게 갱신합니다.
//         FindMember로 한 번만 탐색한 이터레이터를 재사용합니다.
//***************************************************************************
template <typename T>
inline void CRapidJSONUtil::UpdateArrayAt(const _tstring& key, uint32 index, const T& value)
{
	auto itr = _document.FindMember(key.c_str());
	if( itr != _document.MemberEnd() && itr->value.IsArray() && index < itr->value.Size() )
	{
		itr->value[index] = ConvertToJSONValue(value);
	}
}

//***************************************************************************
// @brief JSON 문서에서 단일 기본 데이터를 가져옵니다.
// @param key 가져올 데이터의 JSON 키 이름
// @param defaultValue 해당 키가 존재하지 않을 때 반환할 기본값
// @return 파싱된 데이터 또는 키가 없을 경우 defaultValue
// @detail FindMember로 한 번만 탐색하여 존재 여부 확인과 값 변환(`ConvertFromJSONValue`)을 함께 처리합니다.
//***************************************************************************
template <typename T>
inline T CRapidJSONUtil::GetValue(const _tstring& key, const T& defaultValue) const
{
	auto itr = _document.FindMember(key.c_str());
	if( itr != _document.MemberEnd() && IsConvertible<T>(itr->value) )
	{
		return ConvertFromJSONValue<T>(itr->value);
	}
	return defaultValue;
}

//***************************************************************************
// @brief 지정한 _tValue가 T 타입으로 안전하게 변환 가능한지 검사합니다.
// @detail rapidjson의 GetInt()/GetString() 등은 실제 타입이 다를 경우 릴리스 빌드에서
//         어서션 없이 정의되지 않은 동작을 일으킬 수 있습니다. GetValue/GetVector/GetMap 등이
//         ConvertFromJSONValue를 호출하기 전에 이 함수로 실제 JSON 타입을 먼저 검증하여,
//         타입이 맞지 않는 경우 defaultValue/빈 값으로 안전하게 폴백할 수 있도록 합니다.
//         ToJSON/FromJSON을 사용하는 사용자 정의 타입, 벡터, 맵의 경우 요소 단위 검증은
//         각 GetObject/FromJSON 구현에 위임하고 여기서는 컨테이너 타입 여부만 확인합니다.
//***************************************************************************
template <typename T>
inline bool CRapidJSONUtil::IsConvertible(const _tValue& value) const
{
	using CleanT = std::decay_t<T>;

	if constexpr( std::is_same_v<CleanT, int16> || std::is_same_v<CleanT, int32> )
	{
		return value.IsInt();
	}
	else if constexpr( std::is_same_v<CleanT, uint16> || std::is_same_v<CleanT, uint32> )
	{
		return value.IsUint();
	}
	else if constexpr( std::is_same_v<CleanT, int64> )
	{
		return value.IsInt64();
	}
	else if constexpr( std::is_same_v<CleanT, uint64> )
	{
		return value.IsUint64();
	}
	else if constexpr( std::is_same_v<CleanT, float> || std::is_same_v<CleanT, double> )
	{
		return value.IsNumber();
	}
	else if constexpr( std::is_same_v<CleanT, bool> )
	{
		return value.IsBool();
	}
	else if constexpr( std::is_same_v<CleanT, const TCHAR*> || std::is_same_v<CleanT, _tstring> )
	{
		return value.IsString();
	}
	else if constexpr( is_vector<CleanT>::value )
	{
		return value.IsArray();
	}
	else if constexpr( is_map<CleanT>::value )
	{
		return value.IsObject();
	}
	else
	{
		// ToJSON/FromJSON 기반 사용자 정의 타입 등은 여기서 일반적으로 판별할 수 없으므로
		// 통과시키고, 실패 시의 안전성은 해당 타입의 FromJSON 구현에 맡깁니다.
		return true;
	}
}

//***************************************************************************
// @brief 사용자 정의 클래스 객체를 JSON 객체 멤버로 직렬화하여 설정합니다.
// @param key JSON 문서에 저장될 객체의 키 이름
// @param object `ToJSON` 메서드를 지원하는 사용자 정의 클래스 객체
// @return 없음
// @detail 동일 키가 이미 존재하면 먼저 제거하여 중복 멤버 생성을 방지한 뒤, 새로운 JSON 객체를
//         생성하여 `ToJSON` 메서드로 멤버를 채우고 문서에 추가합니다.
//***************************************************************************
template <typename T>
inline void CRapidJSONUtil::AddObject(const _tstring& key, const T& object)
{
	RemoveMemberIfExists(key);

	auto& allocator = _document.GetAllocator();
	_tValue jsonObject(rapidjson::kObjectType);
	object.ToJSON(jsonObject, allocator);				// 사용자 정의 객체의 ToJSON 호출

	_tValue jsonKey(key.c_str(), (rapidjson::SizeType)key.length(), allocator);
	_document.AddMember(jsonKey, jsonObject, allocator);
}

//***************************************************************************
// @brief JSON 문서로부터 사용자 정의 클래스 객체를 역직렬화하여 가져옵니다.
// @param key 가져올 객체의 JSON 키 이름
// @return 역직렬화된 사용자 정의 클래스 객체 (실패 시 기본 생성된 객체 반환)
// @detail 지정한 키가 존재할 경우, 해당 JSON 값을 객체의 `FromJSON` 메서드에 전달하여 필드를 복원합니다.
//***************************************************************************
template <typename T>
inline T CRapidJSONUtil::GetObject(const _tstring& key) const
{
	T obj;

	auto itr = _document.FindMember(key.c_str());
	if( itr != _document.MemberEnd() )
	{
		obj.FromJSON(itr->value);		// 사용자 정의 객체의 FromJSON 호출
	}

	return obj;
}

//***************************************************************************
// @brief 기본 데이터 타입의 벡터를 JSON 배열로 직렬화하여 설정합니다.
// @param key JSON 배열로 저장될 키 이름
// @param vec 직렬화할 기본 자료형 벡터 (`CVector<T>` 또는 `std::vector<T>`)
// @return 없음
// @detail 동일 키가 이미 존재하면 먼저 제거하여 중복 멤버 생성을 방지한 뒤, 벡터의 각 요소를
//         순회하며 JSON 값으로 변환하여 하나의 JSON 배열에 담아 문서에 등록합니다.
//***************************************************************************
template <typename Container, typename ValueType>
inline void CRapidJSONUtil::AddVector(const _tstring& key, const Container& vec)
{
	RemoveMemberIfExists(key);

	auto& allocator = _document.GetAllocator();
	_tValue jsonArray(rapidjson::kArrayType);
	for( const auto& item : vec )
	{
		jsonArray.PushBack(ConvertToJSONValue(item), allocator);
	}
	_tValue jsonKey(key.c_str(), (rapidjson::SizeType)key.length(), allocator);
	_document.AddMember(jsonKey, jsonArray, allocator);
}

//***************************************************************************
// @brief JSON 배열로부터 기본 데이터 타입 벡터를 역직렬화하여 가져옵니다.
// @param key 가져올 JSON 배열의 키 이름
// @return 복원된 기본 데이터 타입 벡터 (`CVector<T>` 또는 `std::vector<T>`)
// @detail 지정한 키가 유효한 배열인지 검사한 후, 배열의 각 요소를 순회하며 지정된 타입으로 변환하여 결과 벡터에 담아 반환합니다.
//***************************************************************************
template <typename Container, typename ValueType>
inline Container CRapidJSONUtil::GetVector(const _tstring& key) const
{
	Container result;
	auto itr = _document.FindMember(key.c_str());
	if( itr == _document.MemberEnd() || !itr->value.IsArray() )
	{
		return result;
	}
	const auto& jsonArray = itr->value;
	for( rapidjson::SizeType i = 0; i < jsonArray.Size(); ++i )
	{
		// 배열 내 개별 원소의 실제 타입이 ValueType과 다를 수 있으므로(예: 혼합 타입 배열,
		// 손상된 입력) 원소마다 검증 후 변환합니다. 불일치 원소는 조용히 건너뜁니다.
		if( IsConvertible<ValueType>(jsonArray[i]) )
		{
			result.push_back(ConvertFromJSONValue<ValueType>(jsonArray[i]));
		}
	}
	return result;
}

//***************************************************************************
// @brief 사용자 정의 객체 벡터를 JSON 배열로 직렬화하여 설정합니다.
// @param key JSON 배열로 저장될 키 이름
// @param vec 직렬화할 사용자 정의 객체 벡터 (`CVector<T>` 또는 `std::vector<T>`)
// @return 없음
// @detail 동일 키가 이미 존재하면 먼저 제거하여 중복 멤버 생성을 방지한 뒤, 벡터 내 각 객체의
//         `ToJSON` 메서드를 호출해 JSON 객체로 변환하고 배열에 누적하여 문서에 추가합니다.
//***************************************************************************
template <typename Container, typename ValueType>
inline void CRapidJSONUtil::AddObjectVector(const _tstring& key, const Container& vec)
{
	RemoveMemberIfExists(key);

	auto& allocator = _document.GetAllocator();
	_tValue jsonArray(rapidjson::kArrayType);
	for( const auto& item : vec )
	{
		_tValue jsonObject(rapidjson::kObjectType);
		item.ToJSON(jsonObject, allocator);			// 사용자 정의 객체의 ToJSON 호출
		jsonArray.PushBack(jsonObject, allocator);
	}

	_tValue jsonKey(key.c_str(), (rapidjson::SizeType)key.length(), allocator);
	_document.AddMember(jsonKey, jsonArray, allocator);
}

//***************************************************************************
// @brief JSON 배열로부터 사용자 정의 객체 벡터를 역직렬화하여 가져옵니다.
// @param key 가져올 JSON 배열의 키 이름
// @return 복원된 사용자 정의 객체 벡터 (`CVector<T>` 또는 `std::vector<T>`)
// @detail 지정한 배열의 각 원소(JSON 객체)에 대해 객체를 생성하고 `FromJSON` 메서드를 호출하여 상태를 복원한 뒤 벡터에 추가합니다.
//***************************************************************************
template <typename Container, typename ValueType>
inline Container CRapidJSONUtil::GetObjectVector(const _tstring& key) const
{
	Container result;
	auto itr = _document.FindMember(key.c_str());
	if( itr == _document.MemberEnd() || !itr->value.IsArray() )
	{
		return result;
	}

	const auto& jsonArray = itr->value;
	for( rapidjson::SizeType i = 0; i < jsonArray.Size(); ++i )
	{
		const _tValue& jsonObject = jsonArray[i];

		ValueType obj;
		obj.FromJSON(jsonObject);	// 사용자 정의 객체의 FromJSON 호출
		result.push_back(obj);
	}
	return result;
}

//***************************************************************************
// @brief 기본 데이터 타입 맵을 JSON 객체로 직렬화하여 설정합니다.
// @param key JSON 객체로 저장될 키 이름
// @param map 직렬화할 맵 데이터 (`CMap<Key, Value>` 또는 `std::map<Key, Value>`)
// @return 없음
// @detail 동일 키가 이미 존재하면 먼저 제거하여 중복 멤버 생성을 방지한 뒤, 맵의 모든 키-값
//         쌍을 순회하며 각각 JSON 값으로 변환하여 하나의 JSON 객체 멤버로 추가합니다.
//***************************************************************************
template <typename MapContainer>
inline void CRapidJSONUtil::AddMap(const _tstring& key, const MapContainer& map)
{
	RemoveMemberIfExists(key);

	auto& allocator = _document.GetAllocator();
	_tValue jsonObject(rapidjson::kObjectType);

	for( const auto& [mapKey, mapValue] : map )
	{
		// ConvertToJSONValue()는 이미 이 _document의 allocator로 만들어진 임시(rvalue)를
		// 반환하므로, (rvalue, allocator) 2-인자 생성자(딥카피)가 아니라 1-인자 이동
		// 생성자를 타도록 하여 불필요한 재할당/복사를 피합니다.
		_tValue jsonKey(ConvertToJSONValue(mapKey));
		_tValue jsonValue(ConvertToJSONValue(mapValue));
		jsonObject.AddMember(jsonKey, jsonValue, allocator);
	}

	_tValue jsonKey(key.c_str(), (rapidjson::SizeType)key.length(), allocator);
	_document.AddMember(jsonKey, jsonObject, allocator);
}

//***************************************************************************
// @brief JSON 객체로부터 기본 데이터 타입 맵을 역직렬화하여 가져옵니다.
// @param key 가져올 JSON 객체의 키 이름
// @return 복원된 기본 데이터 타입 맵 (`CMap<Key, Value>` 또는 `std::map<Key, Value>`)
// @detail 대상이 유효한 JSON 객체인지 확인한 후, 내부 멤버들을 순회하며 키와 값을 각각 매칭하여 맵 형태로 복원합니다.
//***************************************************************************
template <typename MapContainer>
inline MapContainer CRapidJSONUtil::GetMap(const _tstring& key) const
{
	MapContainer result;
	using KeyType = typename MapContainer::key_type;
	using ValueType = typename MapContainer::mapped_type;

	auto itr = _document.FindMember(key.c_str());
	if( itr == _document.MemberEnd() || !itr->value.IsObject() )
	{
		return result;
	}

	const auto& obj = itr->value;
	for( auto it = obj.MemberBegin(); it != obj.MemberEnd(); ++it )
	{
		// 값의 실제 타입이 ValueType과 다른 항목은 건너뛰어 GetInt()/GetString() 등의
		// 정의되지 않은 동작을 방지합니다. 키는 rapidjson 상 항상 문자열이므로 검증 불필요.
		if( IsConvertible<ValueType>(it->value) )
		{
			KeyType mapKey = ConvertFromJSONValue<KeyType>(it->name);
			ValueType mapValue = ConvertFromJSONValue<ValueType>(it->value);
			result[mapKey] = mapValue;
		}
	}

	return result;
}

//***************************************************************************
// @brief 사용자 정의 객체 맵을 JSON 객체로 직렬화하여 설정합니다.
// @param key JSON 객체로 저장될 키 이름
// @param map 직렬화할 사용자 정의 객체 맵 (`CMap<Key, T>` 또는 `std::map<Key, T>`)
// @return 없음
// @detail 동일 키가 이미 존재하면 먼저 제거하여 중복 멤버 생성을 방지한 뒤, 맵의 각 항목에
//         대해 값을 JSON 객체로 변환하고 `ToJSON` 메서드를 호출하여 채운 뒤 추가합니다.
//***************************************************************************
template <typename MapContainer>
inline void CRapidJSONUtil::AddObjectMap(const _tstring& key, const MapContainer& map)
{
	RemoveMemberIfExists(key);

	auto& allocator = _document.GetAllocator();
	_tValue jsonObject(rapidjson::kObjectType);

	for( const auto& [mapKey, mapValue] : map )
	{
		// AddMap과 동일한 이유로 딥카피 생성자 대신 이동 생성자를 사용합니다.
		_tValue jsonMapKey(ConvertToJSONValue(mapKey));
		_tValue jsonMapValue(rapidjson::kObjectType);

		mapValue.ToJSON(jsonMapValue, allocator);					// T 타입의 ToJSON 호출
		jsonObject.AddMember(jsonMapKey, jsonMapValue, allocator);
	}

	_tValue jsonKey(key.c_str(), (rapidjson::SizeType)key.length(), allocator);
	_document.AddMember(jsonKey, jsonObject, allocator);
}

//***************************************************************************
// @brief JSON 객체로부터 사용자 정의 객체 맵을 역직렬화하여 가져옵니다.
// @param key 가져올 JSON 객체의 키 이름
// @return 복원된 사용자 정의 객체 맵 (`CMap<Key, T>` 또는 `std::map<Key, T>`)
// @detail JSON 객체의 모든 멤버를 순회하며 키를 파싱하고, 값에 대해서는 `FromJSON` 메서드를 호출하여 객체를 복원한 뒤 맵에 삽입합니다.
//***************************************************************************
template <typename MapContainer>
inline MapContainer CRapidJSONUtil::GetObjectMap(const _tstring& key) const
{
	MapContainer result;
	using KeyType = typename MapContainer::key_type;
	using ValueType = typename MapContainer::mapped_type;

	auto itr = _document.FindMember(key.c_str());
	if( itr == _document.MemberEnd() || !itr->value.IsObject() )
	{
		return result;
	}

	const auto& obj = itr->value;
	for( auto it = obj.MemberBegin(); it != obj.MemberEnd(); ++it )
	{
		KeyType mapKey = ConvertFromJSONValue<KeyType>(it->name);

		ValueType mapValue;
		mapValue.FromJSON(it->value);			// T 타입의 FromJSON 호출
		result.emplace(mapKey, mapValue);
	}

	return result;
}

//***************************************************************************
// @brief C++의 다양한 타입을 RapidJSON에서 다루는 `_tValue` 타입으로 변환합니다.
// @param value 변환할 원본 C++ 데이터 값
// @return RapidJSON용 값 객체 (`_tValue`)
// @detail 컴파일 타임 조건문(`if constexpr`)을 사용하여 정수, 실수, 부울, 문자열, 컨테이너 등의 타입을 판별하고 각각에 알맞은 RapidJSON 생성자를 호출합니다.
//***************************************************************************
template <typename T>
inline _tValue CRapidJSONUtil::ConvertToJSONValue(const T& value)
{
	if constexpr( std::is_same<T, int16>::value )
	{
		return _tValue(value);
	}
	else if constexpr( std::is_same<T, uint16>::value )
	{
		return _tValue(value);
	}
	else if constexpr( std::is_same<T, int32>::value )
	{
		return _tValue(value);
	}
	else if constexpr( std::is_same<T, uint32>::value )
	{
		return _tValue(value);
	}
	else if constexpr( std::is_same<T, int64>::value )
	{
		return _tValue(value);
	}
	else if constexpr( std::is_same<T, uint64>::value )
	{
		return _tValue(value);
	}
	else if constexpr( std::is_same<T, float>::value )
	{
		return _tValue(value);
	}
	else if constexpr( std::is_same<T, double>::value )
	{
		return _tValue(value);
	}
	else if constexpr( std::is_same<T, bool>::value )
	{
		return _tValue(value);
	}
	else if constexpr( std::is_same_v<typename std::decay<T>::type, TCHAR*> || std::is_same_v<typename std::decay<T>::type, const TCHAR*> )
	{
		return _tValue(value, _document.GetAllocator());
	}
	else if constexpr( std::is_same_v<T, _tstring> )
	{
		return _tValue(value.c_str(), (rapidjson::SizeType)value.length(), _document.GetAllocator());
	}
	else if constexpr( is_vector<T>::value )
	{
		auto& allocator = _document.GetAllocator();
		_tValue array(rapidjson::kArrayType);
		for( const auto& elem : value )
		{
			array.PushBack(ConvertToJSONValue(elem), allocator);
		}
		return array;
	}
	else if constexpr( is_map<T>::value )
	{
		auto& allocator = _document.GetAllocator();
		_tValue obj(rapidjson::kObjectType);
		for( const auto& pair : value )
		{
			_tValue k(pair.first.c_str(), (rapidjson::SizeType)pair.first.length(), allocator);
			obj.AddMember(k, ConvertToJSONValue(pair.second), allocator);
		}
		return obj;
	}
	else
	{
		static_assert(dependent_false<T>::value, "Unsupported type for serialization");
	}
}

//***************************************************************************
// @brief RapidJSON의 `_tValue` 객체를 지정된 C++ 타입으로 변환합니다.
// @tparam T 변환하고자 하는 대상 C++ 타입
// @param value 변환할 원본 RapidJSON 값 객체
// @return 변환된 C++ 데이터 값
// @detail 컴파일 타임 조건문(`if constexpr`)을 활용하여 `_tValue`로부터 적절한 추출 함수(`GetInt`, `GetString`, 컨테이너 순회 등)를 호출해 데이터를 반환합니다.
// @warning T가 (const) TCHAR*인 경우 반환되는 포인터는 내부 rapidjson 문서 버퍼를 직접
//          가리킵니다. 이 CRapidJSONUtil 객체가 소멸되거나 해당 값이 변경/삭제되면
//          포인터는 즉시 무효화(dangling)됩니다. 값을 보관해야 한다면 반드시 T=_tstring으로
//          호출하여 복사본을 받으십시오. 비-const TCHAR*는 애초에 GetString()의 반환 타입과
//          호환되지 않으므로 컴파일 타임에 막습니다.
//***************************************************************************
template <typename T>
inline T CRapidJSONUtil::ConvertFromJSONValue(const _tValue& value) const
{
	if constexpr( std::is_same_v<T, TCHAR*> )
	{
		static_assert(dependent_false<T>::value,
			"ConvertFromJSONValue<TCHAR*> is not allowed (dangling risk / GetString() returns const TCHAR*). "
			"Use _tstring or const TCHAR* instead.");
	}
	else if constexpr( std::is_same<T, int16>::value )
	{
		return value.GetInt();
	}
	else if constexpr( std::is_same<T, uint16>::value )
	{
		return value.GetUint();
	}
	else if constexpr( std::is_same<T, int32>::value )
	{
		return value.GetInt();
	}
	else if constexpr( std::is_same<T, uint32>::value )
	{
		return value.GetUint();
	}
	else if constexpr( std::is_same<T, int64>::value )
	{
		return value.GetInt64();
	}
	else if constexpr( std::is_same<T, uint64>::value )
	{
		return value.GetUint64();
	}
	else if constexpr( std::is_same<T, float>::value )
	{
		return value.GetFloat();
	}
	else if constexpr( std::is_same<T, double>::value )
	{
		return value.GetDouble();
	}
	else if constexpr( std::is_same<T, bool>::value )
	{
		return value.GetBool();
	}
	else if constexpr( std::is_same_v<typename std::decay<T>::type, const TCHAR*> )
	{
		return value.GetString();
	}
	else if constexpr( std::is_same_v<T, _tstring> )
	{
		return value.GetString();
	}
	else if constexpr( is_vector<T>::value )
	{
		T result;
		for( const auto& elem : value.GetArray() )
		{
			// 중첩된 컨테이너(예: vector<vector<int>>, map<string, vector<int>>)를 역직렬화할 때도
			// GetValue/GetVector와 동일한 안전장치를 적용하여, 손상되거나 혼합 타입인 원소에서
			// GetInt()/GetString() 등이 정의되지 않은 동작을 일으키는 것을 방지합니다.
			if( IsConvertible<typename T::value_type>(elem) )
			{
				result.push_back(ConvertFromJSONValue<typename T::value_type>(elem));
			}
		}
		return result;
	}
	else if constexpr( is_map<T>::value )
	{
		T result;
		for( auto it = value.MemberBegin(); it != value.MemberEnd(); ++it )
		{
			if( IsConvertible<typename T::mapped_type>(it->value) )
			{
				result[it->name.GetString()] = ConvertFromJSONValue<typename T::mapped_type>(it->value);
			}
		}
		return result;
	}
	else
	{
		static_assert(dependent_false<T>::value, "Unsupported type for deserialization");
	}
}
