
//***************************************************************************
// JSONParser.cpp: implementation of the CJSONParser class.
//
//***************************************************************************

#include "pch.h"
#include "JSONParser.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CJSONParser::CJSONParser(bool bIsDebugPrint) : m_bRoot(true), m_pDocument(new _tDocument()), m_pValue(new _tValue(kObjectType))
{
	m_bIsDebugPrint = bIsDebugPrint;
	Print_DebugInfo(_T("JsonParser() create instance\n"));
}

CJSONParser::CJSONParser(_tDocument* pDoc, _tValue* pValue) : m_bIsDebugPrint(false), m_bRoot(false), m_pDocument(pDoc), m_pValue(pValue)
{
}

CJSONParser::CJSONParser(const CJSONParser& other) : m_bIsDebugPrint(false), m_bRoot(false), m_pDocument(other.m_pDocument), m_pValue(other.m_pValue)
{
}

CJSONParser::~CJSONParser(void)
{
	if( m_bRoot )
	{ 
		delete m_pValue;
		m_pValue = nullptr;

		delete m_pDocument;
		m_pDocument = nullptr;
	}

	Print_DebugInfo(_T("JsonParser() instance destructed\n"));
}

//***************************************************************************
//
CJSONParser& CJSONParser::operator=(const CJSONParser& Other)
{
	m_pValue->CopyFrom(*Other.m_pValue, m_pDocument->GetAllocator());
	return *this;
}

//***************************************************************************
//
void CJSONParser::Print_DebugInfo(const TCHAR* ptszFormat, ...)
{
	if( !m_bIsDebugPrint ) return;

	va_list args;

	_ftprintf_s(stdout, _T("CJSONParser::"));

	va_start(args, ptszFormat);
	_vtprintf_s(ptszFormat, args);
	va_end(args);

	return;
}

//***************************************************************************
//
_tstring CJSONParser::ReadFile(const TCHAR* ptszFilePath)
{
	int iRet = 0;

#ifdef _UNICODE
	struct _stat64i32 _Stat;
	iRet = _wstat64i32(ptszFilePath, &_Stat);
#else
	struct _stat64i32 _Stat;
	iRet = _stat64i32(ptszFilePath, &_Stat);
#endif

	if( iRet != 0 )
	{
		Print_DebugInfo(_T("%s not found file path [%s]\n"), __TFUNCTION__, ptszFilePath);
		return _T("");
	}

	_tstringstream read_data;
	_tifstream fp(ptszFilePath);

	read_data << fp.rdbuf();
	fp.close();

	Print_DebugInfo(_T("%s read from [%s] - [%s]\n"), __TFUNCTION__, ptszFilePath, read_data.str().c_str());

	return read_data.str();
}

//***************************************************************************
//
bool CJSONParser::WriteFile(const TCHAR* ptszFilePath, TCHAR* ptszJsonString)
{
	_tofstream out(ptszFilePath);

	if( out.is_open() )
	{
		out << ptszJsonString;
		out.close();

		Print_DebugInfo(_T("%s write to [%s] - [%s]\n"), __TFUNCTION__, ptszFilePath, ptszJsonString);
		return true;

	}
	else
	{
		Print_DebugInfo(_T("%s open failed [%s]\n"), __TFUNCTION__, ptszFilePath);
		return false;
	}
}

//***************************************************************************
//
bool CJSONParser::WriteJson(const TCHAR* ptszFilePath)
{
	_tStringBuffer buffer;

#ifdef _UNICODE
	PrettyWriter<WStringBuffer, UTF16<>, UTF16<>> writer(buffer);
#else
	PrettyWriter<StringBuffer> writer(buffer);
#endif

	if( m_pDocument->Accept(writer) == false )
	{
		Print_DebugInfo(_T("%s open failed [%s]\n"), __TFUNCTION__, ptszFilePath);
		return false;
	}

	_tstring temp = buffer.GetString();

	_tofstream out(ptszFilePath, _tofstream::trunc);
	out << temp;

	Print_DebugInfo(_T("%s write to [%s] - [%s]\n"), __TFUNCTION__, ptszFilePath, temp.c_str());

	return true;
}

