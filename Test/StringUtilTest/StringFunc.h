
//***************************************************************************
// StringFunc.h : interface for the C String Functions.
//
//***************************************************************************

#ifndef __STRINGFUNC_H__
#define __STRINGFUNC_H__

void Print_Va_List(TCHAR* ptszDest, size_t size, const TCHAR* ptszFmt, va_list arg_buff);
void Print(const TCHAR* ptszFuncName, size_t nResult, const TCHAR* ptszFmt, ...);
void Print(const TCHAR* ptszFuncName, TCHAR* ptszDest, const TCHAR* ptszFmt, ...);

void func_memcpy_s();
void func_strcopy_s();
void func_strncopy_s();
void func_strcat_s();
void func_strncat_s();
void func_strupr_s();
void func_strlwr_s();
void func_strset_s();
void func_strnset_s();
void func_strtok_s();
void func_mbstowcs_s();
void func_wcstombs_s();
void func_strerror_s();
void func_sprintf_s();
void func_snprintf_s();
void func_vsprintf_s(const TCHAR* ptszFmt, ...);
void func_vsnprintf_s(const TCHAR* ptszFmt, ...);

void func_memcpy();
void func_memset();
void func_memmove();
void func_memcmp();
void func_memchr();
void func_sizeof();
void func_countof();
void func_strlen();
void func_strcopy();
void func_strncopy();
void func_strdup();
void func_strcat();
void func_strncat();
void func_strcmp();
void func_strncmp();
void func_stricmp();
void func_strnicmp();
void func_strcoll();
void func_strchr();
void func_strrchr();
void func_strstr();
void func_strupr();
void func_strlwr();
void func_strset();
void func_strnset();
void func_strrev();
void func_strtok();
void func_strpbrk();
void func_strcspn();
void func_strspn();
void func_strxfrm();
void func_mbstowcs();
void func_wcstombs();
void func_strerror();
void func_printf();
void func_sprintf();
void func_snprintf();
void func_vprintf(const TCHAR* ptszFmt, ...);
void func_vsprintf(const TCHAR* ptszFmt, ...);
void func_vsnprintf(const TCHAR* ptszFmt, ...);

#endif // ndef __STRINGFUNC_H__
