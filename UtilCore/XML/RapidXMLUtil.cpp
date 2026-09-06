
//***************************************************************************
// RapidXMLUtil.cpp: implementation of the CRapidXMLUtil class.
//
//***************************************************************************

#include "pch.h"
#include "RapidXMLUtil.h"

// 실수 파싱/포맷팅에 사용하는 로케일. 소수점 구분자가 OS/사용자 로케일 설정에 따라
// 바뀌면(예: "," 를 소수점으로 쓰는 로케일) 동일한 XML 데이터를 환경에 따라 다르게
// 해석하는 문제가 생기므로, 실행 환경과 무관하게 항상 "C" 로케일(소수점 ".")로
// 고정하여 로케일 독립적으로 동작하도록 합니다.
_locale_t g_xmlNumericLocale = _create_locale(LC_NUMERIC, "C");

//***************************************************************************
// @brief 특정 속성(Attribute)의 불리언(bool) 값을 읽어옵니다.
// @param ptszKey 속성 키 이름
// @param defaultValue 실패 시 반환할 기본값
// @return 변환된 bool 값
//***************************************************************************
bool CXMLNode::GetBoolAttr(const TCHAR* ptszKey, bool defaultValue)
{
	if( _node == nullptr || ptszKey == nullptr ) return defaultValue;

	xml_attribute<>* attr = _node->first_attribute(TStringToUtf8(ptszKey).c_str());
	if( attr && attr->value() )
	{
		return (_stricmp(attr->value(), "true") == 0 || strcmp(attr->value(), "1") == 0);
	}

	return defaultValue;
}

//***************************************************************************
// @brief 특정 속성(Attribute)의 int8 값을 읽어옵니다.
// @param ptszKey 속성 키 이름
// @param defaultValue 실패 시 반환할 기본값
// @return 변환된 int8 값
//***************************************************************************
int8 CXMLNode::GetInt8Attr(const TCHAR* ptszKey, int8 defaultValue)
{
	if( _node == nullptr || ptszKey == nullptr ) return defaultValue;

	xml_attribute<>* attr = _node->first_attribute(TStringToUtf8(ptszKey).c_str());
	if( attr && attr->value() )
		return static_cast<int8>(atoi(attr->value()));

	return defaultValue;
}

//***************************************************************************
// @brief 특정 속성(Attribute)의 int16 값을 읽어옵니다.
// @param ptszKey 속성 키 이름
// @param defaultValue 실패 시 반환할 기본값
// @return 변환된 int16 값
//***************************************************************************
int16 CXMLNode::GetInt16Attr(const TCHAR* ptszKey, int16 defaultValue)
{
	if( _node == nullptr || ptszKey == nullptr ) return defaultValue;

	xml_attribute<>* attr = _node->first_attribute(TStringToUtf8(ptszKey).c_str());
	if( attr && attr->value() )
		return static_cast<int16>(atoi(attr->value()));

	return defaultValue;
}

//***************************************************************************
// @brief 특정 속성(Attribute)의 int32 값을 읽어옵니다.
// @param ptszKey 속성 키 이름
// @param defaultValue 실패 시 반환할 기본값
// @return 변환된 int32 값
//***************************************************************************
int32 CXMLNode::GetInt32Attr(const TCHAR* ptszKey, int32 defaultValue)
{
	if( _node == nullptr || ptszKey == nullptr ) return defaultValue;

	xml_attribute<>* attr = _node->first_attribute(TStringToUtf8(ptszKey).c_str());
	if( attr && attr->value() )
		return atoi(attr->value());

	return defaultValue;
}

//***************************************************************************
// @brief 특정 속성(Attribute)의 int64 값을 읽어옵니다.
// @param ptszKey 속성 키 이름
// @param defaultValue 실패 시 반환할 기본값
// @return 변환된 int64 값
//***************************************************************************
int64 CXMLNode::GetInt64Attr(const TCHAR* ptszKey, int64 defaultValue)
{
	if( _node == nullptr || ptszKey == nullptr ) return defaultValue;

	xml_attribute<>* attr = _node->first_attribute(TStringToUtf8(ptszKey).c_str());
	if( attr && attr->value() )
		return _atoi64(attr->value());

	return defaultValue;
}

//***************************************************************************
// @brief 특정 속성(Attribute)의 float 값을 읽어옵니다.
// @param ptszKey 속성 키 이름
// @param defaultValue 실패 시 반환할 기본값
// @return 변환된 float 값
//***************************************************************************
float CXMLNode::GetFloatAttr(const TCHAR* ptszKey, float defaultValue)
{
	if( _node == nullptr || ptszKey == nullptr ) return defaultValue;

	xml_attribute<>* attr = _node->first_attribute(TStringToUtf8(ptszKey).c_str());
	if( attr && attr->value() )
		return static_cast<float>(_atof_l(attr->value(), g_xmlNumericLocale));

	return defaultValue;
}

