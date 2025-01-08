
//***************************************************************************
// Singleton.inl : implementation of the CSingleton class.
//
//***************************************************************************

#include "pch.h"
#include "Singleton.h"

#include <assert.h>

//***************************************************************************
//
template<typename T>
T* CSingleton<T>::_pSingleton = nullptr;

//***************************************************************************
//
template<typename T>
std::mutex CSingleton<T>::_mutex;


//***************************************************************************
// Construction/Destruction 
//***************************************************************************

template<typename T>
CSingleton<T>::CSingleton(void)
{
	assert(_pSingleton == nullptr);
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
	if( _pSingleton == nullptr )
		createInstance();

	return *_pSingleton;
}

//***************************************************************************
//
template<typename T>
T* CSingleton<T>::GetSingletonPtr(void)
{
	if( _pSingleton == nullptr )
		createInstance();

	return _pSingleton;
}

//***************************************************************************
//
template<typename T>
void CSingleton<T>::ReleaseInstance(void)
{
	if( nullptr != _pSingleton )
		deleteInstance();
}

//***************************************************************************
//
template<typename T>
void CSingleton<T>::createInstance(void)
{
	std::lock_guard<std::mutex> lockGuard(_mutex);

	if( _pSingleton == nullptr )
		_pSingleton = new T;
}

//***************************************************************************
//
template<typename T>
void CSingleton<T>::deleteInstance(void)
{
	std::lock_guard<std::mutex> lockGuard(_mutex);

	SAFE_DELETE(_pSingleton);
}
