
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
//***************************************************************************
template <typename T>
inline _tstring CRapidJSONUtil::Serialize(const _tstring& key, const T& obj, const bool pretty)
{
	if constexpr( is_vector<T>::value )
	{
		using ValueType = typename T::value_type;
		if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
		{
			AddVector(key, obj);        // 기본 자료형 벡터
		}
		else
		{
			AddObjectVector(key, obj);  // 사용자 정의 클래스 벡터
		}
	}
	else if constexpr( is_map<T>::value )
	{
		// 맵 처리
		using KeyType = typename T::key_type;
		using ValueType = typename T::mapped_type;
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
	}
	else if constexpr( has_tojson_method<T>::value )
	{
		AddObject(key, obj);
	}
	else if constexpr( std::is_arithmetic<T>::value )
	{
		AddValue(key, obj);
	}
	else if constexpr( std::is_same_v<typename std::decay<T>::type, TCHAR*> || std::is_same_v<typename std::decay<T>::type, const TCHAR*> )
	{
		AddValue(key, obj);
	}
	else if constexpr( std::is_same_v<T, _tstring> )
	{
		AddValue(key, obj);
	}

	return ToString(pretty);
}

//***************************************************************************
// @brief 지정한 키의 JSON 데이터를 특정 C++ 타입으로 역직렬화합니다.
// @param key 가져올 값이 위치한 JSON 문서의 키 이름
// @return 역직렬화된 C++ 객체 (지정된 타입 T)
// @detail 템플릿 타입 T를 분석하여 기본 자료형, 컨테이너(벡터, 맵), 또는 FromJSON 메서드를 지원하는 사용자 정의 객체 형태로 JSON 데이터를 파싱하여 반환합니다.
//***************************************************************************
template <typename T>
inline T CRapidJSONUtil::Deserialize(const _tstring& key)
{
	if constexpr( is_vector<T>::value )
	{
		using ValueType = typename T::value_type;
		if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
		{
			return GetVector<ValueType>(key);         // 기본 자료형 벡터
		}
		else
		{
			return GetObjectVector<ValueType>(key);  // 사용자 정의 클래스 벡터
		}
	}
	else if constexpr( is_map<T>::value )
	{
		// 맵 처리
		using KeyType = typename T::key_type;
		using ValueType = typename T::mapped_type;
		if constexpr( std::is_same_v<KeyType, _tstring> )
		{
			// 키가 _tstring인 경우만 처리
			if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
			{
				return GetMap<KeyType, ValueType>(key);            // 기본 자료형 맵
			}
			else
			{
				return GetObjectMap<KeyType, ValueType>(key);      // 사용자 정의 클래스 맵
			}
		}
	}
	else if constexpr( has_tojson_method<T>::value )
	{
		return GetObject<T>(key);
	}
	else if constexpr( std::is_arithmetic<T>::value )
	{
		return GetValue<T>(key, 0);
	}
	else if constexpr( std::is_same_v<typename std::decay<T>::type, TCHAR*> || std::is_same_v<typename std::decay<T>::type, const TCHAR*> )
	{
		return GetValue<T>(key, _T(""));
	}
	else if constexpr( std::is_same_v<T, _tstring> )
	{
		return GetValue<T>(key, _T(""));
	}

	return T();
}

//***************************************************************************
// @brief JSON 문서에 단일 값을 멤버로 추가합니다.
// @param key 추가할 JSON 멤버의 키 이름
// @param value 추가할 데이터 값
// @return 없음
// @detail 전달된 값을 JSON 지원 타입으로 변환(`ConvertToJSONValue`)한 뒤, 내부 문서 객체에 새로운 멤버로 추가합니다.
//***************************************************************************
template <typename T>
inline void CRapidJSONUtil::AddValue(const _tstring& key, const T& value)
{
	_tValue jsonKey(key.c_str(), _allocator);
	_document.AddMember(jsonKey, ConvertToJSONValue(value), _allocator);
}

