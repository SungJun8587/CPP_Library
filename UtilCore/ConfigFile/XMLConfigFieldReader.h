
//***************************************************************************
// XMLConfigFieldReader.h : interface for the CXMLConfigFieldReader class.
//
//***************************************************************************

#ifndef UC_XMLCONFIGFIELDREADER_H
#define UC_XMLCONFIGFIELDREADER_H

#include <ConfigFile/ConfigFieldCommon.h>
#include <XML/RapidXMLUtil.h>

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

//***************************************************************************
// @class CXMLConfigFieldReader
// @brief CRapidXMLUtil로 파싱한 설정 XML에서 필드를 타입/범위 검증하며 읽는 도우미. CJSONConfigFieldReader의 짝.
// @detail
// CXMLNode::GetInt32Value 등은 atoi 계열이라 "abc"나 빈 값이 조용히 0이 되고, 범위 검사도 없어
// "값이 0"과 "요소 없음/오타/형식 오류"를 구분할 수 없다. 이 클래스는 문서를 직접 확인해
//     - 요소가 없으면 대상 변수를 건드리지 않고(= 기본값 유지),
//     - 요소는 있는데 형식이 맞지 않거나 범위를 벗어나면 오류로 기록한다.
// 모든 필드를 읽은 뒤 Validate()를 호출해 오류를 로그로 남기고 성공 여부를 받는다.
// 읽히지 않은 요소(오타 가능성)는 경고만 남긴다.
//
// 문서 형식 (CRapidXMLUtil의 Item/Key/Value 관례와 같다):
//     <Config>                                루트 요소의 이름은 무엇이든 된다.
//         <Port>7000</Port>                   스칼라 = 루트의 자식 요소 텍스트
//         <Name>GatewayServer</Name>
//         <Ports><Item>1</Item><Item>2</Item></Ports>               스칼라 목록 = 키 요소 아래의 Item 요소들
//         <ServerNode><Item><ID>1</ID>...</Item></ServerNode>       객체 목록 = Item마다 FromXML 호출
//     </Config>
// 사용법은 JSON 짝과 같다.
//     CRapidXMLUtil xml;
//     CXMLNode root;
//     if( xml.ParseFromFile(path, root) )
//     {
//         CXMLConfigFieldReader reader(xml);
//         reader.Read(_T("Port"), port, 1, 65535);
//         ...
//         bool ok = reader.Validate();
//     }
//
// 값 해석 규칙:
//     - 숫자와 bool은 앞뒤 공백/줄바꿈을 무시한다. 문자열은 있는 그대로 읽는다(공백 포함).
//     - 정수는 10진수 숫자만 허용한다("1.5", "0x10", "1e3", "+5"는 오류). 부호 없는 타입은 음수를 거부한다.
//     - 실수는 로케일과 무관하게 "." 소수점으로 읽고, NaN/Inf/16진 표기는 거부한다.
//     - bool은 true/false/1/0만 허용한다(대소문자 무시).
//     - 같은 이름의 요소가 형제로 두 번 나오면 오류다(어느 쪽이 유효한지 모호하므로).
//     - 스칼라 자리에 하위 요소가 있으면 오류다. CDATA 섹션만 있는 요소는 CDATA 내용을 값으로 본다.
//     - 객체 목록의 필드(ReadObjects의 specs)는 Item의 자식 요소로 찾는다. FromXML이 속성(attribute)을 읽는 타입이면
//       specs 검증과 맞지 않으므로 이 함수를 쓰지 말고 CXMLNode::Get*Attr로 직접 읽는다.
//***************************************************************************
class CXMLConfigFieldReader : public CConfigErrorLog
{
	template <typename T> using Identity = ConfigField::Identity<T>;	// 템플릿 인자 추론에서 제외하기 위한 도구
	template <typename T> static constexpr bool IsNumeric = ConfigField::IsNumeric<T>;	// bool을 제외한 산술 타입인지
	template <typename C> static constexpr bool IsScalarList = ConfigField::IsScalarList<C>;	// 숫자/bool/문자열을 담는 컨테이너인지

public:
	//***************************************************************************
	// @brief 
	// @param xml ParseFromFile 등으로 이미 파싱된 문서. 이 Reader보다 오래 살아 있어야 합니다
	//***************************************************************************
	explicit CXMLConfigFieldReader(CRapidXMLUtil& xml) : _xml(xml), _root(nullptr)
	{
		// 선언(<?xml ...?>)이나 주석이 앞에 있을 수 있으므로 첫 번째 "요소"를 루트로 본다.
		for( rapidxml::xml_node<>* node = xml.GetDocument().first_node(); node != nullptr; node = node->next_sibling() )
		{
			if( node->type() == rapidxml::node_type::node_element )
			{
				_root = node;
				break;
			}
		}

		if( _root == nullptr )
			_errors.push_back(_T("document has no root element"));
	}

