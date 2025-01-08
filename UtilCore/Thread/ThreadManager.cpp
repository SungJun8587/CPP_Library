
//***************************************************************************
// ThreadManager.cpp: implementation of the CThreadManager class.
//
//***************************************************************************

#include "pch.h"
#include "ThreadManager.h"

//***************************************************************************
// Construction/Destruction
//***************************************************************************
CThreadManager::CThreadManager()
	: _threads()
	, _lock()
{
}

CThreadManager::~CThreadManager()
{
	JoinThreads();
}

//***************************************************************************
//
void CThreadManager::CreateThread(function<void(void)> fncCallback)
{
	lock_guard<mutex> lock(_lock);

	// create new thread
	_threads.push_back(thread([=]()
	{
		// set thread id
		InitTLS();
		fncCallback();
		DestroyTLS();
	}));
}

//***************************************************************************
//
void CThreadManager::JoinThreads()
{
	lock_guard<mutex> lock(_lock);

	for( thread& thread : _threads )
	{
		if( thread.joinable() )
			thread.join();
	}

	_threads.clear();
}

//***************************************************************************
//
void CThreadManager::InitTLS()
{
	static std::atomic<uint32> SThreadId = 1;
	LThreadId = SThreadId.fetch_add(1);
}

//***************************************************************************
//
void CThreadManager::DestroyTLS()
{

}