//***************************************************************************
// @brief JSON 문서 내의 특정 멤버 값을 갱신하거나, 존재하지 않을 경우 새로 추가합니다.
// @param key 갱신할 JSON 멤버의 키 이름
// @param value 새로 설정할 데이터 값
// @return 없음
// @detail 이미 해당 키가 존재하면 기존 값을 소멸시킨 뒤 새로운 값으로 교체하고, 존재하지 않는 경우 `AddValue`를 호출하여 새로 추가합니다.
//***************************************************************************
template <typename T>
inline void CRapidJSONUtil::UpdateValue(const _tstring& key, const T& value)
{
	if( _document.HasMember(key.c_str()) )
	{
		_document[key.c_str()].~_tValue();
		_document[key.c_str()] = ConvertToJSONValue(value);
	}
	else
	{
		AddValue(key, value);
	}
}

//***************************************************************************
// @brief JSON 배열에 새로운 요소를 추가합니다. (기존 배열이 없으면 생성)
// @param key 요소를 추가할 JSON 배열의 키 이름
// @param value 배열에 추가할 데이터 값
// @return 없음
// @detail 지정한 키의 배열이 아직 존재하지 않는 경우 빈 배열 객체를 먼저 생성한 뒤, 해당 배열의 끝에 새로운 값을 푸시합니다.
//***************************************************************************
template <typename T>
inline void CRapidJSONUtil::AddArray(const _tstring& key, const T& value)
{
	if( !_document.HasMember(key.c_str()) )
	{
		_tValue jsonArray(rapidjson::kArrayType);
		_document.AddMember(_tValue(key.c_str(), _allocator), jsonArray, _allocator);
	}
	_document[key.c_str()].PushBack(ConvertToJSONValue(value), _allocator);
}

//***************************************************************************
// @brief JSON 배열의 특정 인덱스에 위치한 요소를 수정합니다.
// @param key 수정할 JSON 배열의 키 이름
// @param index 값을 변경할 배열의 인덱스 위치
// @param value 새로 대입할 데이터 값
// @return 없음
// @detail 지정한 키가 존재하고 배열 타입이며, 인덱스가 배열 크기 범위 내에 있는 경우에만 값을 안전하게 갱신합니다.
//***************************************************************************
template <typename T>
inline void CRapidJSONUtil::UpdateArrayAt(const _tstring& key, uint32 index, const T& value)
{
	if( _document.HasMember(key.c_str()) && _document[key.c_str()].IsArray() )
	{
		if( index < _document[key.c_str()].Size() )
		{
			_document[key.c_str()][index] = ConvertToJSONValue(value);
		}
	}
}

//***************************************************************************
// @brief JSON 문서에서 단일 기본 데이터를 가져옵니다.
// @param key 가져올 데이터의 JSON 키 이름
// @param defaultValue 해당 키가 존재하지 않을 때 반환할 기본값
// @return 파싱된 데이터 또는 키가 없을 경우 defaultValue
// @detail 문서 내에 해당 키가 존재하는지 확인한 후, 값을 요청한 타입으로 변환(`ConvertFromJSONValue`)하여 반환합니다.
//***************************************************************************
template <typename T>
inline T CRapidJSONUtil::GetValue(const _tstring& key, const T& defaultValue) const
{
	if( _document.HasMember(key.c_str()) )
	{
		return ConvertFromJSONValue<T>(_document[key.c_str()]);
	}
	return defaultValue;
}

//***************************************************************************
// @brief 사용자 정의 클래스 객체를 JSON 객체 멤버로 직렬화하여 추가합니다.
// @param key JSON 문서에 저장될 객체의 키 이름
// @param object `ToJSON` 메서드를 지원하는 사용자 정의 클래스 객체
// @return 없음
// @detail 새로운 JSON 객체를 생성한 후 객체 내부의 `ToJSON` 메서드를 호출해 멤버를 채우고, 이를 문서에 추가합니다.
//***************************************************************************
template <typename T>
inline void CRapidJSONUtil::AddObject(const _tstring& key, const T& object)
{
	_tValue jsonObject(rapidjson::kObjectType);
	object.ToJSON(jsonObject, _allocator);				// 사용자 정의 객체의 ToJSON 호출

	_tValue jsonKey(key.c_str(), _allocator);
	_document.AddMember(jsonKey, jsonObject, _allocator);
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

	if( _document.HasMember(key.c_str()) )
	{
		const _tValue& jsonValue = _document[key.c_str()];
		obj.FromJSON(jsonValue);		// 사용자 정의 객체의 FromJSON 호출
		return obj;
	}

	return obj;
}

