
//***************************************************************************
// Singleton.cpp : implementation of the CSingleton class.
//
//***************************************************************************

#include "pch.h"
#include "Singleton.h"

#include <assert.h>

//***************************************************************************
//
template<typename T>
T* CSingleton<T>::sm_pSingleton = nullptr;

//***************************************************************************
//
template<typename T>
CCriticalSection CSingleton<T>::sm_Lock;


//***************************************************************************
// Construction/Destruction 
//***************************************************************************

template<typename T>
CSingleton<T>::CSingleton(void)
{
	assert(sm_pSingleton == nullptr);
}


template<typename T>
CSingleton<T>::~CSingleton(void)
{
}

//***************************************************************************
//
template<typename T>
T& CSingleton<T>::GetSingleton(void)
{
	if( sm_pSingleton == nullptr )
		createInstance();

	return *sm_pSingleton;
}

//***************************************************************************
//
template<typename T>
T* CSingleton<T>::GetSingletonPtr(void)
{
	if( sm_pSingleton == nullptr )
		createInstance();

	return sm_pSingleton;
}

//***************************************************************************
//
template<typename T>
void CSingleton<T>::ReleaseInstance(void)
{
	if( nullptr != sm_pSingleton )
		deleteInstance();
}

//***************************************************************************
//
template<typename T>
void CSingleton<T>::createInstance(void)
{
	CLockGuard<CCriticalSection>	lockGuard(sm_Lock);

	if( sm_pSingleton == nullptr )
		sm_pSingleton = new T;
}

//***************************************************************************
//
template<typename T>
void CSingleton<T>::deleteInstance(void)
{
	CLockGuard<CCriticalSection>	lockGuard(sm_Lock);
	SAFE_DELETE(sm_pSingleton);
}
