
//***************************************************************************
// XMLConfigFieldWriter.h : interface for the CXMLConfigFieldWriter class.
//
//***************************************************************************

#ifndef UC_XMLCONFIGFIELDWRITER_H
#define UC_XMLCONFIGFIELDWRITER_H

#include <ConfigFile/ConfigFieldCommon.h>
#include <XML/RapidXMLUtil.h>

#include <charconv>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

//***************************************************************************
// @class CXMLConfigFieldWriter
// @brief CRapidXMLUtil 문서에 설정 필드를 쓰고 파일로 저장하는 도우미. CXMLConfigFieldReader의 짝.
// @detail
// 읽기 쪽과 같은 규칙으로 대상 변수의 타입에 따라 오버로드를 고른다.
//     writer.Write(_T("Port"), port);					// 모든 정수형(8~64비트, 부호 유무 무관), float/double
//     writer.Write(_T("Name"), name);					// _tstring, const TCHAR*, TCHAR 배열
//     writer.Write(_T("Verbose"), verbose);			// bool (true/false)
//     writer.Write(_T("Ports"), ports);				// 위 타입의 vector 계열 -> <Ports><Item>..</Item>..</Ports>
//     writer.WriteObjects(_T("Nodes"), nodes);			// ToXML을 가진 사용자 타입의 목록
//     writer.Save(_T("Config\\server.xml"));			// 임시 파일에 쓴 뒤 교체
//
// CRapidXMLUtil::AddValue / SaveFile 대신 이 클래스를 쓰는 이유:
//     - 숫자는 std::to_chars로 로케일과 무관하게 기록하며, float/double은 다시 읽었을 때 같은 값이 되는 가장 짧은 표현이다
//       (std::to_string은 0.1f를 "0.100000"으로 기록하고 작은/큰 값은 잘라낸다).
//     - int8/uint8/long 등 모든 정수형을 쓸 수 있다.
//     - NaN/Inf, XML이 표현할 수 없는 제어 문자, XML 이름으로 쓸 수 없는 키는 문서를 망가뜨리기 전에 오류로 기록한다.
//     - 같은 키를 다시 쓰면 요소 순서를 유지한 채 값만 교체한다.
//     - 저장은 임시 파일에 쓴 뒤 교체하므로, 저장 도중 실패해도 기존 파일이 잘리지 않는다.
//     - 파일은 UTF-8(BOM 없음), LF 줄바꿈, 선언 포함, 들여쓰기 적용으로 저장한다.
// 키는 루트 요소의 자식으로 평면 기록한다. 문서에 루트 요소가 없으면 <Root>를 만든다.
//***************************************************************************
class CXMLConfigFieldWriter : public CConfigErrorLog
{
	template <typename T> using Identity = ConfigField::Identity<T>;	// 템플릿 인자 추론에서 제외하기 위한 도구
	template <typename T> static constexpr bool IsNumeric = ConfigField::IsNumeric<T>;	// bool을 제외한 산술 타입인지
	template <typename C> static constexpr bool IsScalarList = ConfigField::IsScalarList<C>;	// 숫자/bool/문자열을 담는 컨테이너인지

public:
	//***************************************************************************
	// @brief 
	// @param xml 쓸 문서. 비어 있으면 선언과 <Root>를 만들고, 이미 루트가 있으면 그 아래에 이어서 씁니다
	//***************************************************************************
	explicit CXMLConfigFieldWriter(CRapidXMLUtil& xml) : _xml(xml), _root(nullptr)
	{
		for( rapidxml::xml_node<>* node = xml.GetDocument().first_node(); node != nullptr; node = node->next_sibling() )
		{
			if( node->type() == rapidxml::node_type::node_element )
			{
				_root = node;
				break;
			}
		}

		if( _root == nullptr )
		{
			if( xml.GetDocument().first_node() == nullptr )
				xml.XMLDeclaration();
			_root = xml.AddNode(&xml.GetDocument(), RootName);
		}
	}

	//***************************************************************************
	// @brief 정수/실수 값을 씁니다.
	// @param key 기록할 키 이름
	// @param value 기록할 정수/실수 값
	// @return 기록에 성공하면 true, NaN/Inf면 false(오류 기록)
	//***************************************************************************
	template <typename T, std::enable_if_t<IsNumeric<T>, int> = 0>
	bool Write(const TCHAR* key, const T& value)
	{
		std::string text;
		if( !MakeNumber(value, text) )
			return Fail(key, _T("number is not finite (NaN/Inf cannot be written)"));

		return PutScalar(key, text);
	}