	//***************************************************************************
	// @brief 정수/실수 필드를 [minV, maxV] 범위 검증과 함께 읽습니다.
	// @detail 범위를 생략하면 대상 타입이 표현할 수 있는 전체 범위다.
	// @param key 읽을 키 이름
	// @param out [OUT] 읽은 값을 받을 변수
	// @param minV 허용하는 최소값 (생략하면 대상 타입의 최소값)
	// @param maxV 허용하는 최대값 (생략하면 대상 타입의 최대값)
	// @return 키가 없거나 정상적으로 읽었으면 true, 형식/범위 오류면 false(오류를 기록하고 out은 변경하지 않음)
	//***************************************************************************
	template <typename T, typename = std::enable_if_t<IsNumeric<T>>>
	bool Read(const TCHAR* key, T& out,
		typename Identity<T>::type minV = (std::numeric_limits<T>::lowest)(),
		typename Identity<T>::type maxV = (std::numeric_limits<T>::max)())
	{
		std::string text;
		bool present = false;
		if( !ReadScalarText(key, text, present) )
			return false;
		if( !present )
			return true;

		T value{};
		TCHAR why[128];
		if( !Convert(text, value, minV, maxV, why, _countof(why)) )
			return Fail(key, why);

		out = value;
		return true;
	}

	//***************************************************************************
	// @brief bool 필드를 읽습니다. (true/false/1/0)
	// @param key 읽을 키 이름
	// @param out [OUT] 읽은 값을 받을 변수
	// @return 키가 없거나 정상적으로 읽었으면 true, 형식/범위 오류면 false(오류를 기록하고 out은 변경하지 않음)
	//***************************************************************************
	bool Read(const TCHAR* key, bool& out)
	{
		std::string text;
		bool present = false;
		if( !ReadScalarText(key, text, present) )
			return false;
		if( !present )
			return true;

		bool value = false;
		if( !ParseBool(text, value) )
			return Fail(key, _T("must be a boolean (true/false/1/0)"));

		out = value;
		return true;
	}

	//***************************************************************************
	// @brief 문자열 필드를 _tstring으로 읽습니다.
	// @param key 읽을 키 이름
	// @param out [OUT] 읽은 값을 받을 변수
	// @return 키가 없거나 정상적으로 읽었으면 true, 형식/범위 오류면 false(오류를 기록하고 out은 변경하지 않음)
	//***************************************************************************
	bool Read(const TCHAR* key, _tstring& out)
	{
		std::string text;
		bool present = false;
		if( !ReadScalarText(key, text, present) )
			return false;
		if( !present )
			return true;

		out = Utf8ToTString(text.c_str());
		return true;
	}

