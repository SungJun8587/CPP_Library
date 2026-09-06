
//***************************************************************************
// RapidJSONUtil.h : interface and implementation for the CRapidJSONUtil class.
//
//***************************************************************************

#ifndef UC_RAPIDJSONUTIL_H
#define UC_RAPIDJSONUTIL_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <type_traits>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"				
#include "rapidjson/filewritestream.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/encodings.h"
#include "rapidjson/pointer.h"

// Windows API GetObject와 rapidjson GetObject 간의 충돌 방지
#ifdef GetObject
#undef GetObject  // 기존 GetObject 매크로 해제
#endif

using namespace std;
using namespace rapidjson;

typedef rapidjson::GenericDocument<rapidjson::UTF16<>> WDocument;
typedef rapidjson::GenericValue<rapidjson::UTF16<>> WValue;
typedef rapidjson::GenericStringStream<rapidjson::UTF16<>> WStringStream;
typedef rapidjson::GenericStringBuffer<rapidjson::UTF16<>> WStringBuffer;
typedef rapidjson::GenericPointer<WValue> WPointer;

#ifdef _UNICODE
typedef WDocument        _tDocument;
typedef WValue           _tValue;
typedef WStringStream    _tStringStream;
typedef WStringBuffer    _tStringBuffer;
typedef WPointer         _tPointer;
typedef Writer<WStringBuffer, UTF16<>, UTF16<>> _tWriter;
typedef PrettyWriter<WStringBuffer, UTF16<>, UTF16<>> _tPrettyWriter;
typedef GenericArray<true, WValue> _tArray;
#else
typedef Document         _tDocument;
typedef Value            _tValue;
typedef StringBuffer     _tStringBuffer;
typedef StringStream     _tStringStream;
typedef Pointer          _tPointer;
typedef Writer<StringBuffer, UTF8<>, UTF8<>> _tWriter;
typedef PrettyWriter<StringBuffer, UTF8<>, UTF8<>> _tPrettyWriter;
typedef GenericArray<true, Value> _tArray;
#endif

// 신뢰할 수 없는 JSON을 재귀적으로 정리(RecursiveRemove)할 때 스택 오버플로우를
// 방지하기 위한 최대 재귀 깊이. 필요 시 조정 가능.
static constexpr int RAPIDJSONUTIL_MAX_RECURSION_DEPTH = 256;

//***************************************************************************
// @brief RapidJSON 라이브러리를 래핑하여 JSON 데이터 조작, 파싱, 직렬화를 지원하는 유틸리티 클래스입니다.
// @detail 유니코드/멀티바이트 환경에 대응하며 연산자 오버로딩 및 프록시 패턴을 통한 직관적인 데이터 접근을 제공합니다.
//***************************************************************************
class CRapidJSONUtil
{
public:
    //***************************************************************************
    // @brief CRapidJSONUtil 객체를 생성합니다.
    // @detail 내부 문서 객체를 빈 객체로 초기화합니다.
    //***************************************************************************
    CRapidJSONUtil();

    //***************************************************************************
    // @brief CRapidJSONUtil 객체의 복사 생성자입니다.
    // @param other 복사할 대상 CRapidJSONUtil 객체 참조
    //***************************************************************************
    CRapidJSONUtil(const CRapidJSONUtil& other);

    //***************************************************************************
    // @brief 내부 래핑된 JSON 문서 객체 참조를 반환합니다.
    // @return 내부 _tDocument 문서 객체 참조
    //***************************************************************************
    _tDocument& GetDocument() { return _document; }

    //***************************************************************************
    // @brief 내부 JSON 문서를 초기화합니다.
    // @detail 내부 문서를 빈 JSON 객체 상태로 설정합니다.
    //         단순히 SetObject()만 호출하면 타입만 바뀔 뿐, 기본 MemoryPoolAllocator가
    //         이전에 할당한 메모리 청크는 반환되지 않고 계속 누적됩니다. 이 객체를
    //         (예: 커넥션 풀이나 세션 객체에서) 요청마다 Clear() 후 재사용하는 패턴에서는
    //         메모리 사용량이 무한정 증가할 수 있습니다. 기본 생성자로 새 Document를 만들어
    //         이동 대입하면 이전 allocator 체인이 소멸자를 통해 정상적으로 해제되어
    //         진짜 빈 상태로 리셋됩니다.
    //***************************************************************************
    void Clear() {
        _document = _tDocument();
        _document.SetObject();
    }

