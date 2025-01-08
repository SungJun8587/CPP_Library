
//***************************************************************************
// Singleton.h : interface for the CSingleton class.
//
//***************************************************************************

#ifndef __SINGLETON_H__
#define __SINGLETON_H__

template<typename T>
class CSingleton
{
	void* operator new(size_t size)		{ return ::operator new(size); }	// 다른 곳에서 new 방지용

public:
	CSingleton(void);
	virtual ~CSingleton(void);

	static T& GetSingleton(void);
	static T* GetSingletonPtr(void);
	static void ReleaseInstance(void);

protected:
	static void createInstance(void);
	static void deleteInstance(void);

private:
	CSingleton(const CSingleton&);
	CSingleton& operator=(const CSingleton&);

protected:
	static T*			_pSingleton;
	static std::mutex	_mutex;;
};

#include "Singleton.inl"

#endif // ndef __SINGLETON_H__
