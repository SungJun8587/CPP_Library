
//***************************************************************************
// JsonParser.h : interface and implementation for the CJSONParser class.
//
//***************************************************************************

#ifndef __JSONPARSER_H__
#define __JSONPARSER_H__

#include <stdarg.h>
#include <string>

#if __cplusplus >= 201703L
#include <filesystem>
#include <fstream>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#endif

#include "json_dom_deserializer.h"
#include "json_sax_deserializer.h"
#include "json_serializer.h"

using namespace json_utils;

#if __cplusplus >= 201703L
#ifdef _UNICODE
typedef WOStreamWrapper  _tStreamWrapper;
#else
typedef OStreamWrapper  _tStreamWrapper;
#endif
#endif

static _tValue gNullValue(kNullType);

class CJSONParser 
{
public:
	CJSONParser(bool bIsDebugPrint = false);
	CJSONParser(const CJSONParser& other);
	~CJSONParser(void);

	CJSONParser& operator=(const CJSONParser& Other);

	void 	SetDebugPrint(bool bIsDebugPrint) {
		m_bIsDebugPrint = bIsDebugPrint;
		Print_DebugInfo(_T("%s is set %s\n"), __TFUNCTION__, bIsDebugPrint ? _T("on") : _T("off"));
	}

	bool   GetDebugPrint() {
		return m_bIsDebugPrint;
	}

	_tstring	ReadFile(const TCHAR* ptszFilePath);
	bool		WriteFile(const TCHAR* ptszFilePath, TCHAR* ptszJsonString);
	bool		WriteJson(const TCHAR* ptszFilePath);

	bool		Parse(const _tstring jsonString);
	_tstring	GetJsonText(bool isPretty = false) const;

	CJSONParser operator[](const TCHAR* ptszValue);
	CJSONParser operator[](const _tstring& strKey);
	const CJSONParser operator[](const TCHAR* ptszValue) const;
	const CJSONParser operator[](const _tstring& strKey) const;

	int32 ArrayCount() const;
	CJSONParser operator[](int32 iArrayIndex);
	const CJSONParser operator[](int32 iArrayIndex) const;

	CJSONParser& operator=(const TCHAR* ptszValue);
	CJSONParser& operator=(const _tstring& strValue);
	CJSONParser& operator=(int32 iValue);
	CJSONParser& operator=(int64 i64Value);
	CJSONParser& operator=(uint32 uValue);
	CJSONParser& operator=(uint64 u64Value);
	CJSONParser& operator=(double dValue);
	CJSONParser& operator=(bool bValue);

	operator _tstring() const;
	operator int8() const;
	operator uint8() const;
	operator int16() const;
	operator uint16() const;
	operator int32() const;
	operator uint32() const;
	operator int64() const;
	operator uint64() const;
	operator double() const;
	operator bool() const;

	bool IsObject() const;
	bool IsString() const;
	bool IsNumber() const;
	bool IsStringNumber() const;
	bool IsInt32() const;
	bool IsInt64() const;
	bool IsUint32() const;
	bool IsUint64() const;
	bool IsDouble() const;
	bool IsBool() const;
	bool IsExists(const TCHAR* ptszKey) const;
	bool IsExists(const _tstring& strKey) const;

	bool Remove(const TCHAR* ptszKey) const;
	bool Remove(const _tstring& strKey) const;
	bool Remove(uint32 ui32ArrayIndex) const;

	template< typename EDataType >
	bool Add(TCHAR* ptszNodePath, EDataType data);

	template< typename EDataType >
	bool Update(TCHAR* ptszNodePath, EDataType data);

	template< typename EDataType >
	_tstring SerializeToJson(const EDataType& data, bool bIsPretty = true);

	template< typename EDataType >
	EDataType DeserializeViaDom(const TCHAR* const json);

	template< typename EDataType >
	EDataType DeserializeViaDom(const _tstring& json);

#if __cplusplus >= 201703L
	template< typename EDataType >
	void SerializeToJson(const EDataType& data, const std::filesystem::path& path, bool bIsPretty = true);

	template< typename EDataType >
	EDataType DeserializeViaDom(const std::filesystem::path& path);

	template< typename EDataType >
	EDataType DeserializeViaSax(const TCHAR* const json);

	template< typename EDataType >
	EDataType DeserializeViaSax(const _tstring& json);

	template< typename EDataType >
	EDataType DeserializeViaSax(const std::filesystem::path& path);
#endif

	void	Print_DebugInfo(const TCHAR* ptszFormat, ...);

private:
	CJSONParser(_tDocument* pDoc, _tValue* pValue);

	template<typename EDataType, typename _CONVERT_FUNC, typename _DEF_VAL>
	EDataType ConvertToNumber(_CONVERT_FUNC pFunc, _DEF_VAL tDefValue) const;

	template< typename EDataType >
	EDataType Deserialize(_tStringStream& stream);

private:
	bool	m_bIsDebugPrint;
	bool    m_bRoot;

	_tDocument*	m_pDocument;
	_tValue*	m_pValue;
};

#include "JSONParser.inl"

#endif // ndef __JSONPARSER_H__
