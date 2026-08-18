
//***************************************************************************
// RapidXMLUtil.h : interface for the CRapidXMLUtil class.
//
//***************************************************************************

#ifndef __RAPIDXMLUTIL_H__
#define __RAPIDXMLUTIL_H__

#pragma once

#ifndef	_INC_TCHAR
#include <tchar.h>
#endif

#include <string>
#include <vector>
#include <map>
#include <type_traits>

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h> 
#endif

#ifndef __ENCODINGCONVERT_H__
#include <Util/EncodingConvert.h>
#endif

#include <rapidxml.hpp>
#include <rapidxml_utils.hpp>
#include <rapidxml_print.hpp>

using namespace rapidxml;

#define RootName		_T("Root")
#define VectorName		_T("Vector")
#define MapName			_T("Map")
#define MapKey			_T("Key")
#define MapValue		_T("Value")
#define ItemName		_T("Item")

//***************************************************************************
// @class CXMLNode
// @brief RapidXML의 xml_node<> 포인터를 래핑하여 안전하고 편리한 데이터 접근을 제공하는 클래스입니다.
//
// @details
// RapidXML 노드 개체에 대한 직접적인 접근을 추상화하여, 속성(Attribute) 및 
// 노드 값(Value)을 다양한 기본 자료형(bool, int, float, double, _tstring 등)으로 
// 안전하게 변환하여 읽을 수 있는 인터페이스를 제공합니다.
//
// 주요 처리 및 특징:
//  - RapidXML raw 포인터(xml_node<>*) 캡슐화 및 Null 포인터 안정성 제공 (IsValid)
//  - 다양한 타입별 속성(Get*Attr) 및 노드 값(Get*Value) 추출 기능 지원
//  - 단일 자식 노드 탐색(FindChild) 및 목록 형태의 다중 자식 노드 탐색(FindChildren) 지원
//***************************************************************************
class CXMLNode
{
public:
	CXMLNode(xml_node<>* node = nullptr) : _node(node)
	{
	}

	~CXMLNode()
	{
	}

	bool IsValid()
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
	const TCHAR*		GetStringValue(const TCHAR* defaultValue = _T(""));

	CXMLNode			FindChild(const TCHAR* ptszKey);
	CVector<CXMLNode>	FindChildren(const TCHAR* ptszKey);

private:
	rapidxml::xml_node<>*		_node = nullptr;
};

//***************************************************************************
// @class CRapidXMLUtil
// @brief RapidXML 라이브러리를 기반으로 XML 파싱, 생성, 수정 및 직렬화/역직렬화를 관리하는 유틸리티 클래스입니다.
//
// @details
// XML 문서의 파일 입출력(ParseFromFile, SaveFile), 노드/속성 편집 및 C++ 데이터 구조체,
// 컨테이너(CVector, CMap)의 자동 XML 직렬화/역직렬화 기능을 종합적으로 관리합니다.
// 내부적으로 TCHAR 문자열과 UTF-8 간 인코딩 변환을 자동으로 수행합니다.
//
// 주요 처리 및 특징:
//  - XML 파일 및 문자열 파싱, 포맷팅 저장/출력 기능 제공
//  - 노드, 속성(Attribute), CData의 동적 추가/수정/삭제 관리
//  - C++ 기본 자료형, 구조체 및 컨테이너(CVector, CMap)의 직렬화/역직렬화 템플릿 지원
//  - operator[] 연산자 오버로딩 및 Proxy 개체를 통한 직관적인 데이터 접근 지원
//***************************************************************************
class CRapidXMLUtil
{
public:
	CRapidXMLUtil();
	CRapidXMLUtil(const _tstring& xmlData);
	CRapidXMLUtil(const CRapidXMLUtil& other);
	~CRapidXMLUtil();

	rapidxml::xml_document<>& GetDocument() { 
		return _doc;  
	}

	xml_node<char>* GetRootNode() {
		// 첫 번째 노드를 가져와서 root 노드를 반환
		return _doc.first_node();
	}

	void Clear() {
		_doc.clear();
	}

	bool ParseFromFile(const _tstring& filename, OUT CXMLNode& root);
	bool SaveFile(const _tstring& filename);
	bool SaveFileToXML(const _tstring& filename, const _tstring& xmlData);
	void PrintXML() const;

	void XMLDeclaration();

	xml_node<>* AddNode(xml_node<>* parentNode, const _tstring& nodeName);
	void RemoveNode(const _tstring& nodeName);

	void AddAttribute(xml_node<>* node, const _tstring& attName, const _tstring& attValue);
	void SetAttribute(xml_node<>* node, const _tstring& attName, const _tstring& attValue);
	void RemoveAttribute(xml_node<>* node, const _tstring& attName);

	void AddValue(const _tstring& str, xml_node<>* parent, const TCHAR* ptszTagName = ItemName);
	void AddCDataValue(const _tstring& str, xml_node<>* parent, const TCHAR* ptszTagName);
	void GetValue(_tstring& str, xml_node<>* node);

	void RemoveNodeRecursive(xml_node<char>* node);

	//***************************************************************************
	// CRapidXMLUtil 클래스 operator[] Setter, Getter Operator Overloading을 위한 프록시 클래스
	class Proxy 
	{
		private:
			CRapidXMLUtil&	_xmlUtil;
			_tstring		_nodeName;

		public:
			Proxy(CRapidXMLUtil& xmlUtil, const _tstring& nodeName) : _xmlUtil(xmlUtil), _nodeName(nodeName) {}

			// = 연산자 오버로딩(값 설정)
			template <typename T>
			Proxy& operator=(const T& value) {
				_xmlUtil.ConvertToXML(_nodeName, value);
				return *this;
			}

