
//***************************************************************************
// WebUtil.h: interface for the WebUtil Function.
//
//***************************************************************************

#ifndef	__WEBUTIL_H__
#define	__WEBUTIL_H__

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef __ENCODINGCONVERT_H__
#include <Util/EncodingConvert.h>
#endif

//***************************************************************************
// @brief Base64Dec 결과 코드. 호출부에서 실패를 명시적으로 확인할 수 있도록
// 에러 코드로 반환한다(민감 데이터에서 손상된 입력을 조용히 넘기지 않기 위함).
enum class Base64DecodeResult
{
	Success = 0,
	InvalidLength,      // 길이가 4의 배수가 아님
	InvalidCharacter,   // Base64 알파벳/패딩에 속하지 않는 문자가 포함됨
};

//***************************************************************************
// 웹 유틸리티 함수 선언
//***************************************************************************

// Base64 인코딩 / 디코딩
_tstring Base64Enc(const _tstring& source);
_tstring Base64Dec(const _tstring& source, Base64DecodeResult* pResult);

// URL 인코딩 / 디코딩
_tstring UrlEncode(const _tstring& source, const int iCodePage = CP_ACP);
_tstring UrlDecode(const _tstring& source, const int iCodePage = CP_ACP);
_tstring UrlPathEncode(const _tstring& source);

// HTML 인코딩 / 디코딩
_tstring HtmlEncode(const _tstring& source);
_tstring HtmlDecode(const _tstring& source);

// 자바스크립트 Escape / UnEscape
_tstring Escape(const _tstring& source);
_tstring UnEscape(const _tstring& source);

// 자바스크립트 EncodeURI / DecodeURI
_tstring EncodeURI(const _tstring& source, bool bSpaceAsPlus);
_tstring DecodeURI(const _tstring& source);

// 자바스크립트 EncodeURIComponent / DecodeURIComponent
_tstring EncodeURIComponent(const _tstring& source);
_tstring DecodeURIComponent(const _tstring& source);

#endif // ndef __WEBUTIL_H__
