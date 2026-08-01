//***************************************************************************
// TestFunction.cpp : implementation of the Test Functions.
//
//***************************************************************************

#include "pch.h"
#include "TestFunction.h"

// 유니코드 설정에 맞춘 _tstring 매크로가 헤더에 정의되어 있다고 가정
extern _tstring g_DirPath;

// 헬퍼 함수: _tstring 환경에 맞춰 filesystem 경로를 _tstring으로 변환
inline _tstring GetPath(const std::filesystem::path& basePath, LPCTSTR fileName) {
    auto fullPath = basePath / fileName;
#ifdef UNICODE
    return fullPath.wstring();
#else
    return fullPath.string();
#endif
}

int GetFileEncodingTypeTest()
{
    _tstring path1 = GetPath(g_DirPath, _T("Ansi.txt"));
    _tstring path2 = GetPath(g_DirPath, _T("UTF16_BE.txt"));
    _tstring path3 = GetPath(g_DirPath, _T("UTF16_LE.txt"));
    _tstring path4 = GetPath(g_DirPath, _T("UTF8_BOM.txt"));
    _tstring path5 = GetPath(g_DirPath, _T("UTF8_NOBOM.txt"));

    EEncoding eEncoding1 = GetFileEncodingType(path1.c_str());
    EEncoding eEncoding2 = GetFileEncodingType(path2.c_str());
    EEncoding eEncoding3 = GetFileEncodingType(path3.c_str());
    EEncoding eEncoding4 = GetFileEncodingType(path4.c_str());
    EEncoding eEncoding5 = GetFileEncodingType(path5.c_str());

    return 0;
}

int ReadFileTest()
{
    _tstring Ansi = ReadFile(GetPath(g_DirPath, _T("Ansi.txt")).c_str());
    _tstring UnicodeBE = ReadFile(GetPath(g_DirPath, _T("UTF16_BE.txt")).c_str());
    _tstring UnicodeLE = ReadFile(GetPath(g_DirPath, _T("UTF16_LE.txt")).c_str());
    _tstring UTF8BOM = ReadFile(GetPath(g_DirPath, _T("UTF8_BOM.txt")).c_str());
    _tstring UTF8NOBOM = ReadFile(GetPath(g_DirPath, _T("UTF8_NOBOM.txt")).c_str());

    WriteFile(GetPath(g_DirPath, _T("__Ansi.txt")).c_str(), Ansi, EEncoding::ANSI);
    WriteFile(GetPath(g_DirPath, _T("__UTF16_BE.txt")).c_str(), UnicodeBE, EEncoding::UTF16_BE);
    WriteFile(GetPath(g_DirPath, _T("__UTF16_LE.txt")).c_str(), UnicodeLE, EEncoding::UTF16_LE);
    WriteFile(GetPath(g_DirPath, _T("__UTF8_BOM.txt")).c_str(), UTF8BOM, EEncoding::UTF8_BOM);
    WriteFile(GetPath(g_DirPath, _T("__UTF8_NOBOM.txt")).c_str(), UTF8NOBOM, EEncoding::UTF8_NOBOM);

    return 0;
}

int SaveFileTest()
{
    TCHAR       tszBuffer[10 * MAX_BUFFER_SIZE];

    _stprintf_s(tszBuffer, _countof(tszBuffer), _T("안녕하세요. 만나서 반갑습니다. 0123456789 ABCDEF ghijklmn 成功(성공) 繁體字(번체자) 简体字(간체자)"));
    int iLength = static_cast<int>(_tcslen(tszBuffer));

    _tstring value(tszBuffer);

    WriteFile(GetPath(g_DirPath, _T("Ansi.txt")).c_str(), value, EEncoding::ANSI);
    WriteFile(GetPath(g_DirPath, _T("UTF16_BE.txt")).c_str(), value, EEncoding::UTF16_BE);
    WriteFile(GetPath(g_DirPath, _T("UTF16_LE.txt")).c_str(), value, EEncoding::UTF16_LE);
    WriteFile(GetPath(g_DirPath, _T("UTF8_BOM.txt")).c_str(), value, EEncoding::UTF8_BOM);
    WriteFile(GetPath(g_DirPath, _T("UTF8_NOBOM.txt")).c_str(), value, EEncoding::UTF8_NOBOM);

    return 0;
}

