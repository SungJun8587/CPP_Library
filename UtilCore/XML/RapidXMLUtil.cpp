
//***************************************************************************
// RapidXMLUtil.cpp: implementation of the CXMLParser class.
//
//***************************************************************************

#include "pch.h"
#include "RapidXMLUtil.h"

_locale_t kr = _create_locale(LC_NUMERIC, "kor");

//***************************************************************************
//
bool CXMLNode::GetBoolAttr(const TCHAR* ptszKey, bool defaultValue)
{
	xml_attribute<>* attr = _node->first_attribute(TcharToUtf8(ptszKey).c_str());
	if( attr )
		return _stricmp(attr->value(), "true") == 0;

	return defaultValue;
}

//***************************************************************************
//
int8 CXMLNode::GetInt8Attr(const TCHAR* ptszKey, int8 defaultValue)
{
	xml_attribute<>* attr = _node->first_attribute(TcharToUtf8(ptszKey).c_str());
	if( attr )
		return static_cast<int8>(atoi(attr->value()));

	return defaultValue;
}

//***************************************************************************
//
int16 CXMLNode::GetInt16Attr(const TCHAR* ptszKey, int16 defaultValue)
{
	xml_attribute<>* attr = _node->first_attribute(TcharToUtf8(ptszKey).c_str());
	if( attr )
		return static_cast<int16>(atoi(attr->value()));

	return defaultValue;
}

//***************************************************************************
//
int32 CXMLNode::GetInt32Attr(const TCHAR* ptszKey, int32 defaultValue)
{
	xml_attribute<>* attr = _node->first_attribute(TcharToUtf8(ptszKey).c_str());
	if( attr )
		return atoi(attr->value());

	return defaultValue;
}

//***************************************************************************
//
int64 CXMLNode::GetInt64Attr(const TCHAR* ptszKey, int64 defaultValue)
{
	xml_attribute<>* attr = _node->first_attribute(TcharToUtf8(ptszKey).c_str());
	if( attr )
		return _atoi64(attr->value());

	return defaultValue;
}

//***************************************************************************
//
float CXMLNode::GetFloatAttr(const TCHAR* ptszKey, float defaultValue)
{
	xml_attribute<>* attr = _node->first_attribute(TcharToUtf8(ptszKey).c_str());
	if( attr )
		return static_cast<float>(atof(attr->value()));

	return defaultValue;
}

//***************************************************************************
//
double CXMLNode::GetDoubleAttr(const TCHAR* ptszKey, double defaultValue)
{
	xml_attribute<>* attr = _node->first_attribute(TcharToUtf8(ptszKey).c_str());
	if( attr )
		return _atof_l(attr->value(), kr);

	return defaultValue;
}

//***************************************************************************
//
_tstring CXMLNode::GetStringAttr(const TCHAR* ptszKey, const TCHAR* defaultValue)
{
	xml_attribute<>* attr = _node->first_attribute(TcharToUtf8(ptszKey).c_str());
	if( attr )
	{
		_tstring value = Utf8ToTchar(attr->value());
		if( value.size() > 0 )
			return value;
	}
	return defaultValue;
}

//***************************************************************************
//
bool CXMLNode::GetBoolValue(bool defaultValue)
{
	char* val = _node->value();
	if( val )
		return _stricmp(val, "true") == 0;

	return defaultValue;
}

//***************************************************************************
//
int8 CXMLNode::GetInt8Value(int8 defaultValue)
{
	char* val = _node->value();
	if( val )
		return static_cast<int8>(atoi(val));

	return defaultValue;
}

//***************************************************************************
//
int16 CXMLNode::GetInt16Value(int16 defaultValue)
{
	char* val = _node->value();
	if( val )
		return static_cast<int16>(atoi(val));
	return defaultValue;
}

//***************************************************************************
//
int32 CXMLNode::GetInt32Value(int32 defaultValue)
{
	char* val = _node->value();
	if( val )
		return static_cast<int32>(atoi(val));

	return defaultValue;
}

//***************************************************************************
//
int64 CXMLNode::GetInt64Value(int64 defaultValue)
{
	char* val = _node->value();
	if( val )
		return static_cast<int64>(_atoi64(val));

	return defaultValue;
}

//***************************************************************************
//
float CXMLNode::GetFloatValue(float defaultValue)
{
	char* val = _node->value();
	if( val )
		return static_cast<float>(atof(val));

	return defaultValue;
}

//***************************************************************************
//
double CXMLNode::GetDoubleValue(double defaultValue)
{
	char* val = _node->value();
	if( val )
		return ::_atof_l(val, kr);

	return defaultValue;
}