	//***************************************************************************
	// @brief 문자열 필드를 고정 크기 TCHAR 배열로 읽습니다. 버퍼(널 포함)에 들어가지 않으면 오류입니다.
	// @param key 읽을 키 이름
	// @param dst [OUT] 문자열을 받을 고정 크기 버퍼
	// @return 키가 없거나 정상적으로 읽었으면 true, 버퍼에 들어가지 않으면 false(오류 기록)
	//***************************************************************************
	template <size_t N>
	bool Read(const TCHAR* key, TCHAR(&dst)[N])
	{
		std::string text;
		bool present = false;
		if( !ReadScalarText(key, text, present) )
			return false;
		if( !present )
			return true;

		const _tstring value = Utf8ToTString(text.c_str());
		if( value.length() >= N )
			return Fail(key, _T("string is too long"));

		_tcsncpy_s(dst, N, value.c_str(), _TRUNCATE);
		return true;
	}

	//***************************************************************************
	// @brief 숫자/bool/문자열 컨테이너(std::vector, CVector 등)를 <키><Item>..</Item>..</키> 형태에서 읽습니다.
	// @detail 한 원소라도 형식/범위가 맞지 않거나 Item이 아닌 요소가 섞여 있으면 오류로 기록하고 out은 건드리지 않는다.
	// @param key 읽을 키 이름
	// @param out [OUT] 읽은 값을 받을 컨테이너. 성공하면 내용을 통째로 교체
	// @param minV 각 원소가 가질 수 있는 최소값 (숫자 원소에만 적용)
	// @param maxV 각 원소가 가질 수 있는 최대값 (숫자 원소에만 적용)
	// @return 키가 없거나 정상적으로 읽었으면 true, Item이 아닌 요소가 섞였거나 원소가 하나라도 맞지 않으면 false(오류 기록, out은 변경하지 않음)
	//***************************************************************************
	template <typename C, typename = std::enable_if_t<IsScalarList<C>>>
	bool Read(const TCHAR* key, C& out,
		typename Identity<typename C::value_type>::type minV = ConfigField::DefaultMin<typename C::value_type>(),
		typename Identity<typename C::value_type>::type maxV = ConfigField::DefaultMax<typename C::value_type>())
	{
		using E = typename C::value_type;

		rapidxml::xml_node<>* list = nullptr;
		if( !FindOne(key, list) )
			return false;
		if( list == nullptr )
			return true;

		const std::string itemTag = TStringToUtf8(ItemName);

		C result;
		uint32 index = 0;
		for( rapidxml::xml_node<>* item = list->first_node(); item != nullptr; item = item->next_sibling() )
		{
			if( item->type() != rapidxml::node_type::node_element )
				continue;

			if( !IsNamed(item, itemTag) )
				return Fail(key, _T("contains an element other than <Item>"));

			TCHAR name[128];
			_sntprintf_s(name, _countof(name), _TRUNCATE, _T("%s[%u]"), key, index);

			if( HasChildElement(item) )
				return Fail(name, _T("must be a value, not a nested element"));

			E element{};
			TCHAR why[128];
			if( !ConvertElement(TextOf(item), element, minV, maxV, why, _countof(why)) )
				return Fail(name, why);

			result.push_back(std::move(element));
			++index;
		}

		out = std::move(result);
		return true;
	}

	//***************************************************************************
	// @brief FromXML을 가진 사용자 타입의 객체 목록(<키><Item>..</Item>..</키>)을 읽습니다.
	// @detail 사용자 타입의 FromXML은 요소 존재/형식을 확인하지 않는 경우가 많아, 채우기 전에 모든 Item을 specs로 검증한다.
	//          통과하면 CRapidXMLUtil::GetObjectVector로 채운다(원소마다 item.FromXML(Item요소)).
	// @param key 읽을 키 이름
	// @param out [OUT] 읽은 객체를 받을 컨테이너
	// @param specs 각 객체가 가져야 할 필드와 형식/범위
	// @return 키가 없거나 정상적으로 읽었으면 true, 원소가 specs를 만족하지 않으면 false(오류 기록)
	//***************************************************************************
	template <typename C>
	bool ReadObjects(const TCHAR* key, C& out, std::initializer_list<SFieldSpec> specs)
	{
		rapidxml::xml_node<>* list = nullptr;
		if( !FindOne(key, list) )
			return false;
		if( list == nullptr )
			return true;

		const std::string itemTag = TStringToUtf8(ItemName);

		uint32 index = 0;
		for( rapidxml::xml_node<>* item = list->first_node(); item != nullptr; item = item->next_sibling() )
		{
			if( item->type() != rapidxml::node_type::node_element )
				continue;

			if( !IsNamed(item, itemTag) )
				return Fail(key, _T("contains an element other than <Item>"));

			TCHAR why[160];
			if( !CheckObject(item, specs, why, _countof(why)) )
			{
				TCHAR name[128];
				_sntprintf_s(name, _countof(name), _TRUNCATE, _T("%s[%u]"), key, index);
				return Fail(name, why);
			}
			++index;
		}

		C result;
		_xml.GetObjectVector(result, _root, key);
		out = std::move(result);
		return true;
	}

