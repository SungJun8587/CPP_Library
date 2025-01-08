
//***************************************************************************
// DeadLockProfiler.h : interface for the CDeadLockProfiler class.
//
//***************************************************************************

#ifndef __DEADLOCKPROFILER_H__
#define __DEADLOCKPROFILER_H__

#pragma once

#ifndef _STACK_
#include <stack>
#endif

#ifndef _MAP_
#include <map>
#endif

#ifndef _VECTOR_
#include <vector>
#endif

#ifndef _THREAD_
#include <thread>
#endif

#ifndef _MUTEX_
#include <mutex>
#endif

#ifndef _SET_
#include <set>
#endif

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef __BASETLS_H__
#include <BaseTLS.h>
#endif

class CDeadLockProfiler
{
public:
	void PushLock(const char* name);
	void PopLock(const char* name);
	void CheckCycle();

private:
	void Dfs(int32 index);

private:
	unordered_map<const char*, int32>	_nameToId;
	unordered_map<int32, const char*>	_idToName;
	map<int32, set<int32>>				_lockHistory;

	std::mutex	_mutex;

private:
	vector<int32>	_discoveredOrder;		// ��尡 �߰ߵ� ������ ����ϴ� �迭
	int32			_discoveredCount = 0;	// ��尡 �߰ߵ� ����
	vector<bool>	_finished;				// Dfs(i)�� ���� �Ǿ����� ����
	vector<int32>	_parent;
};

#endif // ndef __DEADLOCKPROFILER_H__