    //***************************************************************************
    // @brief 디버그 출력 활성화 여부를 설정합니다.
    // @param bIsDebugPrint 디버그 출력 여부 (true: 켜기, false: 끄기)
    //***************************************************************************
    void SetDebugPrint(bool bIsDebugPrint) {
        _bIsDebugPrint = bIsDebugPrint;
        Print_DebugInfo(_T("%s is set %s\n"), __TFUNCTION__, bIsDebugPrint ? _T("on") : _T("off"));
    }

    //***************************************************************************
    // @brief 디버그 출력 활성화 상태를 반환합니다.
    // @return 디버그 출력 활성화 여부
    //***************************************************************************
    bool GetDebugPrint() {
        return _bIsDebugPrint;
    }

    CRapidJSONUtil& operator=(const CRapidJSONUtil& other);
    CRapidJSONUtil& operator=(const TCHAR* ptszValue);
    CRapidJSONUtil& operator=(const _tstring& strValue);
    CRapidJSONUtil& operator=(int32 iValue);
    CRapidJSONUtil& operator=(int64 i64Value);
    CRapidJSONUtil& operator=(uint32 uValue);
    CRapidJSONUtil& operator=(uint64 u64Value);
    CRapidJSONUtil& operator=(double dValue);
    CRapidJSONUtil& operator=(bool bValue);

    CRapidJSONUtil& operator+(const std::pair<const TCHAR*, CRapidJSONUtil>& keyValue);
    CRapidJSONUtil& operator+(const std::pair<_tstring, CRapidJSONUtil>& keyValue);
    CRapidJSONUtil& operator+(const CRapidJSONUtil& other);

    CRapidJSONUtil& operator-(const TCHAR* ptszKey);
    CRapidJSONUtil& operator-(const _tstring& key);
    CRapidJSONUtil& operator-(const uint32 index);

    bool IsExists(const TCHAR* ptszKey) const;
    bool IsExists(const _tstring& key) const;
    bool IsObject() const;
    bool IsArray() const;
    bool IsString() const;
    bool IsNumber() const;
    bool IsStringNumber() const;
    bool IsInt32() const;
    bool IsInt64() const;
    bool IsUint32() const;
    bool IsUint64() const;
    bool IsDouble() const;
    bool IsBool() const;

    bool Parse(const _tstring& jsonString);

    _tstring ToString(const bool pretty = false) const;

    void PrintJSON(const bool pretty) const;
    bool SaveToFile(const _tstring& filename, _tstring& jsonString, const bool pretty = false);
    bool SaveToFile(const _tstring& filename, const bool pretty = false);
    bool LoadFromFile(const _tstring& filename);

    std::vector<_tstring> GetKeys() const;

    void Remove(const TCHAR* ptszKey);
    void Remove(const _tstring& key);
    void Remove(const _tstring& key, uint32 index);

    //***************************************************************************
    // CRapidJSONUtil 클래스 operator[] Setter, Getter Operator Overloading을 위한 프록시 클래스
    //***************************************************************************
    class Proxy
    {
    private:
        CRapidJSONUtil& _jsonUtil; // 대상 CRapidJSONUtil 객체 참조
        _tstring        _key;      // 접근할 키 문자열

    public:
        //***************************************************************************
        // @brief Proxy 객체를 생성합니다.
        // @param jsonUtil 관리할 CRapidJSONUtil 객체 참조
        // @param key 접근할 키 문자열
        //***************************************************************************
        Proxy(CRapidJSONUtil& jsonUtil, const _tstring& key) : _jsonUtil(jsonUtil), _key(key) {}

        // = 연산자 오버로딩(값 설정)
        // NOTE: AddValue()는 내부적으로 기존 동일 키를 제거한 뒤 추가하므로
        //       같은 키에 반복 대입해도 중복 멤버가 생기지 않습니다.
        template <typename T>
        Proxy& operator=(const T& value) {
            _jsonUtil.AddValue(_key, value);
            return *this;
        }

        // T() 연산자 오버로딩(값 읽기)
        template <typename T>
        operator T() const { return _jsonUtil.Deserialize<T>(_key); }
    };