	//***************************************************************************
	// @brief FromXML을 가진 사용자 타입 객체 하나(<키>..</키>)를 읽습니다. (ReadObjects의 단일 객체 버전)
	// @param key 읽을 키 이름
	// @param out [OUT] 읽은 객체를 받을 변수
	// @param specs 각 객체가 가져야 할 필드와 형식/범위
	// @return 키가 없거나 정상적으로 읽었으면 true, 객체가 specs를 만족하지 않으면 false(오류 기록)
	//***************************************************************************
	template <typename T>
	bool ReadObject(const TCHAR* key, T& out, std::initializer_list<SFieldSpec> specs)
	{
		rapidxml::xml_node<>* node = nullptr;
		if( !FindOne(key, node) )
			return false;
		if( node == nullptr )
			return true;

		TCHAR why[160];
		if( !CheckObject(node, specs, why, _countof(why)) )
			return Fail(key, why);

		T value = out;
		_xml.GetObject(value, node);
		out = std::move(value);
		return true;
	}

	//***************************************************************************
	// @brief 이 클래스로 읽지 않지만 파일에 있어도 되는 요소를 등록합니다. (미사용 요소 경고 방지)
	// @param key 경고에서 제외할 요소 이름
	//***************************************************************************
	void Ignore(const TCHAR* key) { _known.insert(key); }

	//***************************************************************************
	// @brief 읽기를 마치고 결과를 보고합니다.
	// @return 오류가 하나도 없으면 true
	//***************************************************************************
	bool Validate() const
	{
		if( _root != nullptr )
		{
			for( rapidxml::xml_node<>* child = _root->first_node(); child != nullptr; child = child->next_sibling() )
			{
				if( child->type() != rapidxml::node_type::node_element )
					continue;

				const _tstring name = Utf8ToTString(std::string(child->name(), child->name_size()).c_str());
				if( _known.find(name) == _known.end() )
					LOG_WARNING(_T("[Config] unknown element ignored. name=%s"), name.c_str());
			}
		}

		LogErrors(_T("invalid"));
		return !HasErrors();
	}

private:
	//***************************************************************************
	// @brief 루트 아래에서 key 요소를 찾습니다. 없으면 node=nullptr(true), 같은 이름이 둘 이상이면 오류(false).
	// @param key 읽을 키 이름
	// @param node [OUT] 찾은 요소. 없으면 nullptr
	// @return 요소가 없거나 하나만 있으면 true, 같은 이름이 둘 이상이면 false(오류 기록)
	//***************************************************************************
	bool FindOne(const TCHAR* key, rapidxml::xml_node<>*& node)
	{
		_known.insert(key);
		node = nullptr;
		if( _root == nullptr )
			return true;

		const std::string name = TStringToUtf8(key);
		node = _root->first_node(name.c_str());
		if( node != nullptr && node->next_sibling(name.c_str()) != nullptr )
		{
			node = nullptr;
			return Fail(key, _T("is specified more than once"));
		}
		return true;
	}