	//***************************************************************************
	// @brief bool 값을 true/false로 씁니다.
	// @param key 기록할 키 이름
	// @param value 기록할 값
	// @return 기록에 성공하면 true, 키가 XML 요소 이름으로 쓸 수 없으면 false(오류 기록)
	//***************************************************************************
	bool Write(const TCHAR* key, bool value)
	{
		return PutScalar(key, value ? "true" : "false");
	}

	//***************************************************************************
	// @brief 문자열을 씁니다.
	// @param key 기록할 키 이름
	// @param value 기록할 문자열
	// @return 기록에 성공하면 true, XML이 표현할 수 없는 문자가 있거나 키가 잘못되면 false(오류 기록)
	//***************************************************************************
	bool Write(const TCHAR* key, const _tstring& value)
	{
		return PutString(key, value.c_str());
	}

	//***************************************************************************
	// @brief 문자열을 씁니다. TCHAR 배열과 문자열 리터럴도 이 오버로드로 들어오며, 널 문자까지만 기록한다.
	// @param key 기록할 키 이름
	// @param value 기록할 널 종료 문자열. nullptr이면 빈 문자열
	// @return 기록에 성공하면 true, XML이 표현할 수 없는 문자가 있거나 키가 잘못되면 false(오류 기록)
	//***************************************************************************
	bool Write(const TCHAR* key, const TCHAR* value)
	{
		return PutString(key, value != nullptr ? value : _T(""));
	}

	//***************************************************************************
	// @brief 숫자/bool/문자열 컨테이너(std::vector, CVector 등)를 <키><Item>..</Item>..</키>로 씁니다.
	// @param key 기록할 키 이름
	// @param values 기록할 컨테이너
	// @return 기록에 성공하면 true, 키가 잘못되었거나 원소를 쓸 수 없으면 false(오류 기록, 문서는 변경하지 않음)
	//***************************************************************************
	template <typename C, std::enable_if_t<IsScalarList<C>, int> = 0>
	bool Write(const TCHAR* key, const C& values)
	{
		using E = typename C::value_type;

		std::string name;
		if( !CheckKey(key, name) )
			return false;

		rapidxml::xml_document<>& doc = _xml.GetDocument();
		rapidxml::xml_node<>* list = doc.allocate_node(rapidxml::node_type::node_element, doc.allocate_string(name.c_str()));
		const std::string itemTag = TStringToUtf8(ItemName);

		uint32 index = 0;
		for( const auto& element : values )
		{
			std::string text;
			TCHAR itemName[128];
			_sntprintf_s(itemName, _countof(itemName), _TRUNCATE, _T("%s[%u]"), key, index);

			if constexpr( std::is_same_v<E, _tstring> )
			{
				if( !ToText(element.c_str(), text) )
					return Fail(itemName, _T("string contains a character that XML cannot represent"));
			}
			else if constexpr( std::is_same_v<E, bool> )
			{
				text = static_cast<bool>(element) ? "true" : "false";
			}
			else if( !MakeNumber(static_cast<E>(element), text) )
			{
				return Fail(itemName, _T("number is not finite (NaN/Inf cannot be written)"));
			}

			list->append_node(doc.allocate_node(rapidxml::node_type::node_element, doc.allocate_string(itemTag.c_str()), doc.allocate_string(text.c_str())));
			++index;
		}

		Place(name, list);
		return true;
	}

	//***************************************************************************
	// @brief ToXML을 가진 사용자 타입 객체 하나를 <키>..</키>로 씁니다.
	// @param key 기록할 키 이름
	// @param object 기록할 객체. ToXML 멤버가 필요
	// @return 기록에 성공하면 true, 키가 XML 요소 이름으로 쓸 수 없으면 false(오류 기록)
	//***************************************************************************
	template <typename T>
	bool WriteObject(const TCHAR* key, const T& object)
	{
		static_assert(HasToXML<T>::value, "WriteObject: T needs 'void ToXML(xml_node<>*, xml_document<>&) const'");

		std::string name;
		if( !CheckKey(key, name) )
			return false;

		_xml.AddObject(object, _root, key);
		DetachLastAndPlace(name);
		return true;
	}

