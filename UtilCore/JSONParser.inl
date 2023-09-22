//***************************************************************************
//	
template<typename DataType, typename _CONVERT_FUNC, typename _DEF_VAL>
DataType CJSONParser::ConvertToNumber(_CONVERT_FUNC pFunc, _DEF_VAL tDefValue) const
{
    if( true == m_pValue->IsString() )
    {
        return pFunc(m_pValue->GetString());
    }
    else if( true == m_pValue->IsInt() )
    {
        return (DataType)m_pValue->GetInt();
    }
    else if( true == m_pValue->IsInt64() )
    {
        return (DataType)m_pValue->GetInt64();
    }
    else if( true == m_pValue->IsUint() )
    {
        return (DataType)m_pValue->GetUint();
    }
    else if( true == m_pValue->IsUint64() )
    {
        return (DataType)m_pValue->GetUint64();
    }
    else if( true == m_pValue->IsFloat() )
    {
        return (DataType)m_pValue->GetFloat();
    }
    else if( true == m_pValue->IsDouble() )
    {
        return (DataType)m_pValue->GetDouble();
    }
    else if( true == m_pValue->IsBool() )
    {
        return (DataType)m_pValue->GetBool();
    }
    else
    {
        return tDefValue;
    }
}

//***************************************************************************
//	
template< typename DataType >
BOOL CJSONParser::Add(TCHAR* ptszNodePath, DataType data)
{
	_tValue* value;
	if( (value = _tPointer(ptszNodePath).Get(*m_pDocument)) == 0 )
	{
		_tPointer(ptszNodePath).Set(*m_pDocument, data);
		return true;
	}

	return false;
}

//***************************************************************************
//	
template< typename DataType >
BOOL CJSONParser::Update(TCHAR* ptszNodePath, DataType data)
{
	_tValue* value;
	if( (value = _tPointer(ptszNodePath).Get(*m_pDocument)) == 0 )
	{
		return false;
	}
	value.Set(*m_pDocument, data);

	return true;
}

//***************************************************************************
//	
template< typename DataType >
_tstring CJSONParser::SerializeToJson(const DataType& data, BOOL bIsPretty)
{
	_tStringBuffer buffer;
	buffer.Clear();

	if( bIsPretty )
	{
#ifdef _UNICODE
		PrettyWriter<WStringBuffer, UTF16<>, UTF16<> > writer(buffer);
#else
		PrettyWriter<StringBuffer, UTF8<>, UTF8<>> writer(buffer);
#endif		
		serializer::to_json(writer, data);
	}
	else
	{
#ifdef _UNICODE
		Writer<WStringBuffer, UTF16<>, UTF16<> > writer(buffer);
#else
		Writer<StringBuffer, UTF8<>, UTF8<>> writer(buffer);
#endif
		serializer::to_json(writer, data);
	}

	_tstring result = _tstring(buffer.GetString(), buffer.GetSize());

	return result;
}

//***************************************************************************
//
template< typename DataType >
DataType CJSONParser::Deserialize(_tStringStream& stream)
{
	_tDocument document;
	document.ParseStream(stream);

	if( RAPIDJSON_UNLIKELY(document.HasParseError()) )
	{
		throw std::invalid_argument{ "Could not parse JSON document." };
	}

	static_assert(
		std::is_default_constructible<DataType>::value,
		"The container must have a default constructor.");

	DataType container;
	dom_deserializer::from_json(document, container);

	return container;
}

//***************************************************************************
//
template< typename DataType >
DataType CJSONParser::DeserializeViaDom(const TCHAR* const json)
{
	_tStringStream string_stream{ json };

	return Deserialize<DataType>(string_stream);
}

//***************************************************************************
//
template< typename DataType >
DataType CJSONParser::DeserializeViaDom(const _tstring& json)
{
	return DeserializeViaDom<DataType>(json.c_str());
}

#if __cplusplus >= 201703L

//***************************************************************************
//
template< typename DataType >
void CJSONParser::SerializeToJson(const DataType& data, const std::filesystem::path& path, BOOL bIsPretty)
{
	_tofstream file_stream{ path };

	_tStreamWrapper stream_wrapper{ file_stream };

	if( bIsPretty )
	{
#ifdef _UNICODE
		PrettyWriter<WOStreamWrapper, UTF16<>, UTF16<> > writer {
			stream_wrapper
		};
#else
		PrettyWriter<OStreamWrapper, UTF8<>, UTF8<>> writer {
			stream_wrapper
		};
#endif		
		serializer::to_json(writer, data);
	}
	else
	{
#ifdef _UNICODE
		Writer<WOStreamWrapper, UTF16<>, UTF16<> > writer {
			stream_wrapper
		};
#else
		Writer<OStreamWrapper, UTF8<>, UTF8<>> writer {
			stream_wrapper
		};
#endif
		serializer::to_json(writer, data);
	}
}

//***************************************************************************
//
template< typename DataType >
DataType CJSONParser::DeserializeViaDom(const std::filesystem::path& path)
{
	_tofstream file_stream{ path };
	_tStreamWrapper stream_wrapper{ file_stream };

	return Deserialize<DataType>(stream_wrapper);
}

//***************************************************************************
//
template< typename DataType >
DataType CJSONParser::DeserializeViaSax(const TCHAR* const json)
{
	return sax_deserializer::detail::from_json<DataType>(json);
}

//***************************************************************************
//
template< typename DataType >
DataType CJSONParser::DeserializeViaSax(const _tstring& json)
{
	return sax_deserializer::detail::from_json<DataType>(json.c_str());
}

//***************************************************************************
//
template< typename DataType >
DataType CJSONParser::DeserializeViaSax(const std::filesystem::path& path)
{
	return sax_deserializer::detail::from_json<DataType>(path);
}

#endif



