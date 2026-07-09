
//***************************************************************************
// DeadLockProfiler.cpp: implementation of the CDeadLockProfiler class.
//
//***************************************************************************

#include "pch.h"
#include "DeadLockProfiler.h"

//***************************************************************************
//
void CDeadLockProfiler::PushLock(const char* name)
{
	std::lock_guard<std::mutex> guard(_mutex);

	// 아이디를 찾거나 발급한다.
	int32 lockId = 0;

	auto findIt = _nameToId.find(name);
	if( findIt == _nameToId.end() )
	{
		lockId = static_cast<int32>(_nameToId.size());
		_nameToId[name] = lockId;
		_idToName[lockId] = name;
	}
	else
	{
		lockId = findIt->second;
	}

	// 잡고 있는 락이 있었다면
	if( LLockStack.empty() == false )
	{
		// 기존에 발견되지 않은 케이스라면 데드락 여부 다시 확인한다.
		const int32 prevId = LLockStack.top();
		if( lockId != prevId )
		{
			CSet<int32>& history = _lockHistory[prevId];
			if( history.find(lockId) == history.end() )
			{
				history.insert(lockId);
				CheckCycle();
			}
		}
	}

	LLockStack.push(lockId);
}

//***************************************************************************
//
void CDeadLockProfiler::PopLock(const char* name)
{
	std::lock_guard<std::mutex> guard(_mutex);

	if( LLockStack.empty() )
		CRASH("MULTIPLE_UNLOCK");

	int32 lockId = _nameToId[name];
	if( LLockStack.top() != lockId )
		CRASH("INVALID_UNLOCK");

	LLockStack.pop();
}

//***************************************************************************
//
void CDeadLockProfiler::CheckCycle()
{
	const int32 lockCount = static_cast<int32>(_nameToId.size());

	// 💡 std::vector 대신 Containers.h의 CVector를 사용하도록 교체
	_discoveredOrder = CVector<int32>(lockCount, -1);
	_discoveredCount = 0;
	_finished = CVector<bool>(lockCount, false);
	_parent = CVector<int32>(lockCount, -1);

	for( int32 lockId = 0; lockId < lockCount; lockId++ )
		Dfs(lockId);

	// 연산이 끝났으면 정리한다.
	// 💡 [성능 팁]: 커스텀 할당기(특히 Pool 방식)를 사용한다면 clear() 후 shrink_to_fit()을 하거나,
	// 매번 새 vector를 대입하기보다는 멤버 변수를 유지(Reuse)하고 assign()이나 clear()만 하는 것이
	// 메모리 재할당 오버헤드를 줄이는 데 훨씬 유리합니다. 현재 구조는 매번 새로 할당하므로 그대로 유지합니다.
	_discoveredOrder.clear();
	_finished.clear();
	_parent.clear();
}

//***************************************************************************
//
void CDeadLockProfiler::Dfs(int32 here)
{
	if( _discoveredOrder[here] != -1 )
		return;

	_discoveredOrder[here] = _discoveredCount++;

	// 모든 인접한 정점을 순회한다.
	auto findIt = _lockHistory.find(here);
	if( findIt == _lockHistory.end() )
	{
		_finished[here] = true;
		return;
	}

	CSet<int32>& nextSet = findIt->second;
	for( int32 there : nextSet )
	{
		// 아직 방문한 적이 없다면 방문한다.
		if( _discoveredOrder[there] == -1 )
		{
			_parent[there] = here;
			Dfs(there);
			continue;
		}

		// here가 there보다 먼저 발견되었다면, there는 here의 후손이다. (순방향 간선)
		if( _discoveredOrder[here] < _discoveredOrder[there] )
			continue;

		// 순방향이 아니고, Dfs(there)가 아직 종료하지 않았다면, there는 here의 선조이다. (역방향 간선)
		if( _finished[there] == false )
		{
			printf("%s -> %s\n", _idToName[here], _idToName[there]);

			int32 now = here;
			while( true )
			{
				printf("%s -> %s\n", _idToName[_parent[now]], _idToName[now]);
				now = _parent[now];
				if( now == there )
					break;
			}

			CRASH("DEADLOCK_DETECTED");
		}
	}

	_finished[here] = true;
}