	//***************************************************************************
	// @brief ToXML을 가진 사용자 타입 컨테이너를 <키><Item>..</Item>..</키>로 씁니다.
	// @param key 기록할 키 이름
	// @param objects 기록할 객체 컨테이너. 원소에 ToXML 멤버가 필요
	// @return 기록에 성공하면 true, 키가 XML 요소 이름으로 쓸 수 없으면 false(오류 기록)
	//***************************************************************************
	template <typename C>
	bool WriteObjects(const TCHAR* key, const C& objects)
	{
		static_assert(HasToXML<typename C::value_type>::value, "WriteObjects: element needs 'void ToXML(xml_node<>*, xml_document<>&) const'");

		std::string name;
		if( !CheckKey(key, name) )
			return false;

		_xml.AddObjectVector(objects, _root, key);
		DetachLastAndPlace(name);
		return true;
	}

	//***************************************************************************
	// @brief 문서를 파일로 저장합니다. (선언 + 들여쓰기, UTF-8 BOM 없음)
	// @detail path + ".tmp"에 먼저 쓰고 성공하면 path로 교체한다. 쓰기 오류가 기록돼 있으면 저장하지 않는다.
	// @param path 저장할 파일 경로
	// @return 저장에 성공하면 true, 쓰기 오류가 기록돼 있거나 저장에 실패하면 false(기존 파일은 그대로 남음)
	//***************************************************************************
	bool Save(const TCHAR* path)
	{
		if( !_errors.empty() )
		{
			ReportErrors();
			return false;
		}

		std::string xmlText;
		rapidxml::print(std::back_inserter(xmlText), _xml.GetDocument());

		const _tstring temp = _tstring(path) + _T(".tmp");
		{
			std::ofstream file(temp, std::ios::binary | std::ios::trunc);
			if( !file.is_open() )
			{
				LOG_ERROR(_T("[Config] save failed. cannot open %s"), temp.c_str());
				return false;
			}

			file.write(xmlText.data(), static_cast<std::streamsize>(xmlText.size()));
			file.flush();
			if( !file.good() )
			{
				file.close();
				LOG_ERROR(_T("[Config] save failed. write error. path=%s"), temp.c_str());
				ConfigField::RemoveFile(temp.c_str());
				return false;
			}
		}

		if( !ConfigField::ReplaceFile(temp.c_str(), path) )
		{
			LOG_ERROR(_T("[Config] replace failed. path=%s"), path);
			ConfigField::RemoveFile(temp.c_str());
			return false;
		}
		return true;
	}

	//***************************************************************************
	// @brief 기록된 쓰기 오류를 로그로 남깁니다.
	//***************************************************************************
	void ReportErrors() const
	{
		LogErrors(_T("write failed"));
	}

private:
	//***************************************************************************
	// @struct HasToXML
	// @brief T가 ToXML 멤버 함수를 가지고 있는지 판별하는 타입 트레이트.
	// @detail const T 객체에서 void ToXML(xml_node<>*, xml_document<>&) const를 호출할 수 있으면 true. WriteObject/WriteObjects의 static_assert에 사용한다.
	//***************************************************************************
	template <typename T, typename = void>
	struct HasToXML : std::false_type {};

	template <typename T>
	struct HasToXML<T, std::void_t<decltype(std::declval<const T&>().ToXML(std::declval<rapidxml::xml_node<>*>(), std::declval<rapidxml::xml_document<>&>()))>> : std::true_type {};

	//***************************************************************************
	// @brief 숫자를 텍스트로 만듭니다. 정수는 그대로, 실수는 왕복되는 가장 짧은 표현(로케일 무관)입니다.
	// @param value 변환할 숫자
	// @param out [OUT] 만들어진 텍스트
	// @return 변환에 성공하면 true, NaN/Inf면 false
	//***************************************************************************
	template <typename T>
	static bool MakeNumber(T value, std::string& out)
	{
		char buffer[64];
		std::to_chars_result r{};

		if constexpr( std::is_floating_point_v<T> )
		{
			if( !std::isfinite(value) )
				return false;
			r = std::to_chars(buffer, buffer + sizeof(buffer), value);
		}
		else if constexpr( std::is_signed_v<T> )
		{
			r = std::to_chars(buffer, buffer + sizeof(buffer), static_cast<long long>(value));
		}
		else
		{
			r = std::to_chars(buffer, buffer + sizeof(buffer), static_cast<unsigned long long>(value));
		}

		if( r.ec != std::errc() )
			return false;
		out.assign(buffer, r.ptr);
		return true;
	}