//***************************************************************************
// @brief 특정 속성(Attribute)의 double 값을 읽어옵니다.
// @param ptszKey 속성 키 이름
// @param defaultValue 실패 시 반환할 기본값
// @return 변환된 double 값
//***************************************************************************
double CXMLNode::GetDoubleAttr(const TCHAR* ptszKey, double defaultValue)
{
	if( _node == nullptr || ptszKey == nullptr ) return defaultValue;

	xml_attribute<>* attr = _node->first_attribute(TStringToUtf8(ptszKey).c_str());
	if( attr && attr->value() )
		return _atof_l(attr->value(), g_xmlNumericLocale);

	return defaultValue;
}

//***************************************************************************
// @brief 특정 속성(Attribute)의 문자열 값을 읽어옵니다.
// @param ptszKey 속성 키 이름
// @param defaultValue 실패 시 반환할 기본값
// @return 변환된 TCHAR 문자열 포인터 (스레드별 정적 버퍼를 가리키며, 같은 스레드의
//         다음 GetStringAttr/GetStringValue 호출 전까지만 유효합니다)
//***************************************************************************
const TCHAR* CXMLNode::GetStringAttr(const TCHAR* ptszKey, const TCHAR* defaultValue)
{
	if( _node == nullptr || ptszKey == nullptr ) return defaultValue;

	xml_attribute<>* attr = _node->first_attribute(TStringToUtf8(ptszKey).c_str());
	if( attr && attr->value() )
	{
		// 속성이 존재하면(값이 빈 문자열이더라도) 그 값을 그대로 반환합니다.
		// defaultValue는 속성 자체가 없을 때만 사용됩니다.
		thread_local static _tstring resultStr;
		resultStr = Utf8ToTString(attr->value());
		return resultStr.c_str();
	}
	return defaultValue;
}

//***************************************************************************
// @brief 노드의 텍스트 불리언(bool) 값을 읽어옵니다.
// @param defaultValue 실패 시 반환할 기본값
// @return 변환된 bool 값
//***************************************************************************
bool CXMLNode::GetBoolValue(bool defaultValue)
{
	if( _node == nullptr ) return defaultValue;

	char* val = _node->value();
	if( val )
		return (_stricmp(val, "true") == 0 || strcmp(val, "1") == 0);

	return defaultValue;
}

//***************************************************************************
// @brief 노드의 텍스트 int8 값을 읽어옵니다.
// @param defaultValue 실패 시 반환할 기본값
// @return 변환된 int8 값
//***************************************************************************
int8 CXMLNode::GetInt8Value(int8 defaultValue)
{
	if( _node == nullptr ) return defaultValue;

	char* val = _node->value();
	if( val )
		return static_cast<int8>(atoi(val));

	return defaultValue;
}

//***************************************************************************
// @brief 노드의 텍스트 int16 값을 읽어옵니다.
// @param defaultValue 실패 시 반환할 기본값
// @return 변환된 int16 값
//***************************************************************************
int16 CXMLNode::GetInt16Value(int16 defaultValue)
{
	if( _node == nullptr ) return defaultValue;

	char* val = _node->value();
	if( val )
		return static_cast<int16>(atoi(val));

	return defaultValue;
}

//***************************************************************************
// @brief 노드의 텍스트 int32 값을 읽어옵니다.
// @param defaultValue 실패 시 반환할 기본값
// @return 변환된 int32 값
//***************************************************************************
int32 CXMLNode::GetInt32Value(int32 defaultValue)
{
	if( _node == nullptr ) return defaultValue;

	char* val = _node->value();
	if( val )
		return static_cast<int32>(atoi(val));

	return defaultValue;
}

//***************************************************************************
// @brief 노드의 텍스트 int64 값을 읽어옵니다.
// @param defaultValue 실패 시 반환할 기본값
// @return 변환된 int64 값
//***************************************************************************
int64 CXMLNode::GetInt64Value(int64 defaultValue)
{
	if( _node == nullptr ) return defaultValue;

	char* val = _node->value();
	if( val )
		return static_cast<int64>(_atoi64(val));

	return defaultValue;
}

//***************************************************************************
// @brief 노드의 텍스트 float 값을 읽어옵니다.
// @param defaultValue 실패 시 반환할 기본값
// @return 변환된 float 값
//***************************************************************************
float CXMLNode::GetFloatValue(float defaultValue)
{
	if( _node == nullptr ) return defaultValue;

	char* val = _node->value();
	if( val )
		return static_cast<float>(_atof_l(val, g_xmlNumericLocale));

	return defaultValue;
}

