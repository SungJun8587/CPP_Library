
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

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h> 
#endif

#ifndef __ICONVUTIL_H__
#include <Util/IconvUtil.h> 
#endif

#include <vector>
#include <map>
#include <type_traits>

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

class CXMLNode
{
public:
	CXMLNode(xml_node<>* node = nullptr) : _node(node)
	{
		_unicodeToUtf8 = new Iconv::CIconvUtil("WCHAR_T", "UTF-8");		// WCHAR_T -> UTF-8 변환
		_utf8ToUnicode = new Iconv::CIconvUtil("UTF-8", "WCHAR_T");		// UTF-8 -> WCHAR_T 변환
	}

	~CXMLNode()
	{
		delete _utf8ToUnicode;
		delete _unicodeToUtf8;
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

	CXMLNode				FindChild(const TCHAR* ptszKey);
	std::vector<CXMLNode>	FindChildren(const TCHAR* ptszKey);

private:
	//***************************************************************************
	// TCHAR → UTF-8(char) 변환
	string TcharToUtf8(const _tstring& input) const
	{
#ifdef UNICODE
		return _unicodeToUtf8->WCharToUtf8(input);
#else
		return input;
#endif
	}

	//***************************************************************************
	// UTF-8(char) → TCHAR 변환
	_tstring Utf8ToTchar(const string& input) const
	{
#ifdef UNICODE
		return _utf8ToUnicode->Utf8ToWChar(input);
#else
		return input;
#endif
	}

private:
	rapidxml::xml_node<>*		_node = nullptr;

	Iconv::CIconvUtil* _unicodeToUtf8;	// WCHAR_T -> UTF-8 변환
	Iconv::CIconvUtil* _utf8ToUnicode;	// UTF-8 -> WCHAR_T 변환
};

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
	inline void AddVector(const std::vector<T>& container, xml_node<>* parent, const TCHAR* ptszTagName = VectorName);

	template <typename T>
	inline void GetVector(std::vector<T>& container, xml_node<>* parent, const TCHAR* ptszTagName = VectorName);

	template <typename T>
	inline void AddObjectVector(const std::vector<T>& container, xml_node<>* parent, const TCHAR* ptszTagName = VectorName);

	template <typename T>
	inline void GetObjectVector(std::vector<T>& container, xml_node<>* parent, const TCHAR* ptszTagName = VectorName);

	template <typename K, typename V>
	inline void AddMap(const std::map<K, V>& container, xml_node<>* parent, const TCHAR* ptszTagName = MapName);

	template <typename K, typename V>
	inline void GetMap(std::map<K, V>& container, xml_node<>* parent, const TCHAR* ptszTagName = MapName);

	template <typename K, typename V>
	inline void AddObjectMap(const std::map<K, V>& container, xml_node<>* parent, const TCHAR* ptszTagName = MapName);

	template <typename K, typename V>
	inline void GetObjectMap(std::map<K, V>& container, xml_node<>* parent, const TCHAR* ptszTagName = MapName);

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
		string nodeName;

#ifdef UNICODE
		nodeName = Iconv::CIconvUtil::ConvertEncoding(ptszTagName, "WCHAR_T", "UTF-8");
#else
		nodeName = ptszTagName;
#endif

		xml_node<>* node = doc.allocate_node(node_type::node_element, doc.allocate_string(nodeName.c_str()), doc.allocate_string(std::to_string(value).c_str()));
		parent->append_node(node);
	}

	//***************************************************************************
	// 문자열 타입 직렬화 함수
	static void Serialize(const _tstring& value, xml_node<>* parent, xml_document<>& doc, const TCHAR* ptszTagName = ItemName)
	{
		string nodeName;
		string nodeValue;

#ifdef UNICODE
		nodeName = Iconv::CIconvUtil::ConvertEncoding(ptszTagName, "WCHAR_T", "UTF-8");
		nodeValue = Iconv::CIconvUtil::ConvertEncoding(value, "WCHAR_T", "UTF-8");
#else
		nodeName = ptszTagName;
		nodeValue = value;
#endif
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
#ifdef UNICODE
			if( node ) value = Iconv::CIconvUtil::ConvertEncodingW(node->value(), "UTF-8", "WCHAR_T");
#else
			if( node ) value = node->value();
#endif
		}
	}

private:
	//***************************************************************************
	// TCHAR → UTF-8(char) 변환
	string TcharToUtf8(const _tstring& input) const
	{
#ifdef UNICODE
		return _unicodeToUtf8->WCharToUtf8(input);
#else
		return input;
#endif
	}

	//***************************************************************************
	// UTF-8(char) → TCHAR 변환
	_tstring Utf8ToTchar(const string& input) const
	{
#ifdef UNICODE
		return _utf8ToUnicode->Utf8ToWChar(input);
#else
		return input;
#endif	
	}

private:
	// 벡터 타입 확인
	template <typename T>
	struct is_vector : std::false_type {};

	template <typename T, typename Alloc>
	struct is_vector<std::vector<T, Alloc>> : std::true_type {};

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

	Iconv::CIconvUtil* _unicodeToUtf8;	// WCHAR_T -> UTF-8 변환
	Iconv::CIconvUtil* _utf8ToUnicode;	// UTF-8 -> WCHAR_T 변환
};

#include "XML/RapidXMLUtil.inl"

#endif // ndef __RAPIDXMLUTIL_H__