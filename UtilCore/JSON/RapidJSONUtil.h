
//***************************************************************************
// RapidJSONUtil.h : interface and implementation for the CRapidJSONUtil class.
//
//***************************************************************************

#ifndef __RAPIDJSONUTIL_H__
#define __RAPIDJSONUTIL_H__

#pragma once

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
typedef WDocument		_tDocument;
typedef WValue			_tValue;
typedef WStringStream   _tStringStream;
typedef WStringBuffer   _tStringBuffer;
typedef WPointer		_tPointer;
typedef Writer<WStringBuffer, UTF16<>, UTF16<>> _tWriter;
typedef PrettyWriter<WStringBuffer, UTF16<>, UTF16<>> _tPrettyWriter;
typedef GenericArray<true, WValue> _tArray;
#else
typedef Document		_tDocument;
typedef Value			_tValue;
typedef StringBuffer	_tStringBuffer;
typedef StringStream	_tStringStream;
typedef Pointer			_tPointer;
typedef Writer<StringBuffer, UTF8<>, UTF8<>> _tWriter;
typedef PrettyWriter<StringBuffer, UTF8<>, UTF8<>> _tPrettyWriter;
typedef GenericArray<true, Value> _tArray;
#endif

class CRapidJSONUtil
{
public:
    CRapidJSONUtil();
    CRapidJSONUtil(const CRapidJSONUtil& other);

    _tDocument& GetDocument() { return _document; }

    void Clear() {
        _document.SetObject();		// 객체 초기화
    }

    void SetDebugPrint(bool bIsDebugPrint) {
        _bIsDebugPrint = bIsDebugPrint;
        Print_DebugInfo(_T("%s is set %s\n"), __TFUNCTION__, bIsDebugPrint ? _T("on") : _T("off"));
    }

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

    CRapidJSONUtil& operator+(const std::pair<_tstring, CRapidJSONUtil>& keyValue);
    CRapidJSONUtil& operator+(const CRapidJSONUtil& other);

    CRapidJSONUtil& operator-(const _tstring& key);
    CRapidJSONUtil& operator-(const uint32 index);

    bool IsExists(const TCHAR* ptszKey) const;
    bool IsExists(const _tstring& strKey) const;
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

    std::vector<_tstring> GetKeys();

    void Remove(const TCHAR* ptszKey);
    void Remove(const _tstring& key);
    void Remove(const _tstring& key, uint32 index);

    //***************************************************************************
    // CRapidJSONUtil 클래스 operator[] Setter, Getter Operator Overloading을 위한 프록시 클래스
    class Proxy 
    {
        private:
            CRapidJSONUtil& _jsonUtil;
            _tstring        _key;

        public:
            Proxy(CRapidJSONUtil& jsonUtil, const _tstring& key) : _jsonUtil(jsonUtil), _key(key) {}

            // = 연산자 오버로딩(값 설정)
            template <typename T>
            Proxy& operator=(const T& value) {
                _jsonUtil.AddValue(_key, value);
                return *this;
            }

            // T() 연산자 오버로딩(값 읽기)
            template <typename T>
            operator T() const { return _jsonUtil.Deserialize<T>(_key); }
    };

    Proxy operator[](const TCHAR* key) {
        return Proxy(*this, key);
    }

    Proxy operator[](const _tstring& key) {
        return operator[](key.c_str());
    }

    template <typename T>
    inline _tstring Serialize(const _tstring& key, const T& obj, const bool pretty = false);

    template <typename T>
    inline T Deserialize(const _tstring& key);

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

    template <typename T>
    inline void AddVector(const _tstring& key, const std::vector<T>& vec);

    template <typename T>
    inline std::vector<T> GetVector(const _tstring& key);

    template <typename T>
    inline void AddObjectVector(const _tstring& key, const std::vector<T>& vec);

    template <typename T>
    inline std::vector<T> GetObjectVector(const _tstring& key);

    template <typename Key, typename Value>
    inline void AddMap(const _tstring& key, const std::map<Key, Value>& map);

    template <typename Key, typename Value>
    inline std::map<Key, Value> GetMap(const _tstring& key) const;

    template <typename Key, typename T>
    inline void AddObjectMap(const _tstring& key, const std::map<Key, T>& map);

    template <typename Key, typename T>
    inline std::map<Key, T> GetObjectMap(const _tstring& key) const;

private:
    void	Print_DebugInfo(const TCHAR* ptszFormat, ...);

    // C++ 구조체 → JSON 변환(템플릿(T) 변수값을 _tValue 변수에 할당)
    template <typename T>
    _tValue ConvertToJSONValue(const T& value) const;

    // JSON → C++ 구조체 변환(_tValue 변수값을 템플릿(T) 변수에 할당)
    template <typename T>
    T ConvertFromJSONValue(const _tValue& value) const;

    void RecursiveRemove(_tValue& value);

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
    struct has_tojson_method : std::false_type {};

    template <typename T>
    struct has_tojson_method<T, std::void_t<decltype(std::declval<T>().ToJSON(std::declval<_tValue&>(), std::declval<_tDocument::AllocatorType&>()))>> : std::true_type {};

    // 컴파일 타임 에러 유도용 유틸리티
    template <typename T>
    struct dependent_false : std::false_type {};

private:
    _tDocument _document;                       // JSON Document
    _tDocument::AllocatorType& _allocator;      // Allocator
    
    bool _bIsDebugPrint;
};

#include <JSON/RapidJSONUtil.inl>

#endif // ndef __RAPIDJSONUTIL_H__