//***************************************************************************
//
bool CJSONParser::Parse(const _tstring jsonString)
{
	if( jsonString.compare(_T("")) == 0 )
	{
		//Print_DebugInfo(_T("%s JSON parse error : [%02d] %s (offset: %u)\n"), __TFUNCTION__,
		//					  kParseErrorDocumentEmpty, GetParseError_En(kParseErrorDocumentEmpty), 0);
		return false;
	}

	m_pDocument->Parse(jsonString.c_str());
	if( kParseErrorNone != m_pDocument->GetParseError() )
		return false;

	*m_pValue = m_pDocument->Move();

	Print_DebugInfo(_T("%s convert to json object [%s]\n"), __TFUNCTION__, jsonString.c_str());

	return true;
}

//***************************************************************************
//
_tstring CJSONParser::GetJsonText(bool bIsPretty) const
{
	_tStringBuffer buffer;
	buffer.Clear();

	if( bIsPretty )
	{
#ifdef _UNICODE
		PrettyWriter<WStringBuffer, UTF16<>, UTF16<> > writer(buffer);
#else
		PrettyWriter<StringBuffer> writer(buffer);
#endif		
		m_pValue->Accept(writer);
	}
	else
	{
#ifdef _UNICODE
		Writer<WStringBuffer, UTF16<>, UTF16<> > writer(buffer);
#else
		Writer<StringBuffer, UTF8<>, UTF8<>> writer(buffer);
#endif
		m_pValue->Accept(writer);
	}

	_tstring result = _tstring(buffer.GetString(), buffer.GetSize());

	return result;
}

//***************************************************************************
//
CJSONParser CJSONParser::operator[](const TCHAR* ptszKey)
{
	_tValue::MemberIterator iter = m_pValue->FindMember(ptszKey);
	if( m_pValue->MemberEnd() == iter )
	{
		_tValue tKey(ptszKey, m_pDocument->GetAllocator());
		_tValue tValue(kObjectType);
		m_pValue->AddMember(tKey, tValue, m_pDocument->GetAllocator());

		iter = m_pValue->FindMember(ptszKey);
	}

	return CJSONParser(m_pDocument, &iter->value);
}

//***************************************************************************
//
CJSONParser CJSONParser::operator[](const _tstring& strKey)
{
	_tValue::MemberIterator iter = m_pValue->FindMember(strKey.c_str());
	if( m_pValue->MemberEnd() == iter )
	{
		_tValue tKey(strKey.c_str(), (rapidjson::SizeType)strKey.length(), m_pDocument->GetAllocator());
		_tValue tValue(kObjectType);
		m_pValue->AddMember(tKey, tValue, m_pDocument->GetAllocator());

		iter = m_pValue->FindMember(strKey.c_str());
	}

	return CJSONParser(m_pDocument, &iter->value);
}

//***************************************************************************
//
const CJSONParser CJSONParser::operator[](const TCHAR* ptszKey) const
{
	_tValue::MemberIterator iter = m_pValue->FindMember(ptszKey);
	if( m_pValue->MemberEnd() == iter )
	{
		return CJSONParser(m_pDocument, &gNullValue);
	}

	return CJSONParser(m_pDocument, &iter->value);
}

//***************************************************************************
//
const CJSONParser CJSONParser::operator[](const _tstring& strKey) const
{
	return operator[](strKey.c_str());
}

//***************************************************************************
//
int32 CJSONParser::ArrayCount() const
{
	if( false == m_pValue->IsArray() )
		return -1;

	return m_pValue->Size();
}

//***************************************************************************
//
CJSONParser CJSONParser::operator[](int32 iArrayIndex)
{
	if( false == m_pValue->IsArray() )
		m_pValue->SetArray();

	int32 iSize = m_pValue->Size();
	for( int32 i = iSize; i < iArrayIndex + 1; ++i )
	{
		_tValue tValue(kObjectType);
		m_pValue->PushBack(tValue, m_pDocument->GetAllocator());
	}

	_tValue& tValue = (*m_pValue)[iArrayIndex];
	return CJSONParser(m_pDocument, &tValue);
}

//***************************************************************************
//
const CJSONParser CJSONParser::operator[](int32 iArrayIndex) const
{
	if( false == m_pValue->IsArray() )
		return CJSONParser(m_pDocument, &gNullValue);

	if( (SizeType)iArrayIndex >= m_pValue->Size() )
		return CJSONParser(m_pDocument, &gNullValue);

	_tValue& tValue = (*m_pValue)[iArrayIndex];
	return CJSONParser(m_pDocument, &tValue);
}

