
//***************************************************************************
// This File include Information about define macro.
// 
//***************************************************************************

#ifndef __BASEMACRO_H__
#define __BASEMACRO_H__

#pragma once

#define NAMESPACE_BEGIN(name)	namespace name {
#define NAMESPACE_END			}

#define SVR									CServiceSvr::GetSvrInstance()
#define TLS_IDX								CThreadManager::GetTlsValue()
#define SERVER_CONFIG						CServerConfig::GetSingletonPtr()

#define CRASH								{ char *p = 0; *p = 1; }
#define SAFE_DELETE(p)						{ if ( p ) delete p; p = nullptr; } 
#define SAFE_DELETE_ARRAY(p)				{ if ( p ) delete[] p; p = nullptr; }
#define USING_SHARED_PTR(name)	using name##Ref = std::shared_ptr<class name>;

#define CRASH_CAUSE(cause)						\
{											\
	uint32* crash = nullptr;				\
	__analysis_assume(crash != nullptr);	\
	*crash = 0xDEADBEEF;					\
}

#define ASSERT_CRASH(expr)			\
{									\
	if (!(expr))					\
	{								\
		CRASH_CAUSE("ASSERT_CRASH");		\
		__analysis_assume(expr);	\
	}								\
}

#ifdef _UNICODE
#	define _tmemset			wmemset
#	define _tmemcpy			wmemcpy
#	define __TFUNCTION__	__FUNCTIONW__
#else
#	define _tmemset			memset
#	define _tmemcpy			memcpy
#	define __TFUNCTION__	__FUNCTION__
#endif

#if (_WIN32_WINNT >= 0x0600)
#define _GetTickCount	GetTickCount64
#else
#define _GetTickCount	GetTickCount
#endif

#define _STOMP

#ifdef _WIN64
#	ifdef _DEBUG
#		define LIB_NAME(LIB) LIB##"64D.lib"
#	else
#		define LIB_NAME(LIB) LIB##"64.lib"
#	endif
#else
#	ifdef _DEBUG
#		define LIB_NAME(LIB) LIB##"32D.lib"
#	else
#		define LIB_NAME(LIB) LIB##"32.lib"
#	endif
#endif

#endif // ndef __BASEMACRO_H__