	//***************************************************************************
	// @brief 스칼라 요소의 텍스트를 가져옵니다. 요소가 없으면 present=false, 하위 요소가 있으면 오류.
	// @param key 읽을 키 이름
	// @param text [OUT] 요소의 텍스트(UTF-8)
	// @param present [OUT] 요소가 있었는지
	// @return 요소가 없거나 값으로 읽었으면 true, 중복되었거나 하위 요소가 있으면 false(오류 기록)
	//***************************************************************************
	bool ReadScalarText(const TCHAR* key, std::string& text, bool& present)
	{
		present = false;

		rapidxml::xml_node<>* node = nullptr;
		if( !FindOne(key, node) )
			return false;
		if( node == nullptr )
			return true;

		if( HasChildElement(node) )
			return Fail(key, _T("must be a value, not a nested element"));

		text = TextOf(node);
		present = true;
		return true;
	}

	//***************************************************************************
	// @brief 요소가 자식 "요소"를 가지고 있는지 확인합니다. 텍스트와 CDATA는 세지 않습니다.
	// @param node 검사할 요소
	// @return 자식 요소가 하나라도 있으면 true
	//***************************************************************************
	static bool HasChildElement(const rapidxml::xml_node<>* node)
	{
		for( const rapidxml::xml_node<>* child = node->first_node(); child != nullptr; child = child->next_sibling() )
		{
			if( child->type() == rapidxml::node_type::node_element )
				return true;
		}
		return false;
	}

	//***************************************************************************
	// @brief 요소의 이름이 주어진 이름과 같은지 비교합니다.
	// @param node 비교할 요소
	// @param name 기대하는 이름(UTF-8)
	// @return 이름이 같으면 true
	//***************************************************************************
	static bool IsNamed(const rapidxml::xml_node<>* node, const std::string& name)
	{
		return node->name_size() == name.size() && std::char_traits<char>::compare(node->name(), name.c_str(), name.size()) == 0;
	}

	//***************************************************************************
	// @brief 요소의 텍스트(UTF-8)를 가져옵니다. 텍스트가 없고 CDATA만 있으면 CDATA 내용을 사용합니다.
	// @param node 텍스트를 읽을 요소
	// @return 요소의 텍스트(UTF-8)
	//***************************************************************************
	static std::string TextOf(const rapidxml::xml_node<>* node)
	{
		return std::string(XmlNodeText(node));
	}

	//***************************************************************************
	// @brief 문자열 앞뒤의 공백, 탭, 개행을 제거합니다.
	// @param s 대상 문자열
	// @return 공백을 제거한 문자열. 공백뿐이면 빈 문자열
	//***************************************************************************
	static std::string Trim(const std::string& s)
	{
		const size_t begin = s.find_first_not_of(" \t\r\n");
		if( begin == std::string::npos )
			return std::string();
		const size_t end = s.find_last_not_of(" \t\r\n");
		return s.substr(begin, end - begin + 1);
	}

	//***************************************************************************
	// @brief 문자열이 10진수 숫자로만 이루어졌는지 확인합니다. (선택적으로 앞의 '-' 허용)
	// 10진수 숫자만(선택적으로 앞의 '-') 허용
	// @param s 검사할 문자열
	// @param allowMinus 앞의 '-'를 허용할지
	// @return 숫자로만 이루어졌으면 true
	//***************************************************************************
	static bool IsDecimal(const std::string& s, bool allowMinus)
	{
		size_t i = 0;
		if( allowMinus && !s.empty() && s[0] == '-' )
			i = 1;
		if( i >= s.size() )
			return false;
		for( ; i < s.size(); ++i )
		{
			if( s[i] < '0' || s[i] > '9' )
				return false;
		}
		return true;
	}

	//***************************************************************************
	// @brief true/false/1/0(대소문자 무시, 앞뒤 공백 무시)을 bool로 읽습니다.
	// @param raw 대상 문자열
	// @param out [OUT] 읽은 값
	// @return 해석할 수 있으면 true
	//***************************************************************************
	static bool ParseBool(const std::string& raw, bool& out)
	{
		std::string s = Trim(raw);
		for( char& c : s )
		{
			if( c >= 'A' && c <= 'Z' )
				c = static_cast<char>(c - 'A' + 'a');
		}

		if( s == "true" || s == "1" )
			out = true;
		else if( s == "false" || s == "0" )
			out = false;
		else
			return false;
		return true;
	}