//***************************************************************************
//
_tstring CXMLNode::GetStringValue(const TCHAR* defaultValue)
{
	_tstring value = Utf8ToTchar(_node->value());
	if( value.size() > 0 )
		return value;

	return defaultValue;
}

//***************************************************************************
//
CXMLNode CXMLNode::FindChild(const TCHAR* ptszKey)
{
	return CXMLNode(_node->first_node(TcharToUtf8(ptszKey).c_str()));
}

//***************************************************************************
//
std::vector<CXMLNode> CXMLNode::FindChildren(const TCHAR* ptszKey)
{
	std::vector<CXMLNode> nodes;

	xml_node<>* node = _node->first_node(TcharToUtf8(ptszKey).c_str());
	while( node )
	{
		nodes.push_back(CXMLNode(node));
		node = node->next_sibling(TcharToUtf8(ptszKey).c_str());
	}

	return nodes;
}

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CRapidXMLUtil::CRapidXMLUtil()
{
	_unicodeToUtf8 = new Iconv::CIconvUtil("WCHAR_T", "UTF-8");		// WCHAR_T -> UTF-8 변환
	_utf8ToUnicode = new Iconv::CIconvUtil("UTF-8", "WCHAR_T");		// UTF-8 -> WCHAR_T 변환
}

//***************************************************************************
// 생성자
CRapidXMLUtil::CRapidXMLUtil(const _tstring& xmlData)
{
	_unicodeToUtf8 = new Iconv::CIconvUtil("WCHAR_T", "UTF-8");	// WCHAR_T -> UTF-8 변환
	_utf8ToUnicode = new Iconv::CIconvUtil("UTF-8", "WCHAR_T");		// UTF-8 -> WCHAR_T 변환

	_xmlString = TcharToUtf8(xmlData);
	_doc.parse<0>(&_xmlString[0]);
}

//***************************************************************************
// 깊은 복사 생성자
CRapidXMLUtil::CRapidXMLUtil(const CRapidXMLUtil& other)
{
	_unicodeToUtf8 = new Iconv::CIconvUtil("WCHAR_T", "UTF-8");	// WCHAR_T -> UTF-8 변환
	_utf8ToUnicode = new Iconv::CIconvUtil("UTF-8", "WCHAR_T");		// UTF-8 -> WCHAR_T 변환

	_xmlString = other._xmlString;
	_doc.parse<0>(&_xmlString[0]);
}

CRapidXMLUtil::~CRapidXMLUtil()
{
	delete _utf8ToUnicode;
	delete _unicodeToUtf8;
}

//***************************************************************************
// 노드 및 모든 하위 노드 삭제
void CRapidXMLUtil::RemoveNode(const _tstring& nodeName)
{
	xml_node<char>* root = _doc.first_node(RootName);
	if( !root ) 
	{
		_tcout << _T("Root node not found") << endl;
		return;
	}

	xml_node<char>* targetNode = root->first_node(TcharToUtf8(nodeName).c_str());
	if( targetNode ) 
	{
		RemoveNodeRecursive(targetNode);
	}
	else 
	{
		_tcout << _T("Node not found: ") << nodeName << endl;
	}
}

//***************************************************************************
//
bool CRapidXMLUtil::ParseFromFile(const _tstring& filename, OUT CXMLNode& root)
{
	_xmlString = LoadFromFile<std::string>(filename);

	_doc.parse<0>(reinterpret_cast<char*>(&_xmlString[0]));
	root = CXMLNode(_doc.first_node());

	return true;
}

//***************************************************************************
//
bool CRapidXMLUtil::SaveFileToXML(const _tstring& filename, const _tstring& xmlData)
{
	_xmlString = TcharToUtf8(xmlData);
	return SaveToFile<std::string>(_xmlString, filename);
}

//***************************************************************************
// XML 출력 (디버깅용)
void CRapidXMLUtil::PrintXML() const
{
	std::cout << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;

	string xmlString;
	rapidxml::print(std::back_inserter(xmlString), _doc);

	_tcout << Utf8ToTchar(xmlString) << std::endl;
}

//***************************************************************************
// 재귀적으로 하위 노드 삭제
void CRapidXMLUtil::RemoveNodeRecursive(xml_node<char>* node)
{
	if( !node ) return;

	while( node->first_node() )
	{
		RemoveNodeRecursive(node->first_node());
	}

	if( node->parent() )
	{
		node->parent()->remove_node(node);
	}
}