			// T() 연산자 오버로딩(값 읽기)
			template <typename T>
			operator T() const { return _xmlUtil.ConvertFromXML<T>(_nodeName).value(); }
	};

	Proxy operator[](const TCHAR* key) {
		return Proxy(*this, key);
	}

	Proxy operator[](const _tstring& key) {
		return operator[](key.c_str());
	}

	template <typename T>
	inline const CRapidXMLUtil& operator=(const T& value);

	template <typename T>
	inline bool SaveToFile(const T& obj, const _tstring& filename);

	template <typename T>
	inline T LoadFromFile(const _tstring& filename);

	template <typename T>
	void AddNode(const _tstring& nodeName, const T& obj);

	template <typename T>
	void UpdateNode(const _tstring& nodeName, const T& obj);

	template <typename T>
	inline std::string SerializeWithIndent(const T& obj);

	template <typename T>
	inline std::string Serialize(const T& obj);

	template <typename T>
	inline T Deserialize(const std::string& xml);

	template <typename T>
	inline void AddObject(const T& obj, xml_node<>* parent, const TCHAR* ptszTagName = ItemName);

	template <typename T>
	inline void GetObject(T& obj, xml_node<>* node);

	template <typename T>
	inline void AddVector(const CVector<T>& container, xml_node<>* parent, const TCHAR* ptszTagName = VectorName);

	template <typename T>
	inline void GetVector(CVector<T>& container, xml_node<>* parent, const TCHAR* ptszTagName = VectorName);

	template <typename T>
	inline void AddObjectVector(const CVector<T>& container, xml_node<>* parent, const TCHAR* ptszTagName = VectorName);

	template <typename T>
	inline void GetObjectVector(CVector<T>& container, xml_node<>* parent, const TCHAR* ptszTagName = VectorName);

	template <typename K, typename V>
	inline void AddMap(const CMap<K, V>& container, xml_node<>* parent, const TCHAR* ptszTagName = MapName);

	template <typename K, typename V>
	inline void GetMap(CMap<K, V>& container, xml_node<>* parent, const TCHAR* ptszTagName = MapName);

	template <typename K, typename V>
	inline void AddObjectMap(const CMap<K, V>& container, xml_node<>* parent, const TCHAR* ptszTagName = MapName);

	template <typename K, typename V>
	inline void GetObjectMap(CMap<K, V>& container, xml_node<>* parent, const TCHAR* ptszTagName = MapName);

	template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
	inline void AddValue(const T& value, xml_node<>* parent, const TCHAR* ptszTagName = ItemName);

	template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
	inline void GetValue(T& value, xml_node<>* node);

	// C++ 구조체 → XML 변환(해당 노드명에 값을 할당하여 생성한 후에 XML 문자열을 반환) 
	template <typename T>
	inline _tstring ConvertToXML(const _tstring& nodeName, const T& obj);

	// XML → C++ 구조체 변환(해당 노드명에 값을 std::optional<T> 변수에 할당) 
	template <typename T>
	inline std::optional<T> ConvertFromXML(const _tstring& nodeName);

	//***************************************************************************
	// 숫자 타입 직렬화 함수
	template <typename T>
	static void Serialize(const T& value, xml_node<>* parent, xml_document<>& doc, const TCHAR* ptszTagName = ItemName)
	{
		std::string nodeName = TStringToUtf8(ptszTagName);

		xml_node<>* node = doc.allocate_node(node_type::node_element, doc.allocate_string(nodeName.c_str()), doc.allocate_string(std::to_string(value).c_str()));
		parent->append_node(node);
	}

	//***************************************************************************
	// 문자열 타입 직렬화 함수
	static void Serialize(const _tstring& value, xml_node<>* parent, xml_document<>& doc, const TCHAR* ptszTagName = ItemName)
	{
		std::string nodeName = TStringToUtf8(ptszTagName);
		std::string nodeValue = TStringToUtf8(value);

		xml_node<>* node = doc.allocate_node(node_type::node_element, doc.allocate_string(nodeName.c_str()), doc.allocate_string(nodeValue.c_str()));
		parent->append_node(node);
	}

	//***************************************************************************
	// 숫자, 문자열 타입 역직렬화 함수
	template <typename T>
	static void Deserialize(T& value, xml_node<>* node)
	{
		if constexpr( std::is_arithmetic<T>::value )
		{
			if( node ) value = static_cast<T>(std::stod(node->value()));
		}
		else if constexpr( std::is_same_v<T, _tstring> )
		{
			if( node ) value = Utf8ToTString(node->value());
		}
	}

private:
	// 벡터 타입 확인
	template <typename T>
	struct is_vector : std::false_type {};

	template <typename T, typename Alloc>
	struct is_vector<CVector<T, Alloc>> : std::true_type {};

	// 맵 타입 확인
	template <typename T>
	struct is_map : std::false_type {};

	template <typename K, typename V, typename Comp, typename Alloc>
	struct is_map<std::map<K, V, Comp, Alloc>> : std::true_type {};

	// T에 ToXML 멤버 함수가 있는지 확인하는 타입 트레이트
	template <typename T, typename = void>
	struct has_toxml_method : std::false_type {};

	template <typename T>
	struct has_toxml_method<T, std::void_t<decltype(std::declval<T>().ToXML(std::declval<xml_node<>*>(), std::declval<xml_document<>&>()))>> : std::true_type {};

	// 컴파일 타임 에러 유도용 유틸리티
	template <typename T>
	struct dependent_false : std::false_type {};

private:
	rapidxml::xml_document<>	_doc;			// XML은 항상 char 기반 저장
	std::string					_xmlString;     // UTF-8로 저장
};

#include "XML/RapidXMLUtil.inl"

#endif // ndef __RAPIDXMLUTIL_H__