int Win_GetFileEncodingTypeTest()
{
    EEncoding eEncoding1 = GetFileEncodingType(GetPath(g_DirPath, _T("Ansi.txt")).c_str());
    EEncoding eEncoding2 = GetFileEncodingType(GetPath(g_DirPath, _T("UTF16_BE.txt")).c_str());
    EEncoding eEncoding3 = GetFileEncodingType(GetPath(g_DirPath, _T("UTF16_LE.txt")).c_str());
    EEncoding eEncoding4 = GetFileEncodingType(GetPath(g_DirPath, _T("UTF8_BOM.txt")).c_str());
    EEncoding eEncoding5 = GetFileEncodingType(GetPath(g_DirPath, _T("UTF8_NOBOM.txt")).c_str());

    return 0;
}

int Win_ReadFileTest()
{
    _tstring AnsiDestString;
    _tstring UnicodeBEDestString;
    _tstring UnicodeLEDestString;
    _tstring UTF8BOMDestString;
    _tstring UTF8NOBOMDestString;

    ReadFile(AnsiDestString, GetPath(g_DirPath, _T("Ansi.txt")).c_str());
    ReadFile(UnicodeBEDestString, GetPath(g_DirPath, _T("UTF16_BE.txt")).c_str());
    ReadFile(UnicodeLEDestString, GetPath(g_DirPath, _T("UTF16_LE.txt")).c_str());
    ReadFile(UTF8BOMDestString, GetPath(g_DirPath, _T("UTF8_BOM.txt")).c_str());
    ReadFile(UTF8NOBOMDestString, GetPath(g_DirPath, _T("UTF8_NOBOM.txt")).c_str());

    return 0;
}

int Win_ReadFileMapTest()
{
    _tstring AnsiDestString;
    _tstring UnicodeBEDestString;
    _tstring UnicodeLEDestString;
    _tstring UTF8BOMDestString;
    _tstring UTF8NOBOMDestString;

    ReadFileMap(AnsiDestString, GetPath(g_DirPath, _T("Ansi.txt")).c_str());
    ReadFileMap(UnicodeBEDestString, GetPath(g_DirPath, _T("UTF16_BE.txt")).c_str());
    ReadFileMap(UnicodeLEDestString, GetPath(g_DirPath, _T("UTF16_LE.txt")).c_str());
    ReadFileMap(UTF8BOMDestString, GetPath(g_DirPath, _T("UTF8_BOM.txt")).c_str());
    ReadFileMap(UTF8NOBOMDestString, GetPath(g_DirPath, _T("UTF8_NOBOM.txt")).c_str());

    return 0;
}

int Win_SaveFileTest()
{
    TCHAR       tszBuffer[10 * MAX_BUFFER_SIZE];

    _stprintf_s(tszBuffer, _countof(tszBuffer), _T("안녕하세요. 만나서 반갑습니다. 0123456789 ABCDEF ghijklmn 成功(성공) 繁體字(번체자) 简体字(간체자)"));
    int iLength = static_cast<int>(_tcslen(tszBuffer));

    SaveAnsiFile(GetPath(g_DirPath, _T("Win_Ansi.txt")).c_str(), tszBuffer, iLength + 1);
    SaveUnicodeBEFile(GetPath(g_DirPath, _T("Win_UTF16_BE.txt")).c_str(), tszBuffer, iLength + 1);
    SaveUnicodeLEFile(GetPath(g_DirPath, _T("Win_UTF16_LE.txt")).c_str(), tszBuffer, iLength + 1);
    SaveUTF8BOMFile(GetPath(g_DirPath, _T("Win_UTF8_BOM.txt")).c_str(), tszBuffer, iLength + 1);
    SaveUTF8NOBOMFile(GetPath(g_DirPath, _T("Win_UTF8_NOBOM.txt")).c_str(), tszBuffer, iLength + 1);

    return 0;
}