	//***************************************************************************
	// @brief 텍스트를 T로 변환하며 형식/범위를 검사합니다. 실패 시 사유를 why에 적습니다.
	// @detail 비교는 64비트로 하므로 uint32(4294967296)나 int8(300) 같은 넘침도 범위 오류로 잡힌다.
	// @param raw 변환할 텍스트
	// @param out [OUT] 변환 결과
	// @param minV 허용하는 최소값
	// @param maxV 허용하는 최대값
	// @param why [OUT] 실패 사유를 받을 버퍼
	// @param whyCount why 버퍼의 크기(문자 수)
	// @return 변환에 성공하면 true, 형식/범위가 맞지 않으면 false(사유는 why에 기록)
	//***************************************************************************
	template <typename T>
	static bool Convert(const std::string& raw, T& out, T minV, T maxV, TCHAR* why, size_t whyCount)
	{
		const std::string s = Trim(raw);

		if constexpr( std::is_floating_point_v<T> )
		{
			if( s.empty() )
				return ConfigField::Describe(why, whyCount, _T("must be a number (value is empty)"));
			if( s.find_first_not_of("0123456789+-.eE") != std::string::npos )
				return ConfigField::Describe(why, whyCount, _T("must be a number"));

			// float은 double을 거치지 않고 직접 파싱한다(이중 반올림 방지)
			char* end = nullptr;
			double d;
			if constexpr( std::is_same_v<T, float> )
				d = _strtof_l(s.c_str(), &end, g_xmlNumericLocale);
			else
				d = _strtod_l(s.c_str(), &end, g_xmlNumericLocale);
			if( end == s.c_str() || *end != '\0' || !std::isfinite(d) )
				return ConfigField::Describe(why, whyCount, _T("must be a finite number"));

			if( d < static_cast<double>(minV) || d > static_cast<double>(maxV) )
				return ConfigField::Describe(why, whyCount, _T("out of range"), d);
			out = static_cast<T>(d);
			return true;
		}
		else if constexpr( std::is_signed_v<T> )
		{
			if( !IsDecimal(s, true) )
				return ConfigField::Describe(why, whyCount, _T("must be an integer"));

			errno = 0;
			const long long n = std::strtoll(s.c_str(), nullptr, 10);
			if( errno == ERANGE )
				return ConfigField::Describe(why, whyCount, _T("integer is too large"));

			if( n < static_cast<long long>(minV) || n > static_cast<long long>(maxV) )
				return ConfigField::DescribeRange(why, whyCount, static_cast<long long>(minV), static_cast<long long>(maxV), n);
			out = static_cast<T>(n);
			return true;
		}
		else
		{
			if( !IsDecimal(s, false) )
				return ConfigField::Describe(why, whyCount, _T("must be a non-negative integer"));

			errno = 0;
			const unsigned long long n = std::strtoull(s.c_str(), nullptr, 10);
			if( errno == ERANGE )
				return ConfigField::Describe(why, whyCount, _T("integer is too large"));

			if( n < static_cast<unsigned long long>(minV) || n > static_cast<unsigned long long>(maxV) )
				return ConfigField::DescribeRangeU(why, whyCount, static_cast<unsigned long long>(minV), static_cast<unsigned long long>(maxV), n);
			out = static_cast<T>(n);
			return true;
		}
	}