//***************************************************************************
// @brief 기본 데이터 타입의 벡터를 JSON 배열로 직렬화하여 추가합니다.
// @param key JSON 배열로 저장될 키 이름
// @param vec 직렬화할 기본 자료형 벡터 (`CVector<T>`)
// @return 없음
// @detail 벡터의 각 요소를 순회하며 JSON 값으로 변환한 뒤, 하나의 JSON 배열에 추가하여 문서에 등록합니다.
//***************************************************************************
template <typename T>
inline void CRapidJSONUtil::AddVector(const _tstring& key, const CVector<T>& vec)
{
	_tValue jsonArray(rapidjson::kArrayType);
	for( const auto& item : vec )
	{
		jsonArray.PushBack(ConvertToJSONValue(item), _allocator);
	}
	_tValue jsonKey(key.c_str(), _allocator);
	_document.AddMember(jsonKey, jsonArray, _allocator);
}

//***************************************************************************
// @brief JSON 배열로부터 기본 데이터 타입 벡터를 역직렬화하여 가져옵니다.
// @param key 가져올 JSON 배열의 키 이름
// @return 복원된 기본 데이터 타입 벡터 (`CVector<T>`)
// @detail 지정한 키가 유효한 배열인지 검사한 후, 배열의 각 요소를 순회하며 지정된 타입으로 변환하여 결과 벡터에 담아 반환합니다.
//***************************************************************************
template <typename T>
inline CVector<T> CRapidJSONUtil::GetVector(const _tstring& key)
{
	CVector<T> result;
	if( !_document.HasMember(key.c_str()) || !_document[key.c_str()].IsArray() )
	{
		return result;
	}
	const auto& jsonArray = _document[key.c_str()];
	for( rapidjson::SizeType i = 0; i < jsonArray.Size(); ++i )
	{
		result.push_back(ConvertFromJSONValue<T>(jsonArray[i]));
	}
	return result;
}

//***************************************************************************
// @brief 사용자 정의 객체 벡터를 JSON 배열로 직렬화하여 추가합니다.
// @param key JSON 배열로 저장될 키 이름
// @param vec 직렬화할 사용자 정의 객체 벡터 (`CVector<T>`)
// @return 없음
// @detail 벡터 내 각 객체의 `ToJSON` 메서드를 호출해 JSON 객체로 변환하고, 이를 배열에 누적한 뒤 문서에 추가합니다.
//***************************************************************************
template <typename T>
inline void CRapidJSONUtil::AddObjectVector(const _tstring& key, const CVector<T>& vec)
{
	_tValue jsonArray(rapidjson::kArrayType);
	for( const auto& item : vec )
	{
		_tValue jsonObject(rapidjson::kObjectType);
		item.ToJSON(jsonObject, _allocator);			// 사용자 정의 객체의 ToJSON 호출
		jsonArray.PushBack(jsonObject, _allocator);
	}

	_tValue jsonKey(key.c_str(), _allocator);
	_document.AddMember(jsonKey, jsonArray, _allocator);
}