    //***************************************************************************
    // @brief 키 문자열을 인자로 받아 프록시 객체를 반환합니다.
    // @param key 접근할 키 문자열
    // @return 생성된 Proxy 객체
    // @detail key가 nullptr이면 Proxy 생성 시 _tstring(nullptr) 생성자가 호출되어
    //         크래시하므로, 빈 문자열 키로 안전하게 대체합니다.
    //***************************************************************************
    Proxy operator[](const TCHAR* key) {
        return Proxy(*this, key ? key : _T(""));
    }

    //***************************************************************************
    // @brief 키 문자열(_tstring)을 인자로 받아 프록시 객체를 반환합니다.
    // @param key 접근할 키 문자열
    // @return 생성된 Proxy 객체
    //***************************************************************************
    Proxy operator[](const _tstring& key) {
        return operator[](key.c_str());
    }

    template <typename T>
    inline _tstring Serialize(const _tstring& key, const T& obj, const bool pretty = false);

    template <typename T>
    inline T Deserialize(const _tstring& key) const;

    template <typename T>
    inline void AddValue(const _tstring& key, const T& value);

    template <typename T>
    inline void UpdateValue(const _tstring& key, const T& value);

    template <typename T>
    inline void AddArray(const _tstring& key, const T& value);

    template <typename T>
    inline void UpdateArrayAt(const _tstring& key, uint32 index, const T& value);

    template <typename T>
    inline T GetValue(const _tstring& key, const T& defaultValue = T()) const;

    template <typename T>
    inline void AddObject(const _tstring& key, const T& object);

    template <typename T>
    inline T GetObject(const _tstring& key) const;

    template <typename Container, typename ValueType = typename Container::value_type>
    inline void AddVector(const _tstring& key, const Container& vec);

    template <typename Container, typename ValueType = typename Container::value_type>
    inline Container GetVector(const _tstring& key) const;

    template <typename Container, typename ValueType = typename Container::value_type>
    inline void AddObjectVector(const _tstring& key, const Container& vec);

    template <typename Container, typename ValueType = typename Container::value_type>
    inline Container GetObjectVector(const _tstring& key) const;

    template <typename MapContainer>
    inline void AddMap(const _tstring& key, const MapContainer& map);

    template <typename MapContainer>
    inline MapContainer GetMap(const _tstring& key) const;

    template <typename MapContainer>
    inline void AddObjectMap(const _tstring& key, const MapContainer& map);

    template <typename MapContainer>
    inline MapContainer GetObjectMap(const _tstring& key) const;

private:
    void Print_DebugInfo(const TCHAR* ptszFormat, ...);

    //***************************************************************************
    // @brief 지정한 키의 멤버가 이미 존재하면 재귀적으로 정리한 뒤 제거합니다.
    // @detail Add* 계열 함수들이 "추가"가 아니라 "설정(덮어쓰기)" 의미로 동작하도록
    //         호출 전에 기존 동일 키 멤버를 제거해 rapidjson::AddMember가 중복 키를
    //         만들지 않게 합니다. HasMember/FindMember를 한 번만 사용하여
    //         불필요한 이중 탐색을 피합니다.
    //***************************************************************************
    void RemoveMemberIfExists(const _tstring& key);

    //***************************************************************************
    // @brief C++ 구조체 → JSON 변환(템플릿(T) 변수값을 _tValue 변수에 할당)
    // @detail 값을 새로 할당(rapidjson allocator 사용)해야 하므로 non-const로 선언합니다.
    //         (allocator를 참조 멤버로 들고 있지 않으므로 항상 _document.GetAllocator()를
    //         통해 얻어야 하며, 이는 비-const 컨텍스트에서만 가능합니다.)
    //***************************************************************************
    template <typename T>
    _tValue ConvertToJSONValue(const T& value);

    //***************************************************************************
    // @brief JSON → C++ 구조체 변환(_tValue 변수값을 템플릿(T) 변수에 할당)
    //***************************************************************************
    template <typename T>
    T ConvertFromJSONValue(const _tValue& value) const;

