
//***************************************************************************
// JSONBase.cpp: implementation of the CJSONBase class.
//
//***************************************************************************

#include "pch.h"
#include "JSONBase.h"

//***************************************************************************
//
_tstring CJSONBase::Serialize(bool isPretty) const
{
	_tStringBuffer ss;

	if( isPretty )
	{
		_tPrettyWriter writer(ss);		// Can also use Writer for condensed formatting

		if( Serialize(&writer) )
			return ss.GetString();
	}
	else
	{ 
		_tWriter writer(ss);		// Can also use Writer for condensed formatting
	
		if( Serialize(&writer) )
			return ss.GetString();
	}
	return _T("");
}

//***************************************************************************
//
BOOL CJSONBase::Deserialize(const _tstring& s)
{
	_tDocument doc;
	if( !InitDocument(s, doc) )
		return false;

	Deserialize(doc);

	return true;
}

//***************************************************************************
//
BOOL CJSONBase::DeserializeFromFile(const _tstring& filePath)
{
	_tifstream f(filePath);
	_tstringstream buffer;

	buffer << f.rdbuf();
	f.close();

	return Deserialize(buffer.str());
}

//***************************************************************************
//
BOOL CJSONBase::SerializeToFile(const _tstring& filePath, bool isPretty)
{
	_tofstream f(filePath);
	_tstring s = Serialize(isPretty);

	f << s;
	f.flush();
	f.close();

	return true;
}

//***************************************************************************
//
_tstring CJSONBase::SerializeToJsonText(bool isPretty)
{
	_tstring s = Serialize(isPretty);

	return s;
}

//***************************************************************************
//
BOOL CJSONBase::InitDocument(const _tstring& s, _tDocument& doc)
{
	if( s.empty() )
		return false;

	_tstring validJson(s);

	return !doc.Parse(validJson.c_str()).HasParseError() ? true : false;
}