//***************************************************************************
// @brief JSON 배열로부터 사용자 정의 객체 벡터를 역직렬화하여 가져옵니다.
// @param key 가져올 JSON 배열의 키 이름
// @return 복원된 사용자 정의 객체 벡터 (`CVector<T>`)
// @detail 지정한 배열의 각 원소(JSON 객체)에 대해 객체를 생성하고 `FromJSON` 메서드를 호출하여 상태를 복원한 뒤 벡터에 추가합니다.
//***************************************************************************
template <typename T>
inline CVector<T> CRapidJSONUtil::GetObjectVector(const _tstring& key)
{
	CVector<T> result;
	if( !_document.HasMember(key.c_str()) || !_document[key.c_str()].IsArray() )
	{
		return result;
	}

	const auto& jsonArray = _document[key.c_str()];
	for( rapidjson::SizeType i = 0; i < jsonArray.Size(); ++i )
	{
		const _tValue& jsonObject = jsonArray[i];

		T obj;
		obj.FromJSON(jsonObject);	// 사용자 정의 객체의 FromJSON 호출
		result.push_back(obj);
	}
	return result;
}

//***************************************************************************
// @brief 기본 데이터 타입 맵을 JSON 객체로 직렬화하여 추가합니다.
// @param key JSON 객체로 저장될 키 이름
// @param map 직렬화할 맵 데이터 (`CMap<Key, Value>`)
// @return 없음
// @detail 맵의 모든 키-값 쌍을 순회하며 각각 JSON 값으로 변환 후, 하나의 JSON 객체 멤버로 추가하여 문서에 등록합니다.
//***************************************************************************
template <typename Key, typename Value>
inline void CRapidJSONUtil::AddMap(const _tstring& key, const CMap<Key, Value>& map)
{
	_tValue jsonObject(rapidjson::kObjectType);

	for( const auto& [mapKey, mapValue] : map )
	{
		_tValue jsonKey(ConvertToJSONValue(mapKey), _allocator);
		_tValue jsonValue(ConvertToJSONValue(mapValue), _allocator);
		jsonObject.AddMember(jsonKey, jsonValue, _allocator);
	}

	_tValue jsonKey(key.c_str(), _allocator);
	_document.AddMember(jsonKey, jsonObject, _allocator);
}

//***************************************************************************
// @brief JSON 객체로부터 기본 데이터 타입 맵을 역직렬화하여 가져옵니다.
// @param key 가져올 JSON 객체의 키 이름
// @return 복원된 기본 데이터 타입 맵 (`CMap<Key, Value>`)
// @detail 대상이 유효한 JSON 객체인지 확인한 후, 내부 멤버들을 순회하며 키와 값을 각각 매칭하여 맵 형태로 복원합니다.
//***************************************************************************
template <typename Key, typename Value>
inline CMap<Key, Value> CRapidJSONUtil::GetMap(const _tstring& key) const
{
	CMap<Key, Value> result;

	if( !_document.HasMember(key.c_str()) || !_document[key.c_str()].IsObject() )
	{
		return result;
	}

	const auto& obj = _document[key.c_str()];
	for( auto it = obj.MemberBegin(); it != obj.MemberEnd(); ++it )
	{
		Key mapKey = ConvertFromJSONValue<Key>(it->name);
		Value mapValue = ConvertFromJSONValue<Value>(it->value);
		result[mapKey] = mapValue;
	}

	return result;
}

//***************************************************************************
// @brief 사용자 정의 객체 맵을 JSON 객체로 직렬화하여 추가합니다.
// @param key JSON 객체로 저장될 키 이름
// @param map 직렬화할 사용자 정의 객체 맵 (`CMap<Key, T>`)
// @return 없음
// @detail 맵의 각 항목에 대해 값을 JSON 객체로 변환하고 `ToJSON` 메서드를 호출하여 채운 뒤, 최종 JSON 객체에 추가합니다.
//***************************************************************************
template <typename Key, typename T>
inline void CRapidJSONUtil::AddObjectMap(const _tstring& key, const CMap<Key, T>& map)
{
	_tValue jsonObject(rapidjson::kObjectType);

	for( const auto& [mapKey, mapValue] : map )
	{
		_tValue jsonMapKey(ConvertToJSONValue(mapKey), _allocator);
		_tValue jsonMapValue(rapidjson::kObjectType);

		mapValue.ToJSON(jsonMapValue, _allocator);					// T 타입의 ToJSON 호출
		jsonObject.AddMember(jsonMapKey, jsonMapValue, _allocator);
	}

	_tValue jsonKey(key.c_str(), _allocator);
	_document.AddMember(jsonKey, jsonObject, _allocator);
}

