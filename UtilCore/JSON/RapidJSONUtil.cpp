
//***************************************************************************
// RapidJSONUtil.cpp: implementation of the CRapidJSONUtil class.
//
//***************************************************************************

#include "pch.h"
#include "RapidJSONUtil.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CRapidJSONUtil::CRapidJSONUtil() : _allocator(_document.GetAllocator()) 
{
	_document.SetObject();
}

//***************************************************************************
// 복사 생성자
CRapidJSONUtil::CRapidJSONUtil(const CRapidJSONUtil& other) : _allocator(_document.GetAllocator())
{
	_document.CopyFrom(other._document, _allocator);		// 객체 복사
}

//***************************************************************************
//
void CRapidJSONUtil::Print_DebugInfo(const TCHAR* ptszFormat, ...)
{
	if( !_bIsDebugPrint ) return;

	va_list args;

	_ftprintf_s(stdout, _T("CRapidJSONUtil::"));

	va_start(args, ptszFormat);
	_vtprintf_s(ptszFormat, args);
	va_end(args);

	return;
}

//***************************************************************************
// = 연산자 오버로딩 (JSON 복사)
CRapidJSONUtil& CRapidJSONUtil::operator=(const CRapidJSONUtil& other)
{
	if( this != &other )
	{
		_document.CopyFrom(other._document, _allocator);
	}
	return *this;  // 자신을 리턴하여 연속적인 연산 가능
}

//***************************************************************************
//
CRapidJSONUtil& CRapidJSONUtil::operator=(const TCHAR* ptszValue)
{
	_document.SetString(ptszValue, _allocator);
	return *this;
}

//***************************************************************************
//
CRapidJSONUtil& CRapidJSONUtil::operator=(const _tstring& strValue)
{
	_document.SetString(strValue.c_str(), (rapidjson::SizeType)strValue.length(), _allocator);
	return *this;
}

//***************************************************************************
//
CRapidJSONUtil& CRapidJSONUtil::operator=(int32 iValue)
{
	_document.SetInt(iValue);
	return *this;
}

//***************************************************************************
//
CRapidJSONUtil& CRapidJSONUtil::operator=(int64 i64Value)
{
	_document.SetInt64(i64Value);
	return *this;
}

//***************************************************************************
//
CRapidJSONUtil& CRapidJSONUtil::operator=(uint32 uValue)
{
	_document.SetUint(uValue);
	return *this;
}

//***************************************************************************
//
CRapidJSONUtil& CRapidJSONUtil::operator=(uint64 u64Value)
{
	_document.SetUint64(u64Value);
	return *this;
}

//***************************************************************************
//
CRapidJSONUtil& CRapidJSONUtil::operator=(double dValue)
{
	_document.SetDouble(dValue);
	return *this;
}

//***************************************************************************
//
CRapidJSONUtil& CRapidJSONUtil::operator=(bool bValue)
{
	_document.SetBool(bValue);
	return *this;
}

//***************************************************************************
// + 연산자 오버로딩(객체 확장)
CRapidJSONUtil& CRapidJSONUtil::operator+(const std::pair<_tstring, CRapidJSONUtil>& keyValue)
{
	if( !_document.IsObject() )
	{
		_document.SetObject();
	}
	
	_tValue key(keyValue.first.c_str(), _allocator);	// Key를 rapidjson::Value로 변환
	_tValue value;

	// Value도 복사하여 추가해야 함
	value.CopyFrom(keyValue.second._document, _allocator);

	_document.AddMember(key, value, _allocator);
	return *this;
}

//***************************************************************************
// + 연산자 오버로딩(배열  확장)
CRapidJSONUtil& CRapidJSONUtil::operator+(const CRapidJSONUtil& other)
{
	if( !_document.IsArray() )
	{
		_document.SetArray();
	}
	_tValue value;
	value.CopyFrom(other._document, _allocator);	// JSON 객체를 복사하여 추가
	_document.PushBack(value, _allocator);
	return *this;
}

//***************************************************************************
// - 연산자 오버로딩(객체 속성 삭제)
CRapidJSONUtil& CRapidJSONUtil::operator-(const _tstring& key)
{
	if( _document.IsObject() && _document.HasMember(key.c_str()) )
	{
		RecursiveRemove(_document[key.c_str()]);
		_document.RemoveMember(key.c_str());
	}
	return *this;
}

