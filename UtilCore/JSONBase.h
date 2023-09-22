
//***************************************************************************
// JSONBase.h : interface and implementation for the CJSONBase class.
//
//***************************************************************************

#ifndef __JSONBASE_H__
#define __JSONBASE_H__

#pragma once

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"				
#include "rapidjson/filewritestream.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/encodings.h"
#include "rapidjson/pointer.h"

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

class CJSONBase
{
public:	
	CJSONBase() {};

	BOOL DeserializeFromFile(const _tstring& filePath);
	BOOL SerializeToFile(const _tstring& filePath, bool isPretty = true);
	_tstring SerializeToJsonText(bool isPretty = true);

	virtual _tstring Serialize(bool isPretty = false) const;
	virtual BOOL Deserialize(const _tstring& s) = 0;
	virtual BOOL Deserialize(const _tValue& obj) = 0;
	virtual BOOL Serialize(_tWriter* writer) const = 0;
	virtual BOOL Serialize(_tPrettyWriter* writer) const = 0;

protected:	
	BOOL InitDocument(const _tstring& s, _tDocument &doc);
};

#endif // ndef __JSONBASE_H__