//***************************************************************************
// @brief 노드의 텍스트 double 값을 읽어옵니다.
// @param defaultValue 실패 시 반환할 기본값
// @return 변환된 double 값
//***************************************************************************
double CXMLNode::GetDoubleValue(double defaultValue)
{
	if( _node == nullptr ) return defaultValue;

	char* val = _node->value();
	if( val )
		return ::_atof_l(val, g_xmlNumericLocale);

	return defaultValue;
}

//***************************************************************************
// @brief 노드의 텍스트 문자열 값을 읽어옵니다.
// @param defaultValue 실패 시 반환할 기본값
// @return 변환된 TCHAR 문자열 포인터 (스레드별 정적 버퍼를 가리키며, 같은 스레드의
//         다음 GetStringAttr/GetStringValue 호출 전까지만 유효합니다)
//***************************************************************************
const TCHAR* CXMLNode::GetStringValue(const TCHAR* defaultValue)
{
	if( _node == nullptr ) return defaultValue;

	char* val = _node->value();
	if( val )
	{
		// 노드가 존재하면(텍스트가 빈 문자열이더라도) 그 값을 그대로 반환합니다.
		thread_local static _tstring resultStr;
		resultStr = Utf8ToTString(val);
		return resultStr.c_str();
	}

	return defaultValue;
}

//***************************************************************************
// @brief 키 이름에 해당하는 첫 번째 자식 노드를 찾습니다.
// @param ptszKey 찾을 자식 노드 이름
// @return CXMLNode 객체 (실패 시 유효하지 않은 CXMLNode 반환)
//***************************************************************************
CXMLNode CXMLNode::FindChild(const TCHAR* ptszKey)
{
	if( _node == nullptr || ptszKey == nullptr ) return CXMLNode(nullptr);

	return CXMLNode(_node->first_node(TStringToUtf8(ptszKey).c_str()));
}

//***************************************************************************
// @brief 키 이름에 해당하는 모든 자식 노드 리스트를 찾습니다.
// @param ptszKey 찾을 자식 노드 이름
// @return 자식 노드들이 담긴 CVector<CXMLNode>
//***************************************************************************
CVector<CXMLNode> CXMLNode::FindChildren(const TCHAR* ptszKey)
{
	CVector<CXMLNode> nodes;

	if( _node == nullptr || ptszKey == nullptr ) return nodes;

	std::string utf8Key = TStringToUtf8(ptszKey);
	xml_node<>* node = _node->first_node(utf8Key.c_str());
	while( node )
	{
		nodes.push_back(CXMLNode(node));
		node = node->next_sibling(utf8Key.c_str());
	}

	return nodes;
}

//***************************************************************************
// @brief CRapidXMLUtil 기본 생성자
//***************************************************************************
CRapidXMLUtil::CRapidXMLUtil()
{
}

//***************************************************************************
// @brief XML 문자열 데이터를 전달받아 초기화하는 생성자
// @param xmlData 파싱할 XML 문자열
//***************************************************************************
CRapidXMLUtil::CRapidXMLUtil(const _tstring& xmlData)
{
	_xmlString = TStringToUtf8(xmlData);
	if( !_xmlString.empty() )
	{
		_doc.parse<0>(&_xmlString[0]);
	}
}

//***************************************************************************
// @brief 깊은 복사(Deep Copy) 생성자
// @param other 복사할 CRapidXMLUtil 객체
//***************************************************************************
CRapidXMLUtil::CRapidXMLUtil(const CRapidXMLUtil& other)
{
	_xmlString = other._xmlString;
	if( !_xmlString.empty() )
	{
		_doc.parse<0>(&_xmlString[0]);
	}
}

//***************************************************************************
// @brief CRapidXMLUtil 소멸자
//***************************************************************************
CRapidXMLUtil::~CRapidXMLUtil()
{
}

