
//***************************************************************************
//
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
				AddMap(key, obj);           // 기본 자료형 맵
			}
			else
			{
				AddObjectMap(key, obj);     // 사용자 정의 클래스 맵
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
	else if constexpr( std::is_same_v<T, _tstring> )
	{
		AddValue(key, obj);
	}

	return ToString(pretty);
}

//***************************************************************************
//
template <typename T>
inline T CRapidJSONUtil::Deserialize(const _tstring& key)
{
	if constexpr( is_vector<T>::value )
	{
		using ValueType = typename T::value_type;
		if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
		{
			return GetVector<ValueType>(key);        // 기본 자료형 벡터
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
				return GetMap<KeyType, ValueType>(key);           // 기본 자료형 맵
			}
			else
			{
				return GetObjectMap<KeyType, ValueType>(key);     // 사용자 정의 클래스 맵
			}
		}
	}
	else if constexpr( has_tojson_method<T>::value )
	{
		return GetObject<T>(key);
	}
	else if constexpr( std::is_arithmetic<T>::value )
	{
		return GetValue<T>(key);
	}
	else if constexpr( std::is_same_v<T, _tstring> )
	{
		return GetValue<T>(key);
	}
}

//***************************************************************************
// 객체 추가
template <typename T>
inline void CRapidJSONUtil::AddValue(const _tstring& key, const T& value)
{
	_tValue jsonKey(key.c_str(), _allocator);
	_document.AddMember(jsonKey, ConvertToJSONValue(value), _allocator);
}

//***************************************************************************
// 객체 업데이트
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
// 배열 추가
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
// 배열 수정
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
// 데이터 가져오기
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
// 사용자 정의 객체 추가(직렬화)
template <typename T>
inline void CRapidJSONUtil::AddObject(const _tstring& key, const T& object)
{
	_tValue jsonObject(rapidjson::kObjectType);
	object.ToJSON(jsonObject, _allocator);				// 사용자 정의 객체의 ToJSON 호출

	_tValue jsonKey(key.c_str(), _allocator);
	_document.AddMember(jsonKey, jsonObject, _allocator);
}

//***************************************************************************
// 사용자 정의 객체 가져오기(역직렬화)
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
// 기본 데이터 벡터 추가(직렬화)
template <typename T>
inline void CRapidJSONUtil::AddVector(const _tstring& key, const std::vector<T>& vec)
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
// 기본 데이터 벡터 가져오기(역직렬화)
template <typename T>
inline std::vector<T> CRapidJSONUtil::GetVector(const _tstring& key)
{
	std::vector<T> result;
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
// 사용자 정의 객체 벡터 추가(직렬화)
template <typename T>
inline void CRapidJSONUtil::AddObjectVector(const _tstring& key, const std::vector<T>& vec)
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
// 사용자 정의 객체 벡터 가져오기(역직렬화)
template <typename T>
inline std::vector<T> CRapidJSONUtil::GetObjectVector(const _tstring& key)
{
	std::vector<T> result;
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
// 기본 데이터 맵 추가(직렬화)
template <typename Key, typename Value>
inline void CRapidJSONUtil::AddMap(const _tstring& key, const std::map<Key, Value>& map)
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
// 기본 데이터 맵 가져오기(역직렬화)
template <typename Key, typename Value>
inline std::map<Key, Value> CRapidJSONUtil::GetMap(const _tstring& key) const
{
	std::map<Key, Value> result;

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
// 사용자 정의 객체 맵 추가(직렬화)
template <typename Key, typename T>
inline void CRapidJSONUtil::AddObjectMap(const _tstring& key, const std::map<Key, T>& map)
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
// 사용자 정의 객체 맵 가져오기(역직렬화)
template <typename Key, typename T>
inline std::map<Key, T> CRapidJSONUtil::GetObjectMap(const _tstring& key) const
{
	std::map<Key, T> result;

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
// 타입별 JSON 값 변환
template <typename T>
inline _tValue CRapidJSONUtil::ConvertToJSONValue(const T& value) const
{
	if constexpr( std::is_same<T, int32_t>::value )
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
	else if constexpr( std::is_same_v<T, std::string> )
	{
		return _tValue(value.c_str(), _allocator);
	}
	else if constexpr( std::is_same_v<T, std::wstring> )
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
// JSON 값 -> C++ 객체 변환
template <typename T>
inline T CRapidJSONUtil::ConvertFromJSONValue(const _tValue& value) const
{
	if constexpr( std::is_same<T, int32_t>::value )
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
	else if constexpr( std::is_same_v<T, std::string> )
	{
		return value.GetString();
	}
	else if constexpr( std::is_same_v<T, std::wstring> )
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