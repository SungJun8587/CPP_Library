//***************************************************************************
//	
template<typename EDataType, typename _CONVERT_FUNC, typename _DEF_VAL>
EDataType CJSONParser::ConvertToNumber(_CONVERT_FUNC pFunc, _DEF_VAL tDefValue) const
{
    if( true == m_pValue->IsString() )
    {
        return pFunc(m_pValue->GetString());
    }
    else if( true == m_pValue->IsInt() )
    {
        return (EDataType)m_pValue->GetInt();
    }
    else if( true == m_pValue->IsInt64() )
    {
        return (EDataType)m_pValue->GetInt64();
    }
    else if( true == m_pValue->IsUint() )
    {
        return (EDataType)m_pValue->GetUint();
    }
    else if( true == m_pValue->IsUint64() )
    {
        return (EDataType)m_pValue->GetUint64();
    }
    else if( true == m_pValue->IsFloat() )
    {
        return (EDataType)m_pValue->GetFloat();
    }
    else if( true == m_pValue->IsDouble() )
    {
        return (EDataType)m_pValue->GetDouble();
    }
    else if( true == m_pValue->IsBool() )
    {
        return (EDataType)m_pValue->GetBool();
    }
    else
    {
        return tDefValue;
    }
}

//***************************************************************************
//	
template< typename EDataType >
bool CJSONParser::Add(TCHAR* ptszNodePath, EDataType data)
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
template< typename EDataType >
bool CJSONParser::Update(TCHAR* ptszNodePath, EDataType data)
{
	_tValue* value;
	if( (value = _tPointer(ptszNodePath).Get(*m_pDocument)) == 0 )
	{
		return false;
	}
	value->Set(*m_pDocument, data);

	return true;
}

//***************************************************************************
//	
template< typename EDataType >
_tstring CJSONParser::SerializeToJson(const EDataType& data, bool bIsPretty)
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
template< typename EDataType >
EDataType CJSONParser::Deserialize(_tStringStream& stream)
{
	_tDocument document;
	document.ParseStream(stream);

	if( RAPIDJSON_UNLIKELY(document.HasParseError()) )
	{
		throw std::invalid_argument{ "Could not parse JSON document." };
	}

	static_assert(
		std::is_default_constructible<EDataType>::value,
		"The container must have a default constructor.");

	EDataType container;
	dom_deserializer::from_json(document, container);

	return container;
}

//***************************************************************************
//
template< typename EDataType >
EDataType CJSONParser::DeserializeViaDom(const TCHAR* const json)
{
	_tStringStream string_stream{ json };

	return Deserialize<EDataType>(string_stream);
}

//***************************************************************************
//
template< typename EDataType >
EDataType CJSONParser::DeserializeViaDom(const _tstring& json)
{
	return DeserializeViaDom<EDataType>(json.c_str());
}

#if __cplusplus >= 201703L

//***************************************************************************
//
template< typename EDataType >
void CJSONParser::SerializeToJson(const EDataType& data, const std::filesystem::path& path, bool bIsPretty)
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
template< typename EDataType >
EDataType CJSONParser::DeserializeViaDom(const std::filesystem::path& path)
{
	_tofstream file_stream{ path };
	_tStreamWrapper stream_wrapper{ file_stream };

	return Deserialize<EDataType>(stream_wrapper);
}

//***************************************************************************
//
template< typename EDataType >
EDataType CJSONParser::DeserializeViaSax(const TCHAR* const json)
{
	return sax_deserializer::detail::from_json<EDataType>(json);
}

//***************************************************************************
//
template< typename EDataType >
EDataType CJSONParser::DeserializeViaSax(const _tstring& json)
{
	return sax_deserializer::detail::from_json<EDataType>(json.c_str());
}

//***************************************************************************
//
template< typename EDataType >
EDataType CJSONParser::DeserializeViaSax(const std::filesystem::path& path)
{
	return sax_deserializer::detail::from_json<EDataType>(path);
}

#endif