//***************************************************************************
// - 연산자 오버로딩(배열 요소 삭제)
CRapidJSONUtil& CRapidJSONUtil::operator-(const uint32 index)
{
	if( _document.IsArray() && index < _document.Size() )
	{
		RecursiveRemove(_document[index]);
		_document.Erase(_document.Begin() + index);
	}
	return *this;
}

/*
//***************************************************************************
// [] 연산자 오버로딩(Setter)
_tValue& CRapidJSONUtil::operator[](const _tstring& key)
{
	if( !_document.HasMember(key.c_str()) ) 
	{
		_tValue newKey(key.c_str(), _allocator);
		_document.AddMember(newKey, _tValue(), _allocator);
	}
	return _document[key.c_str()];
}

//***************************************************************************
// [] 연산자 오버로딩(Getter)
const _tValue& CRapidJSONUtil::operator[](const _tstring& key) const
{
	if( !_document.HasMember(key.c_str()) ) 
	{
#ifdef _UNICODE
		std::wcerr << L"Key not found : " << key << std::endl;
#else
		std::cerr << "Key not found : " << key << std::endl;
#endif
	}
	return _document[key.c_str()];
}
*/

//***************************************************************************
//
bool CRapidJSONUtil::IsExists(const TCHAR* ptszKey) const
{
	return _document.HasMember(ptszKey);
}

//***************************************************************************
//
bool CRapidJSONUtil::IsExists(const _tstring& strKey) const
{
	return IsExists(strKey.c_str());
}

//***************************************************************************
//
bool CRapidJSONUtil::IsObject() const
{
	return _document.GetType() == kObjectType ? true : false;
}

//***************************************************************************
//
bool CRapidJSONUtil::IsArray() const
{
	return _document.GetType() == kArrayType ? true : false;
}

//***************************************************************************
//
bool CRapidJSONUtil::IsString() const
{
	return _document.IsString();
}

//***************************************************************************
//
bool CRapidJSONUtil::IsNumber() const
{
	return _document.IsNumber();
}