	//***************************************************************************
	// @brief TCHAR 문자열을 UTF-8로 바꾸며 XML 1.0이 표현할 수 없는 제어 문자(탭/개행 제외)를 거부합니다.
	// @param value 변환할 문자열
	// @param out [OUT] UTF-8로 변환된 문자열
	// @return XML에 쓸 수 있으면 true, 표현할 수 없는 제어 문자가 있으면 false
	//***************************************************************************
	static bool ToText(const TCHAR* value, std::string& out)
	{
		out = TStringToUtf8(value);
		for( char c : out )
		{
			const unsigned char u = static_cast<unsigned char>(c);
			if( u < 0x20 && u != '\t' && u != '\n' )		// '\r'은 XML 파서가 '\n'으로 바꿔 읽으므로 거부한다
				return false;
		}
		return true;
	}

	//***************************************************************************
	// @brief 키가 XML 요소 이름으로 쓸 수 있는지 검사하고 UTF-8 이름을 돌려줍니다.
	// @detail 영문자/밑줄로 시작하고 영문자/숫자/밑줄/하이픈/마침표가 이어져야 한다(비ASCII 바이트는 허용).
	//          콜론은 네임스페이스와 혼동되므로 거부한다.
	// @param key 기록할 키 이름
	// @param name [OUT] UTF-8로 변환된 요소 이름
	// @return 요소 이름으로 쓸 수 있으면 true, 아니면 false(오류 기록)
	//***************************************************************************
	bool CheckKey(const TCHAR* key, std::string& name)
	{
		name = TStringToUtf8(key);
		bool valid = !name.empty();
		for( size_t i = 0; valid && i < name.size(); ++i )
		{
			const unsigned char c = static_cast<unsigned char>(name[i]);
			const bool alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c >= 0x80;
			const bool other = (c >= '0' && c <= '9') || c == '-' || c == '.';
			valid = i == 0 ? alpha : (alpha || other);
		}
		if( !valid )
			return Fail(key, _T("is not a valid XML element name"));
		return true;
	}

	//***************************************************************************
	// @brief 스칼라 값을 가진 요소를 루트에 넣습니다. 같은 이름의 요소가 있으면 그 자리에서 교체합니다.
	// @param key 기록할 키 이름
	// @param text 요소의 텍스트(UTF-8)
	// @return 기록에 성공하면 true, 키가 잘못되면 false(오류 기록)
	//***************************************************************************
	bool PutScalar(const TCHAR* key, const std::string& text)
	{
		std::string name;
		if( !CheckKey(key, name) )
			return false;

		rapidxml::xml_document<>& doc = _xml.GetDocument();
		Place(name, doc.allocate_node(rapidxml::node_type::node_element, doc.allocate_string(name.c_str()), doc.allocate_string(text.c_str())));
		return true;
	}

	//***************************************************************************
	// @brief 문자열 값을 검사한 뒤 스칼라 요소로 넣습니다.
	// @param key 기록할 키 이름
	// @param value 기록할 널 종료 문자열
	// @return 기록에 성공하면 true, 표현할 수 없는 문자가 있거나 키가 잘못되면 false(오류 기록)
	//***************************************************************************
	bool PutString(const TCHAR* key, const TCHAR* value)
	{
		std::string text;
		if( !ToText(value, text) )
			return Fail(key, _T("string contains a character that XML cannot represent"));
		return PutScalar(key, text);
	}

	//***************************************************************************
	// @brief 분리된 노드를 루트에 넣습니다. 같은 이름의 요소가 이미 있으면 그 자리에 넣고 기존 요소는 모두 제거합니다.
	// @param name 요소 이름(UTF-8)
	// @param node 루트에 넣을, 부모가 없는 요소
	//***************************************************************************
	void Place(const std::string& name, rapidxml::xml_node<>* node)
	{
		rapidxml::xml_node<>* old = _root->first_node(name.c_str());
		if( old == nullptr )
		{
			_root->append_node(node);
			return;
		}

		_root->insert_node(old, node);
		while( old != nullptr )
		{
			rapidxml::xml_node<>* next = old->next_sibling(name.c_str());
			_root->remove_node(old);
			old = next;
		}
	}

	//***************************************************************************
	// @brief CRapidXMLUtil::AddObject* 가 루트 끝에 붙인 요소를 떼어내 Place로 제자리에 넣습니다.
	// @param name 요소 이름(UTF-8)
	//***************************************************************************
	void DetachLastAndPlace(const std::string& name)
	{
		rapidxml::xml_node<>* added = _root->last_node();
		_root->remove_node(added);
		Place(name, added);
	}

private:
	CRapidXMLUtil& _xml;	// 값을 기록할 XML 문서 (외부 소유)
	rapidxml::xml_node<>* _root;		// 값을 쓰는 루트 요소
};

#endif // ndef UC_XMLCONFIGFIELDWRITER_H