    //***************************************************************************
    // @brief 지정한 _tValue가 T로 안전하게 변환 가능한 JSON 타입인지 검사합니다.
    // @detail GetValue/GetVector/GetMap 등에서 저장된 JSON 값의 실제 타입이 요청한 C++
    //         타입과 다를 때(예: 문자열 필드를 int로 읽으려는 경우) rapidjson의 GetInt() 등이
    //         릴리스 빌드에서 어서션 없이 정의되지 않은 동작을 일으키는 것을 막기 위한
    //         사전 검증입니다. 불일치 시 호출자는 defaultValue/빈 값으로 폴백해야 합니다.
    //         ToJSON/FromJSON을 쓰는 사용자 정의 타입은 일반적으로 판별할 수 없어 true를 반환합니다.
    //***************************************************************************
    template <typename T>
    bool IsConvertible(const _tValue& value) const;

    void RecursiveRemove(_tValue& value, int depth = 0);

    //***************************************************************************
    // @brief 벡터 타입 확인
    //***************************************************************************
    // 1. 기본 템플릿 (기본값: false)
    template <typename T>
    struct is_vector : std::false_type {};

    // 2. 커스텀 벡터 CVector 특수화 (true)
    template <typename T, typename Alloc>
    struct is_vector<CVector<T, Alloc>> : std::true_type {};

    // 3. 표준 벡터 std::vector 특수화 (true)
    template <typename T, typename Alloc>
    struct is_vector<std::vector<T, Alloc>> : std::true_type {};

    // 4. std::vector<bool> 특수화 제외 (비트 압축 객체로 인한 컴파일 에러 방지)
    template <typename Alloc>
    struct is_vector<std::vector<bool, Alloc>> : std::false_type {};

    //***************************************************************************
    // @brief 맵 타입 확인
    //***************************************************************************
    template <typename T>
    struct is_map : std::false_type {};

    template <typename K, typename V, typename Comp, typename Alloc>
    struct is_map<CMap<K, V, Comp, Alloc>> : std::true_type {};

    template <typename K, typename V, typename Comp, typename Alloc>
    struct is_map<std::map<K, V, Comp, Alloc>> : std::true_type {};

    //***************************************************************************
    // @brief T에 ToXML 멤버 함수가 있는지 확인하는 타입 트레이트
    // @detail AddObject/AddObjectVector/AddObjectMap은 모두 대상 객체를 `const T&`로 받아
    //         ToJSON을 호출합니다(예: `const auto& item : vec` 순회 후 `item.ToJSON(...)`).
    //         트레이트도 동일하게 `const T&` 기준으로 호출 가능 여부를 검사해야, ToJSON이
    //         const로 선언되지 않은 타입에 대해 여기서 즉시 false를 반환하고 다른 분기로
    //         빠지거나 명확한 static_assert 메시지를 내도록 유도할 수 있습니다. `declval<T>()`
    //         (비-const)로 검사하면 그런 타입도 true로 잘못 판정되어, 실제로는 AddObject 등의
    //         본문 깊숙한 곳에서 "const 객체에서 비-const 멤버 함수 호출 불가"라는 알아보기
    //         힘든 컴파일 에러로 이어집니다.
    //***************************************************************************
    template <typename T, typename = void>
    struct has_tojson_method : std::false_type {};

    template <typename T>
    struct has_tojson_method<T, std::void_t<decltype(std::declval<const T&>().ToJSON(std::declval<_tValue&>(), std::declval<_tDocument::AllocatorType&>()))>> : std::true_type {};

    //***************************************************************************
    // @brief 컴파일 타임 에러 유도용 유틸리티
    //***************************************************************************
    template <typename T>
    struct dependent_false : std::false_type {};

private:
    // NOTE: allocator는 더 이상 참조 멤버로 보관하지 않습니다. 참조 멤버가 있으면
    // 컴파일러가 이동 생성자/이동 대입 연산자를 암묵적으로 생성하지 않아, 값 전달/반환/
    // 컨테이너 저장 시마다 CopyFrom()에 의한 JSON 트리 전체 깊은 복사가 강제로 일어납니다.
    // _document만 멤버로 남기면(rapidjson::GenericDocument는 이동 가능) 컴파일러가
    // 이동 연산을 자동 생성해 불필요한 복사를 피할 수 있습니다. allocator가 필요하면
    // 그때그때 _document.GetAllocator()로 얻습니다(비-const 컨텍스트에서만 가능).
    _tDocument                  _document;  // JSON Document

    bool                        _bIsDebugPrint; // 디버그 출력 활성화 여부 플래그
};

#include <JSON/RapidJSONUtil.inl>

#endif // ndef UC_RAPIDJSONUTIL_H