//***************************************************************************
//
bool CRapidJSONUtil::IsStringNumber() const
{
	if( false == IsString() )
		return false;

	const TCHAR* cch = _document.GetString();
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
bool CRapidJSONUtil::IsInt32() const
{
	return _document.IsInt();
}

//***************************************************************************
//
bool CRapidJSONUtil::IsInt64() const
{
	return _document.IsInt64();
}

//***************************************************************************
//
bool CRapidJSONUtil::IsUint32() const
{
	return _document.IsUint();
}

//***************************************************************************
//
bool CRapidJSONUtil::IsUint64() const
{
	return _document.IsUint64();
}

//***************************************************************************
//
bool CRapidJSONUtil::IsDouble() const
{
	return _document.IsDouble();
}

//***************************************************************************
//
bool CRapidJSONUtil::IsBool() const
{
	switch( _document.GetType() )
	{
		case kTrueType:
		case kFalseType:
			return true;
	}

	return _document.IsBool();
}

//***************************************************************************
// JSON 문자열 파싱
bool CRapidJSONUtil::Parse(const _tstring& jsonString)
{
	if( _document.Parse(jsonString.c_str()).HasParseError() )
	{
		_tcerr << _T("JSON parsing error!") << std::endl;
		return false;
	}
	return true;
}

//***************************************************************************
// JSON 문자열 생성
_tstring CRapidJSONUtil::ToString(const bool pretty) const
{
	_tStringBuffer buffer;

	if( pretty )
	{
#ifdef _UNICODE
		PrettyWriter<WStringBuffer, UTF16<>, UTF16<> > writer(buffer);
#else
		PrettyWriter<StringBuffer> writer(buffer);
#endif	

		_document.Accept(writer);
	}
	else
	{
#ifdef _UNICODE
		Writer<WStringBuffer, UTF16<>, UTF16<> > writer(buffer);
#else
		Writer<StringBuffer, UTF8<>, UTF8<>> writer(buffer);
#endif

		_document.Accept(writer);
	}

	return buffer.GetString();
}

//***************************************************************************
// JSON 출력 (디버깅용)
void CRapidJSONUtil::PrintJSON(const bool pretty) const
{
	_tcout << ToString(pretty) << std::endl;
}

//***************************************************************************
//
bool CRapidJSONUtil::SaveToFile(const _tstring& filename, _tstring& jsonString, const bool pretty)
{
	bool result = false;

	result = Parse(jsonString);
	if( result ) result = SaveToFile(filename, pretty);

	return result;
}

//***************************************************************************
//
bool CRapidJSONUtil::SaveToFile(const _tstring& filename, const bool pretty)
{
	_tStringBuffer buffer;

	if( pretty )
	{
#ifdef _UNICODE
		PrettyWriter<WStringBuffer, UTF16<>, UTF16<> > writer(buffer);
#else
		PrettyWriter<StringBuffer> writer(buffer);
#endif	

		if( _document.Accept(writer) == false )
		{
			Print_DebugInfo(_T("%s open failed [%s]\n"), __TFUNCTION__, filename.c_str());
			return false;
		}
	}
	else
	{
#ifdef _UNICODE
		Writer<WStringBuffer, UTF16<>, UTF16<> > writer(buffer);
#else
		Writer<StringBuffer, UTF8<>, UTF8<>> writer(buffer);
#endif

		if( _document.Accept(writer) == false )
		{
			Print_DebugInfo(_T("%s open failed [%s]\n"), __TFUNCTION__, filename.c_str());
			return false;
		}
	}

	_tstring temp = buffer.GetString();

	_tofstream out(filename, _tofstream::trunc);

#ifdef _UNICODE
	out.imbue(std::locale("ko_KR.UTF-8"));
	out << temp;
#else
	out << Iconv::CIconvUtil::ConvertEncoding(temp, "CP949", "UTF-8");
#endif
	out.close();

	Print_DebugInfo(_T("%s write to [%s] - [%s]\n"), __TFUNCTION__, filename.c_str(), temp.c_str());

	return true;
}

//***************************************************************************
// JSON 파일 로드
bool CRapidJSONUtil::LoadFromFile(const _tstring& filename)
{
	_tifstream ifs(filename);
	if( !ifs.is_open() )
	{
		_tcerr << _T("Failed to open file: ") << filename << std::endl;
		return false;
	}
	_tstring content((std::istreambuf_iterator<TCHAR>(ifs)), std::istreambuf_iterator<TCHAR>());
	ifs.close();

	return Parse(content);
}

//***************************************************************************
//
std::vector<_tstring> CRapidJSONUtil::GetKeys()
{
	std::vector<_tstring> keys;

	// JSON 객체 순회
	if( _document.IsObject() )
	{
		for( auto& member : _document.GetObject() )
		{
			keys.push_back(member.name.GetString());
		}
	}

	return keys;
}

//***************************************************************************
//
void CRapidJSONUtil::Remove(const TCHAR* ptszKey)
{
	if( _document.IsObject() && _document.HasMember(ptszKey) )
	{
		RecursiveRemove(_document[ptszKey]);
		_document.RemoveMember(ptszKey);
	}
}

//***************************************************************************
// 객체 또는 배열 요소 삭제 함수
void CRapidJSONUtil::Remove(const _tstring& key)
{
	return Remove(key.c_str());
}

//***************************************************************************
//
void CRapidJSONUtil::Remove(const _tstring& key, uint32 index)
{
	if( _document.HasMember(key.c_str()) && _document[key.c_str()].IsArray() )
	{
		auto& arr = _document[key.c_str()];
		if( index < arr.Size() )
		{
			RecursiveRemove(arr);
			arr.Erase(arr.Begin() + index);
		}
	}
}

//***************************************************************************
// 재귀적으로 요소 삭제하는 함수
void CRapidJSONUtil::RecursiveRemove(_tValue& value)
{
	if( value.IsObject() ) 
	{
		for( auto itr = value.MemberBegin(); itr != value.MemberEnd(); ++itr ) 
		{
			RecursiveRemove(itr->value);
		}
		value.SetObject();
	}
	else if( value.IsArray() ) 
	{
		for( auto itr = value.Begin(); itr != value.End(); ++itr ) 
		{
			RecursiveRemove(*itr);
		}
		value.SetArray();
	}
}

