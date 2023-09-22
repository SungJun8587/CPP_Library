
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
	CJSONParser(BOOL bIsDebugPrint = false);
	CJSONParser(const CJSONParser& other);
	~CJSONParser(void);

	CJSONParser& operator=(const CJSONParser& Other);

	void 	SetDebugPrint(BOOL bIsDebugPrint) {
		m_bIsDebugPrint = bIsDebugPrint;
		Print_DebugInfo(_T("%s is set %s\n"), __TFUNCTION__, bIsDebugPrint ? _T("on") : _T("off"));
	}

	BOOL   GetDebugPrint() {
		return m_bIsDebugPrint;
	}

	_tstring	ReadFile(const TCHAR* ptszFilePath);
	BOOL		WriteFile(const TCHAR* ptszFilePath, TCHAR* ptszJsonString);
	BOOL		WriteJson(const TCHAR* ptszFilePath);

	BOOL		Parse(const _tstring jsonString);
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
	operator int16() const;
	operator uint16() const;
	operator int32() const;
	operator uint32() const;
	operator int64() const;
	operator uint64() const;
	operator double() const;
	operator bool() const;

	BOOL IsObject() const;
	BOOL IsString() const;
	BOOL IsNumber() const;
	BOOL IsStringNumber() const;
	BOOL IsInt32() const;
	BOOL IsInt64() const;
	BOOL IsUint32() const;
	BOOL IsUint64() const;
	BOOL IsDouble() const;
	BOOL IsBool() const;
	BOOL IsExists(const TCHAR* ptszKey) const;
	BOOL IsExists(const _tstring& strKey) const;

	BOOL Remove(const TCHAR* ptszKey) const;
	BOOL Remove(const _tstring& strKey) const;
	BOOL Remove(uint32 ui32ArrayIndex) const;

	template< typename DataType >
	BOOL Add(TCHAR* ptszNodePath, DataType data);

	template< typename DataType >
	BOOL Update(TCHAR* ptszNodePath, DataType data);

	template< typename DataType >
	_tstring SerializeToJson(const DataType& data, BOOL bIsPretty = true);

	template< typename DataType >
	DataType DeserializeViaDom(const TCHAR* const json);

	template< typename DataType >
	DataType DeserializeViaDom(const _tstring& json);

#if __cplusplus >= 201703L
	template< typename DataType >
	void SerializeToJson(const DataType& data, const std::filesystem::path& path, BOOL bIsPretty = true);

	template< typename DataType >
	DataType DeserializeViaDom(const std::filesystem::path& path);

	template< typename DataType >
	DataType DeserializeViaSax(const TCHAR* const json);

	template< typename DataType >
	DataType DeserializeViaSax(const _tstring& json);

	template< typename DataType >
	DataType DeserializeViaSax(const std::filesystem::path& path);
#endif

	void	Print_DebugInfo(const TCHAR* ptszFormat, ...);

private:
	CJSONParser(_tDocument* pDoc, _tValue* pValue);

	template<typename DataType, typename _CONVERT_FUNC, typename _DEF_VAL>
	DataType ConvertToNumber(_CONVERT_FUNC pFunc, _DEF_VAL tDefValue) const;

	template< typename DataType >
	DataType Deserialize(_tStringStream& stream);

private:
	BOOL	m_bIsDebugPrint;
	BOOL    m_bRoot;

	_tDocument*	m_pDocument;
	_tValue*	m_pValue;
};

#include "JSONParser.inl"

#endif // ndef __JSONPARSER_H__
