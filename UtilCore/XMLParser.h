
//***************************************************************************
// XMLParser.h : interface for the CXMLParser class.
//
//***************************************************************************

#ifndef __XMLPARSER_H__
#define __XMLPARSER_H__

#pragma once

#include "rapidxml.hpp"
#include <vector>

#ifndef __SHELLUTIL_H__
#include <ShellUtil.h> 
#endif

using namespace rapidxml;

typedef xml_node<TCHAR> _tXmlNodeType;
typedef xml_document<TCHAR> _tXmlDocumentType;
typedef xml_attribute<TCHAR> _tXmlAttributeType;

class CXMLNode
{
public:
	CXMLNode(_tXmlNodeType* node = nullptr) : _node(node)
	{
	}

	bool				IsValid()
	{
		return _node != nullptr;
	}

	bool				GetBoolAttr(const TCHAR* ptszKey, bool defaultValue = false);
	int8				GetInt8Attr(const TCHAR* ptszKey, int8 defaultValue = 0);
	int16				GetInt16Attr(const TCHAR* ptszKey, int16 defaultValue = 0);
	int32				GetInt32Attr(const TCHAR* ptszKey, int32 defaultValue = 0);
	int64				GetInt64Attr(const TCHAR* ptszKey, int64 defaultValue = 0);
	float				GetFloatAttr(const TCHAR* ptszKey, float defaultValue = 0.0f);
	double				GetDoubleAttr(const TCHAR* ptszKey, double defaultValue = 0.0);
	const TCHAR*		GetStringAttr(const TCHAR* ptszKey, const TCHAR* defaultValue = _T(""));

	bool				GetBoolValue(bool defaultValue = false);
	int8				GetInt8Value(int8 defaultValue = 0);
	int16				GetInt16Value(int16 defaultValue = 0);
	int32				GetInt32Value(int32 defaultValue = 0);
	int64				GetInt64Value(int64 defaultValue = 0);
	float				GetFloatValue(float defaultValue = 0.0f);
	double				GetDoubleValue(double defaultValue = 0.0);
	const TCHAR*		GetStringValue(const TCHAR* ptszDefaultValue = _T(""));

	CXMLNode			FindChild(const TCHAR* ptszKey);
	CVector<CXMLNode>	FindChildren(const TCHAR* ptszKey);

private:
	_tXmlNodeType* _node = nullptr;
};

class CXMLParser
{
public:
	bool ParseFromFile(const TCHAR* ptszPath, OUT CXMLNode& root);

private:
	shared_ptr<_tXmlDocumentType>		_document = nullptr;
	_tstring							_data;
};

#endif // ndef __XMLPARSER_H__