	//***************************************************************************
	// @brief 목록 원소 하나를 E로 변환합니다. (숫자/bool/문자열)
	// @param raw 변환할 Item 텍스트
	// @param out [OUT] 변환 결과
	// @param minV 허용하는 최소값 (숫자 원소에만 적용)
	// @param maxV 허용하는 최대값 (숫자 원소에만 적용)
	// @param why [OUT] 실패 사유를 받을 버퍼
	// @param whyCount why 버퍼의 크기(문자 수)
	// @return 변환에 성공하면 true, 형식/범위가 맞지 않으면 false(사유는 why에 기록)
	//***************************************************************************
	template <typename E>
	static bool ConvertElement(const std::string& raw, E& out, E minV, E maxV, TCHAR* why, size_t whyCount)
	{
		if constexpr( std::is_same_v<E, bool> )
		{
			bool value = false;
			if( !ParseBool(raw, value) )
				return ConfigField::Describe(why, whyCount, _T("must be a boolean (true/false/1/0)"));
			out = value;
			return true;
		}
		else if constexpr( std::is_same_v<E, _tstring> )
		{
			out = Utf8ToTString(raw.c_str());
			return true;
		}
		else
		{
			return Convert(raw, out, minV, maxV, why, whyCount);
		}
	}

	//***************************************************************************
	// @brief 객체(Item 또는 단일 객체 요소)의 자식 요소가 specs를 만족하는지 검사합니다.
	// @param obj 검사할 객체 요소(Item 또는 단일 객체 요소)
	// @param specs 각 객체가 가져야 할 필드와 형식/범위
	// @param why [OUT] 실패 사유를 받을 버퍼
	// @param whyCount why 버퍼의 크기(문자 수)
	// @return 모든 필드가 specs를 만족하면 true, 아니면 false(사유는 why에 기록)
	//***************************************************************************
	static bool CheckObject(rapidxml::xml_node<>* obj, std::initializer_list<SFieldSpec> specs, TCHAR* why, size_t whyCount)
	{
		for( const SFieldSpec& spec : specs )
		{
			const std::string name = TStringToUtf8(spec.name);
			rapidxml::xml_node<>* field = obj->first_node(name.c_str());
			if( field == nullptr )
			{
				if( !spec.required )
					continue;
				_sntprintf_s(why, whyCount, _TRUNCATE, _T("field '%s' is required"), spec.name);
				return false;
			}
			if( field->next_sibling(name.c_str()) != nullptr )
			{
				_sntprintf_s(why, whyCount, _TRUNCATE, _T("field '%s' is specified more than once"), spec.name);
				return false;
			}
			if( HasChildElement(field) )
			{
				_sntprintf_s(why, whyCount, _TRUNCATE, _T("field '%s' must be a value, not a nested element"), spec.name);
				return false;
			}

			const std::string text = TextOf(field);
			TCHAR ignored[64];
			bool ok = false;
			switch( spec.type )
			{
			case EFieldType::Int:
			{
				int64 v = 0;
				ok = Convert(text, v, spec.minV, spec.maxV, ignored, _countof(ignored));
				break;
			}
			case EFieldType::UInt:
			{
				uint64 v = 0;
				ok = Convert(text, v, uint64(0), (std::numeric_limits<uint64>::max)(), ignored, _countof(ignored))
					&& ConfigField::InSpecRangeU(spec, v);
				break;
			}
			case EFieldType::Number:
			{
				double v = 0;
				ok = Convert(text, v, (std::numeric_limits<double>::lowest)(), (std::numeric_limits<double>::max)(), ignored, _countof(ignored));
				break;
			}
			case EFieldType::String:
				ok = true;
				break;
			case EFieldType::Bool:
			{
				bool v = false;
				ok = ParseBool(text, v);
				break;
			}
			}

			if( !ok )
			{
				_sntprintf_s(why, whyCount, _TRUNCATE, _T("field '%s' has wrong type or is out of range"), spec.name);
				return false;
			}
		}
		return true;
	}

private:
	CRapidXMLUtil& _xml;	// 읽을 XML 문서 (외부 소유)
	rapidxml::xml_node<>* _root;		// 루트 요소 (없으면 nullptr)
	std::set<_tstring>			_known;		// 읽기를 시도한 요소 이름
};

#endif // ndef UC_XMLCONFIGFIELDREADER_H