//***************************************************************************
// @brief JSON 객체로부터 사용자 정의 객체 맵을 역직렬화하여 가져옵니다.
// @param key 가져올 JSON 객체의 키 이름
// @return 복원된 사용자 정의 객체 맵 (`CMap<Key, T>`)
// @detail JSON 객체의 모든 멤버를 순회하며 키를 파싱하고, 값에 대해서는 `FromJSON` 메서드를 호출하여 객체를 복원한 뒤 맵에 삽입합니다.
//***************************************************************************
template <typename Key, typename T>
inline CMap<Key, T> CRapidJSONUtil::GetObjectMap(const _tstring& key) const
{
	CMap<Key, T> result;

	if( !_document.HasMember(key.c_str()) || !_document[key.c_str()].IsObject() )
	{
		return result;
	}

	const auto& obj = _document[key.c_str()];
	for( auto it = obj.MemberBegin(); it != obj.MemberEnd(); ++it )
	{
		Key mapKey = ConvertFromJSONValue<Key>(it->name);

		T mapValue;
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
inline _tValue CRapidJSONUtil::ConvertToJSONValue(const T& value) const
{
	if constexpr( std::is_same<T, int16_t>::value )
	{
		return _tValue(value);
	}
	else if constexpr( std::is_same<T, uint16_t>::value )
	{
		return _tValue(value);
	}
	else if constexpr( std::is_same<T, int32_t>::value )
	{
		return _tValue(value);
	}
	else if constexpr( std::is_same<T, uint32_t>::value )
	{
		return _tValue(value);
	}
	else if constexpr( std::is_same<T, int64_t>::value )
	{
		return _tValue(value);
	}
	else if constexpr( std::is_same<T, uint64_t>::value )
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
		return _tValue(value, _allocator);
	}
	else if constexpr( std::is_same_v<T, _tstring> )
	{
		return _tValue(value.c_str(), _allocator);
	}
	else if constexpr( is_vector<T>::value )
	{
		_tValue array(rapidjson::kArrayType);
		for( const auto& elem : value )
		{
			array.PushBack(ConvertToJSONValue(elem), _allocator);
		}
		return array;
	}
	else if constexpr( is_map<T>::value )
	{
		_tValue obj(rapidjson::kObjectType);
		for( const auto& pair : value )
		{
			_tValue k(pair.first.c_str(), _allocator);
			obj.AddMember(k, ConvertToJSONValue(pair.second), _allocator);
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
//***************************************************************************
template <typename T>
inline T CRapidJSONUtil::ConvertFromJSONValue(const _tValue& value) const
{
	if constexpr( std::is_same<T, int16_t>::value )
	{
		return value.GetInt();
	}
	else if constexpr( std::is_same<T, uint16_t>::value )
	{
		return value.GetUint();
	}
	else if constexpr( std::is_same<T, int32_t>::value )
	{
		return value.GetInt();
	}
	else if constexpr( std::is_same<T, uint32_t>::value )
	{
		return value.GetUint();
	}
	else if constexpr( std::is_same<T, int64_t>::value )
	{
		return value.GetInt64();
	}
	else if constexpr( std::is_same<T, uint64_t>::value )
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
	else if constexpr( std::is_same_v<typename std::decay<T>::type, TCHAR*> || std::is_same_v<typename std::decay<T>::type, const TCHAR*> )
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
			result.push_back(ConvertFromJSONValue<typename T::value_type>(elem));
		}
		return result;
	}
	else if constexpr( is_map<T>::value )
	{
		T result;
		for( auto it = value.MemberBegin(); it != value.MemberEnd(); ++it )
		{
			result[it->name.GetString()] = ConvertFromJSONValue<typename T::mapped_type>(it->value);
		}
		return result;
	}
	else
	{
		static_assert(dependent_false<T>::value, "Unsupported type for deserialization");
	}
}