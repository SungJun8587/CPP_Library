
//***************************************************************************
// XMLParser.cpp: implementation of the CXMLParser class.
//
//***************************************************************************

#include "pch.h"
#include "XMLParser.h"

_locale_t kr = _create_locale(LC_NUMERIC, "kor");

//***************************************************************************
//
bool CXMLNode::GetBoolAttr(const TCHAR* ptszKey, bool defaultValue)
{
	_tXmlAttributeType* attr = _node->first_attribute(ptszKey);
	if( attr )
		return ::_tcsicmp(attr->value(), _T("true")) == 0;

	return defaultValue;
}

//***************************************************************************
//
int8 CXMLNode::GetInt8Attr(const TCHAR* ptszKey, int8 defaultValue)
{
	_tXmlAttributeType* attr = _node->first_attribute(ptszKey);
	if( attr )
		return static_cast<int8>(::_ttoi(attr->value()));

	return defaultValue;
}

//***************************************************************************
//
int16 CXMLNode::GetInt16Attr(const TCHAR* ptszKey, int16 defaultValue)
{
	_tXmlAttributeType* attr = _node->first_attribute(ptszKey);
	if( attr )
		return static_cast<int16>(::_ttoi(attr->value()));

	return defaultValue;
}

//***************************************************************************
//
int32 CXMLNode::GetInt32Attr(const TCHAR* ptszKey, int32 defaultValue)
{
	_tXmlAttributeType* attr = _node->first_attribute(ptszKey);
	if( attr )
		return ::_ttoi(attr->value());

	return defaultValue;
}

//***************************************************************************
//
int64 CXMLNode::GetInt64Attr(const TCHAR* ptszKey, int64 defaultValue)
{
	_tXmlAttributeType* attr = _node->first_attribute(ptszKey);
	if( attr )
		return ::_ttoi64(attr->value());

	return defaultValue;
}

//***************************************************************************
//
float CXMLNode::GetFloatAttr(const TCHAR* ptszKey, float defaultValue)
{
	_tXmlAttributeType* attr = _node->first_attribute(ptszKey);
	if( attr )
		return static_cast<float>(::_ttof(attr->value()));

	return defaultValue;
}

//***************************************************************************
//
double CXMLNode::GetDoubleAttr(const TCHAR* ptszKey, double defaultValue)
{
	_tXmlAttributeType* attr = _node->first_attribute(ptszKey);
	if( attr )
#ifdef _UNICODE
		return ::_wtof_l(attr->value(), kr);
#else
		return ::_atof_l(attr->value(), kr);
#endif
	return defaultValue;
}

//***************************************************************************
//
const TCHAR* CXMLNode::GetStringAttr(const TCHAR* ptszKey, const TCHAR* ptszDefaultValue)
{
	_tXmlAttributeType* attr = _node->first_attribute(ptszKey);
	if( attr )
		return attr->value();

	return ptszDefaultValue;
}

//***************************************************************************
//
bool CXMLNode::GetBoolValue(bool defaultValue)
{
	TCHAR* val = _node->value();
	if( val )
		return ::_tcsicmp(val, _T("true")) == 0;

	return defaultValue;
}

//***************************************************************************
//
int8 CXMLNode::GetInt8Value(int8 defaultValue)
{
	TCHAR* val = _node->value();
	if( val )
		return static_cast<int8>(::_ttoi(val));

	return defaultValue;
}

//***************************************************************************
//
int16 CXMLNode::GetInt16Value(int16 defaultValue)
{
	TCHAR* val = _node->value();
	if( val )
		return static_cast<int16>(::_ttoi(val));
	return defaultValue;
}

//***************************************************************************
//
int32 CXMLNode::GetInt32Value(int32 defaultValue)
{
	TCHAR* val = _node->value();
	if( val )
		return static_cast<int32>(::_ttoi(val));

	return defaultValue;
}

//***************************************************************************
//
int64 CXMLNode::GetInt64Value(int64 defaultValue)
{
	TCHAR* val = _node->value();
	if( val )
		return static_cast<int64>(::_ttoi64(val));

	return defaultValue;
}

//***************************************************************************
//
float CXMLNode::GetFloatValue(float defaultValue)
{
	TCHAR* val = _node->value();
	if( val )
		return static_cast<float>(::_ttof(val));

	return defaultValue;
}

//***************************************************************************
//
double CXMLNode::GetDoubleValue(double defaultValue)
{
	TCHAR* val = _node->value();
	if( val )
#ifdef _UNICODE
		return ::_wtof_l(val, kr);
#else
		return ::_atof_l(val, kr);
#endif

	return defaultValue;
}

//***************************************************************************
//
const TCHAR* CXMLNode::GetStringValue(const TCHAR* ptszDefaultValue)
{
	TCHAR* val = _node->first_node()->value();
	if( val )
		return val;

	return ptszDefaultValue;
}

//***************************************************************************
//
CXMLNode CXMLNode::FindChild(const TCHAR* ptszKey)
{
	return CXMLNode(_node->first_node(ptszKey));
}

//***************************************************************************
//
CVector<CXMLNode> CXMLNode::FindChildren(const TCHAR* ptszKey)
{
	CVector<CXMLNode> nodes;

	xml_node<TCHAR>* node = _node->first_node(ptszKey);
	while( node )
	{
		nodes.push_back(CXMLNode(node));
		node = node->next_sibling(ptszKey);
	}

	return nodes;
}

//***************************************************************************
//
bool CXMLParser::ParseFromFile(const TCHAR* ptszPath, OUT CXMLNode& root)
{
	ReadFile(_data, ptszPath);
	if( _data.empty() )
		return false;

	_document = std::make_shared<_tXmlDocumentType>();
	_document->parse<0>(reinterpret_cast<TCHAR*>(&_data[0]));
	root = CXMLNode(_document->first_node());

	return true;
}