//***************************************************************************
// @brief XML 파일을 읽어서 CXMLNode 루트로 파싱합니다.
// @param filename 파일 경로
// @param root [OUT] 파싱 결과를 전달받을 CXMLNode 참조 객체
// @return 성공 시 true, 실패 시 false
// @detail 파일의 원문 내용을 있는 그대로 읽어 XML로 파싱합니다. (특정 T로 값을
//         역직렬화하는 LoadFromFile<T>와는 별개의 경로이며, 파일 내용이 임의의
//         XML 문서 형태여도 그대로 파싱할 수 있습니다.)
//***************************************************************************
bool CRapidXMLUtil::ParseFromFile(const _tstring& filename, OUT CXMLNode& root)
{
	std::ifstream file(filename, std::ios::binary);
	if( !file.is_open() )
	{
		_tcerr << _T("Failed to open file for reading: ") << filename << std::endl;
		return false;
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	_xmlString = buffer.str();
	if( _xmlString.empty() ) return false;

	_doc.clear();
	try
	{
		_doc.parse<0>(&_xmlString[0]);
	}
	catch( const parse_error& e )
	{
		_tcerr << _T("XML parse error : ") << e.what() << std::endl;
		return false;
	}

	root = CXMLNode(_doc.first_node());

	return true;
}

//***************************************************************************
// @brief 현재 XML Document 상태를 파일에 저장합니다.
// @param filename 저장할 파일 경로
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CRapidXMLUtil::SaveFile(const _tstring& filename)
{
	_xmlString.clear();
	rapidxml::print(std::back_inserter(_xmlString), _doc);

	std::ofstream file(filename);
	if( !file.is_open() )
	{
		_tcerr << _T("Failed to open file for writing: ") << filename << std::endl;
		return false;
	}

	file << _xmlString;
	file.close();

	return true;
}

//***************************************************************************
// @brief 특정 XML 문자열을 파일로 직접 저장합니다.
// @param filename 저장할 파일 경로
// @param xmlData 저장할 XML 문자열
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CRapidXMLUtil::SaveFileToXML(const _tstring& filename, const _tstring& xmlData)
{
	_xmlString = TStringToUtf8(xmlData);

	std::ofstream file(filename);
	if( !file.is_open() )
	{
		_tcerr << _T("Failed to open file for writing: ") << filename << std::endl;
		return false;
	}

	file << _xmlString;
	file.close();

	return true;
}

//***************************************************************************
// @brief 콘솔 화면에 현재 XML 문서 내용을 출력합니다 (디버깅용).
//***************************************************************************
void CRapidXMLUtil::PrintXML() const
{
	std::string xmlStr;
	rapidxml::print(std::back_inserter(xmlStr), _doc);

	_tcout << Utf8ToTString(xmlStr) << std::endl;
}

//***************************************************************************
// @brief XML 헤더 선언(<?xml version="1.0" encoding="utf-8"?>)을 추가합니다.
//***************************************************************************
void CRapidXMLUtil::XMLDeclaration()
{
	rapidxml::xml_node<>* header = _doc.allocate_node(rapidxml::node_type::node_declaration);
	header->append_attribute(_doc.allocate_attribute("version", "1.0"));
	header->append_attribute(_doc.allocate_attribute("encoding", "utf-8"));
	_doc.append_node(header);
}

//***************************************************************************
// @brief 부모 노드에 새로운 자식 노드를 추가합니다.
// @param parentNode 부모 노드 포인터
// @param nodeName 생성할 노드 이름
// @return 생성된 노드의 포인터
//***************************************************************************
xml_node<>* CRapidXMLUtil::AddNode(xml_node<>* parentNode, const _tstring& nodeName)
{
	if( parentNode == nullptr ) return nullptr;

	xml_node<>* node = _doc.allocate_node(rapidxml::node_type::node_element, _doc.allocate_string(TStringToUtf8(nodeName).c_str()));
	parentNode->append_node(node);

	return node;
}

//***************************************************************************
// @brief Root 노드 아래에서 특정 이름을 가진 노드를 찾아 삭제합니다.
// @param nodeName 삭제할 노드 이름
//***************************************************************************
void CRapidXMLUtil::RemoveNode(const _tstring& nodeName)
{
	xml_node<char>* root = _doc.first_node(TStringToUtf8(RootName).c_str());
	if( !root )
	{
		_tcout << _T("Root node not found") << std::endl;
		return;
	}

	xml_node<char>* targetNode = root->first_node(TStringToUtf8(nodeName).c_str());
	if( targetNode )
	{
		RemoveNodeRecursive(targetNode);
	}
	else
	{
		_tcout << _T("Node not found: ") << nodeName << std::endl;
	}
}

//***************************************************************************
// @brief 특정 노드에 속성을 추가합니다.
// @param node 대상 노드 포인터
// @param attName 속성 이름
// @param attValue 속성 값
//***************************************************************************
void CRapidXMLUtil::AddAttribute(xml_node<>* node, const _tstring& attName, const _tstring& attValue)
{
	if( node == nullptr ) return;

	std::string name = TStringToUtf8(attName);
	std::string value = TStringToUtf8(attValue);

	if( node->first_attribute(name.c_str()) ) return;

	char* attr_name = _doc.allocate_string(name.c_str());
	char* attr_value = _doc.allocate_string(value.c_str());

	xml_attribute<>* attr = _doc.allocate_attribute(attr_name, attr_value);
	node->append_attribute(attr);
}

//***************************************************************************
// @brief 특정 노드의 속성을 수정합니다 (없으면 신규 추가).
// @param node 대상 노드 포인터
// @param attName 속성 이름
// @param attValue 설정할 속성 값
//***************************************************************************
void CRapidXMLUtil::SetAttribute(xml_node<>* node, const _tstring& attName, const _tstring& attValue)
{
	if( node == nullptr ) return;

	xml_attribute<>* attr = node->first_attribute(TStringToUtf8(attName).c_str());
	if( attr )
	{
		attr->value(_doc.allocate_string(TStringToUtf8(attValue).c_str()));
	}
	else
	{
		AddAttribute(node, attName, attValue);
	}
}

//***************************************************************************
// @brief 특정 노드 및 해당 자식 노드 전체에서 지정한 속성을 제거합니다.
// @param node 대상 노드 포인터
// @param attName 삭제할 속성 이름
//***************************************************************************
void CRapidXMLUtil::RemoveAttribute(xml_node<>* node, const _tstring& attName)
{
	// attName의 UTF-8 변환은 한 번만 수행하고, 재귀 순회에는 변환된 문자열을 그대로 재사용합니다.
	RemoveAttributeUtf8(node, TStringToUtf8(attName));
}

//***************************************************************************
// @brief RemoveAttribute의 내부 구현. 이미 UTF-8로 변환된 속성 이름을 받아 재귀적으로 처리합니다.
// @param node 대상 노드 포인터
// @param utf8AttName UTF-8로 변환된 속성 이름
//***************************************************************************
void CRapidXMLUtil::RemoveAttributeUtf8(xml_node<>* node, const std::string& utf8AttName)
{
	if( node == nullptr ) return;

	xml_attribute<>* attr = node->first_attribute(utf8AttName.c_str());
	if( attr )
	{
		node->remove_attribute(attr);
	}

	for( xml_node<>* child = node->first_node(); child; child = child->next_sibling() )
	{
		RemoveAttributeUtf8(child, utf8AttName);
	}
}

//***************************************************************************
// @brief 부모 노드에 문자열 값을 가지는 요소를 추가합니다.
// @param str 설정할 문자열 값
// @param parent 부모 노드 포인터
// @param ptszTagName 생성할 태그 이름
//***************************************************************************
void CRapidXMLUtil::AddValue(const _tstring& str, xml_node<>* parent, const TCHAR* ptszTagName)
{
	if( parent == nullptr || ptszTagName == nullptr ) return;

	xml_node<>* node = _doc.allocate_node(node_type::node_element, _doc.allocate_string(TStringToUtf8(ptszTagName).c_str()), _doc.allocate_string(TStringToUtf8(str).c_str()));
	parent->append_node(node);
}

//***************************************************************************
// @brief 부모 노드에 CDATA 섹션(<![CDATA[ 내용 ]]>)을 추가합니다.
// @param str CDATA 섹션에 들어갈 문자열
// @param parent 부모 노드 포인터
// @param ptszTagName 생성할 태그 이름
//***************************************************************************
void CRapidXMLUtil::AddCDataValue(const _tstring& str, xml_node<>* parent, const TCHAR* ptszTagName)
{
	if( parent == nullptr || ptszTagName == nullptr ) return;

	xml_node<>* node = _doc.allocate_node(node_type::node_cdata, _doc.allocate_string(TStringToUtf8(ptszTagName).c_str()), _doc.allocate_string(TStringToUtf8(str).c_str()));
	parent->append_node(node);
}

//***************************************************************************
// @brief 지정한 노드의 텍스트 값을 얻어옵니다.
// @param str [OUT] 가져온 텍스트 값을 저장할 변수
// @param node 대상 노드 포인터
//***************************************************************************
void CRapidXMLUtil::GetValue(_tstring& str, xml_node<>* node)
{
	if( node && node->value() )
	{
		str = Utf8ToTString(node->value());
	}
}

//***************************************************************************
// @brief 지정한 노드 및 그 하위 자식 노드들을 재귀적으로 완전히 제거합니다.
// @param node 삭제할 노드 포인터
//***************************************************************************
void CRapidXMLUtil::RemoveNodeRecursive(xml_node<char>* node)
{
	if( node == nullptr ) return;

	while( node->first_node() )
	{
		RemoveNodeRecursive(node->first_node());
	}

	if( node->parent() )
	{
		node->parent()->remove_node(node);
	}
}