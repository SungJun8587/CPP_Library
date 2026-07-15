
//***************************************************************************
// RapidXMLUtil.cpp: implementation of the CRapidXMLUtil class.
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
const TCHAR* CXMLNode::GetStringAttr(const TCHAR* ptszKey, const TCHAR* defaultValue)
{
	xml_attribute<>* attr = _node->first_attribute(TcharToUtf8(ptszKey).c_str());
	if( attr )
	{
		_tstring value = Utf8ToTchar(attr->value());
		if( value.size() > 0 )
			return value.c_str();
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
const TCHAR* CXMLNode::GetStringValue(const TCHAR* defaultValue)
{
	_tstring value = Utf8ToTchar(_node->value());
	if( value.size() > 0 )
		return value.c_str();

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
CVector<CXMLNode> CXMLNode::FindChildren(const TCHAR* ptszKey)
{
	CVector<CXMLNode> nodes;

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
// XML 파일 파싱하여 파싱 클래스인 CXMLNode 클래스에 할당
bool CRapidXMLUtil::ParseFromFile(const _tstring& filename, OUT CXMLNode& root)
{
	_xmlString = LoadFromFile<std::string>(filename);

	_doc.parse<0>(reinterpret_cast<char*>(&_xmlString[0]));
	root = CXMLNode(_doc.first_node());

	return true;
}

//***************************************************************************
// 현재 XML 다큐먼트 상태 파일에 저장
bool CRapidXMLUtil::SaveFile(const _tstring& filename)
{
	rapidxml::print(std::back_inserter(_xmlString), _doc);

	std::ofstream file(filename);
	if( !file.is_open() )
	{
		_tcerr << _T("Failed to open file for writing:") << filename << std::endl;
		return false;
	}

	file << _xmlString;
	file.close();

	return true;
}

//***************************************************************************
// XML에 문자열에 파일에 저장
bool CRapidXMLUtil::SaveFileToXML(const _tstring& filename, const _tstring& xmlData)
{
	_xmlString = TcharToUtf8(xmlData);

	std::ofstream file(filename);
	if( !file.is_open() )
	{
		_tcerr << _T("Failed to open file for writing:") << filename << std::endl;
		return false;
	}

	file << _xmlString;
	file.close();

	return true;
}

//***************************************************************************
// XML 출력(디버깅용)
void CRapidXMLUtil::PrintXML() const
{
	string xmlString;
	rapidxml::print(std::back_inserter(xmlString), _doc);

	_tcout << Utf8ToTchar(xmlString) << std::endl;
}

//***************************************************************************
// XML 헤더 설정
void CRapidXMLUtil::XMLDeclaration()
{
	rapidxml::xml_node<>* header = _doc.allocate_node(rapidxml::node_type::node_declaration);
	header->append_attribute(_doc.allocate_attribute("version", "1.0"));
	header->append_attribute(_doc.allocate_attribute("encoding", "utf-8"));
	_doc.append_node(header);
}

//***************************************************************************
// 부모 노드(parentNode)에 노드 추가
xml_node<>* CRapidXMLUtil::AddNode(xml_node<>* parentNode, const _tstring& nodeName)
{
	xml_node<>* node  = _doc.allocate_node(rapidxml::node_type::node_element, _doc.allocate_string(TcharToUtf8(nodeName).c_str()));
	parentNode->append_node(node);

	return node;
}

//***************************************************************************
// 노드 및 모든 하위 노드 삭제
void CRapidXMLUtil::RemoveNode(const _tstring& nodeName)
{
	xml_node<char>* root = _doc.first_node(TcharToUtf8(RootName).c_str());
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
// 특정 노드에 속성 추가
void CRapidXMLUtil::AddAttribute(xml_node<>* node, const _tstring& attName, const _tstring& attValue)
{
	string name = TcharToUtf8(attName);
	string value = TcharToUtf8(attValue);

	if( !node || node->first_attribute(name.c_str()) ) return; // 중복 방지

	char* attr_name = _doc.allocate_string(name.c_str());
	char* attr_value = _doc.allocate_string(value.c_str());

	xml_attribute<>* attr = _doc.allocate_attribute(attr_name, attr_value);
	node->append_attribute(attr);
}

//***************************************************************************
// 특정 노드의 속성 수정 (없으면 추가)
void CRapidXMLUtil::SetAttribute(xml_node<>* node, const _tstring& attName, const _tstring& attValue)
{
	if( !node ) return;

	xml_attribute<>* attr = node->first_attribute(TcharToUtf8(attName).c_str());
	if( attr ) 
	{
		attr->value(_doc.allocate_string(TcharToUtf8(attValue).c_str()));
	}
	else 
	{
		AddAttribute(node, attName, attValue);
	}
}

//***************************************************************************
// 특정 노드 및 모든 하위 노드에서 속성 삭제
void CRapidXMLUtil::RemoveAttribute(xml_node<>* node, const _tstring& attName)
{
	if( !node ) return;

	// 현재 노드에서 속성 삭제
	xml_attribute<>* attr = node->first_attribute(TcharToUtf8(attName).c_str());
	if( attr ) 
	{
		node->remove_attribute(attr);
	}

	// 모든 자식 노드에 대해 재귀적으로 수행
	for( xml_node<>* child = node->first_node(); child; child = child->next_sibling() )
	{
		RemoveAttribute(child, attName);
	}
}

//***************************************************************************
// 노드 문자열 값 추가
void CRapidXMLUtil::AddValue(const _tstring& str, xml_node<>* parent, const TCHAR* ptszTagName)
{
	xml_node<>* node = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TcharToUtf8(ptszTagName).c_str()), _doc.allocate_string(TcharToUtf8(str).c_str()));
	parent->append_node(node);
}

//***************************************************************************
// 노드 CData 문자열 추가(<![CDATA[ 내용 ]]>)
void CRapidXMLUtil::AddCDataValue(const _tstring& str, xml_node<>* parent, const TCHAR* ptszTagName)
{
	xml_node<>* node = _doc.allocate_node(node_type::node_cdata, _doc.allocate_string(TcharToUtf8(ptszTagName).c_str()), _doc.allocate_string(TcharToUtf8(str).c_str()));
	parent->append_node(node);
}

//***************************************************************************
// 노드 문자열 값 얻기
void CRapidXMLUtil::GetValue(_tstring& str, xml_node<>* node)
{
	if( node ) str = Utf8ToTchar(node->value());
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