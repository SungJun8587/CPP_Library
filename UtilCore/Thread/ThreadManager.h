
//***************************************************************************
// ThreadManager.h : interface for the CThreadManager class.
//
//***************************************************************************

#ifndef __THREADMANAGER_H__
#define __THREADMANAGER_H__

#pragma once

#ifndef _THREAD_
#include <thread>
#endif

#ifndef _FUNCTIONAL_
#include <functional>
#endif

#ifndef _ATOMIC_
#include <atomic>
#endif

#ifndef _MUTEX_
#include <mutex>
#endif

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef __BASETLS_H__
#include <BaseTLS.h>
#endif

class CThreadManager
{
public:
	CThreadManager();
	CThreadManager(const CThreadManager& other) = delete;
	CThreadManager(CThreadManager&& other) = delete;
	CThreadManager& operator=(const CThreadManager& other) = delete;
	CThreadManager& operator=(CThreadManager&& other) = delete;
	~CThreadManager();

	void CreateThread(function<void(void)> function);
	void JoinThreads();

	static void InitTLS();
	static void DestroyTLS();
	
	// 스레드 ID 반환 (TLS 활용)
	static int getThreadID() {
		return LThreadId;
	}

private:
	vector<thread> _threads;
	mutex _lock;
};

#endif // ndef __THREADMANAGER_H__