//***************************************************************************
//
CJSONParser& CJSONParser::operator=(const TCHAR* ptszValue)
{
	m_pValue->SetString(ptszValue, m_pDocument->GetAllocator());
	return *this;
}

//***************************************************************************
//
CJSONParser& CJSONParser::operator=(const _tstring& strValue)
{
	m_pValue->SetString(strValue.c_str(), (rapidjson::SizeType)strValue.length(), m_pDocument->GetAllocator());
	return *this;
}

CJSONParser& CJSONParser::operator=(int32 iValue)
{
	m_pValue->SetInt(iValue);
	return *this;
}

CJSONParser& CJSONParser::operator=(int64 i64Value)
{
	m_pValue->SetInt64(i64Value);
	return *this;
}

CJSONParser& CJSONParser::operator=(uint32 uValue)
{
	m_pValue->SetUint(uValue);
	return *this;
}

CJSONParser& CJSONParser::operator=(uint64 u64Value)
{
	m_pValue->SetUint64(u64Value);
	return *this;
}

CJSONParser& CJSONParser::operator=(double dValue)
{
	m_pValue->SetDouble(dValue);
	return *this;
}

CJSONParser& CJSONParser::operator=(bool bValue)
{
	m_pValue->SetBool(bValue);
	return *this;
}

//***************************************************************************
//
CJSONParser::operator _tstring() const
{
	if( true == m_pValue->IsString() )
	{
		return _tstring(m_pValue->GetString(), m_pValue->GetStringLength());
	}
	else if( m_pValue->IsInt() )
	{
		TCHAR tszBuf[64] = { 0 };
		_sntprintf_s(tszBuf, _countof(tszBuf), _TRUNCATE, _T("%d"), m_pValue->GetInt());
		return tszBuf;
	}
	else if( m_pValue->IsInt64() )
	{
		TCHAR tszBuf[64] = { 0 };
		_sntprintf_s(tszBuf, _countof(tszBuf), _TRUNCATE, _T("%jd"), m_pValue->GetInt64());
		return tszBuf;
	}
	else if( m_pValue->IsUint() )
	{
		TCHAR tszBuf[64] = { 0 };
		_sntprintf_s(tszBuf, _countof(tszBuf), _TRUNCATE, _T("%u"), m_pValue->GetUint());
		return tszBuf;
	}
	else if( true == m_pValue->IsUint64() )
	{
		TCHAR tszBuf[64] = { 0 };
		_sntprintf_s(tszBuf, _countof(tszBuf), _TRUNCATE, _T("%ju"), m_pValue->GetUint64());
		return tszBuf;
	}
	else if( true == m_pValue->IsFloat() )
	{
		TCHAR tszBuf[64] = { 0 };
		_sntprintf_s(tszBuf, _countof(tszBuf), _TRUNCATE, _T("%f"), m_pValue->GetFloat());
		return tszBuf;
	}
	else if( true == m_pValue->IsDouble() )
	{
		TCHAR tszBuf[64] = { 0 };
		_sntprintf_s(tszBuf, _countof(tszBuf), _TRUNCATE, _T("%g"), m_pValue->GetDouble());
		return tszBuf;
	}
	else
	{
		return GetJsonText();
	}
}

//***************************************************************************
//
CJSONParser::operator int8() const
{
#ifdef _UNICODE
	return ConvertToNumber<int8>(_wtoi, 0);
#else
	return ConvertToNumber<int8>(_ttoi, 0);
#endif
}

//***************************************************************************
//
CJSONParser::operator uint8() const
{
#ifdef _UNICODE
	return ConvertToNumber<uint8>(_wtoi, 0);
#else
	return ConvertToNumber<uint8>(_ttoi, 0);
#endif
}

//***************************************************************************
//
CJSONParser::operator int16() const
{
#ifdef _UNICODE
	return ConvertToNumber<int16>(_wtoi, 0);
#else
	return ConvertToNumber<int16>(_ttoi, 0);
#endif
}

//***************************************************************************
//
CJSONParser::operator uint16() const
{
#ifdef _UNICODE
	return ConvertToNumber<uint16>(_wtoi, 0);
#else
	return ConvertToNumber<uint16>(_ttoi, 0);
#endif
}

//***************************************************************************
//
CJSONParser::operator int32() const
{
#ifdef _UNICODE
	return ConvertToNumber<int32>(_wtoi, 0);
#else
	return ConvertToNumber<int32>(_ttoi, 0);
#endif
}

//***************************************************************************
//
CJSONParser::operator uint32() const
{
#ifdef _UNICODE
	return ConvertToNumber<uint32>(_wtoi, 0);
#else
	return ConvertToNumber<uint32>(_ttoi, 0);
#endif
}

//***************************************************************************
//
CJSONParser::operator int64() const
{
#ifdef _UNICODE
	return ConvertToNumber<int64>(_wtoll, 0L);
#else
	return ConvertToNumber<int64>(_ttoll, 0L);
#endif
}

//***************************************************************************
//
CJSONParser::operator uint64() const
{
#ifdef _UNICODE
	return ConvertToNumber<uint64>(_wtoll, 0L);
#else
	return ConvertToNumber<uint64_t>(_ttoll, 0L);
#endif
}

//***************************************************************************
//
CJSONParser::operator double() const
{
#ifdef _UNICODE
	return ConvertToNumber<double>(_wtof, 0.0);
#else
	return ConvertToNumber<double>(_ttof, 0.0);
#endif
}

//***************************************************************************
//
CJSONParser::operator bool() const
{
#ifdef _UNICODE
	return ConvertToNumber<bool>(_wtoi, 0);
#else
	return ConvertToNumber<bool>(_ttoi, 0);
#endif
}

//***************************************************************************
//
bool CJSONParser::IsObject() const
{
	switch( m_pValue->GetType() )
	{
		case kStringType:
		case kNumberType:
		case kTrueType:
		case kFalseType:
			return false;
		default:
			return true;
	}
}

//***************************************************************************
//
bool CJSONParser::IsString() const
{
	return m_pValue->IsString();
}

//***************************************************************************
//
bool CJSONParser::IsNumber() const
{
	return m_pValue->IsNumber();
}

//***************************************************************************
//
bool CJSONParser::IsStringNumber() const
{
	if( false == IsString() )
		return false;

	const TCHAR* cch = m_pValue->GetString();
	if( L'-' == *cch )
		++cch;

	int iDotCount = 0;
	for( ; *cch != 0; ++cch )
	{
		if( 0 == isdigit(*cch) )
		{
			if( '.' != *cch )
				return false;

			++iDotCount;
			if( 1 < iDotCount )
				return false;
		}
	}

	return true;
}

//***************************************************************************
//
bool CJSONParser::IsInt32() const
{
	return m_pValue->IsInt();
}

//***************************************************************************
//
bool CJSONParser::IsInt64() const
{
	return m_pValue->IsInt64();
}

//***************************************************************************
//
bool CJSONParser::IsUint32() const
{
	return m_pValue->IsUint();
}

//***************************************************************************
//
bool CJSONParser::IsUint64() const
{
	return m_pValue->IsUint64();
}

//***************************************************************************
//
bool CJSONParser::IsDouble() const
{
	return m_pValue->IsDouble();
}

//***************************************************************************
//
bool CJSONParser::IsBool() const
{
	switch( m_pValue->GetType() )
	{
		case kTrueType:
		case kFalseType:
			return true;
	}

	return m_pValue->IsBool();
}

//***************************************************************************
//
bool CJSONParser::IsExists(const TCHAR* ptszKey) const
{
	return m_pValue->HasMember(ptszKey);
}

//***************************************************************************
//
bool CJSONParser::IsExists(const _tstring& strKey) const
{
	return IsExists(strKey.c_str());
}

//***************************************************************************
//
bool CJSONParser::Remove(const TCHAR* ptszKey) const
{
	if( m_pValue->HasMember(ptszKey) )
	{
		return m_pValue->RemoveMember(ptszKey);
	}

	return false;
}

//***************************************************************************
//
bool CJSONParser::Remove(const _tstring& strKey) const
{
	return Remove(strKey.c_str());
}

//***************************************************************************
//
bool CJSONParser::Remove(uint32 ui32ArrayIndex) const
{
	if( false == m_pValue->IsArray() )
		return false;

	if( m_pValue->Size() < ui32ArrayIndex )
		return false;

	int index = 0;
	for( _tValue::ValueIterator iter = m_pValue->Begin(); iter != m_pValue->End();)
	{
		if( index == ui32ArrayIndex )
		{
			m_pValue->Erase(iter);
			break;
		}
		++index;
		++iter;
	}

	return true;
}
