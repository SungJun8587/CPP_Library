
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

#ifndef __CONTAINERS_H__
#include <Memory/Containers.h>
#endif

#ifndef __BASETLS_H__
#include <BaseTLS.h>
#endif

extern thread_local CStack<int32>	LLockStack;

//***************************************************************************
// @brief 락 획득 순서를 추적하여 데드락 발생 여부를 감지하는 프로파일러 클래스입니다.
// @detail 멀티스레드 환경에서 락의 의존성 그래프를 구성하고 DFS를 통해 순환(Cycle)을 탐지합니다.
//***************************************************************************
class CDeadLockProfiler
{
public:
	//***************************************************************************
	// @brief 락을 획득할 때 호출하여 락 계층 구조와 의존성을 기록합니다.
	// @param name 획득한 락의 고유 이름
	//***************************************************************************
	void PushLock(const char* name);

	//***************************************************************************
	// @brief 락을 해제할 때 호출하여 스레드 로컬 락 스택을 관리합니다.
	// @param name 해제할 락의 이름
	//***************************************************************************
	void PopLock(const char* name);

	//***************************************************************************
	// @brief 현재 구성된 락 의존성 그래프에서 순환(데드락)이 존재하는지 확인합니다.
	//***************************************************************************
	void CheckCycle();

private:
	//***************************************************************************
	// @brief DFS(깊이 우선 탐색)를 수행하여 락 의존성 그래프의 순환 여부를 검사합니다.
	// @param index 탐색을 시작할 락의 ID
	//***************************************************************************
	void Dfs(int32 index);

private:
	CUnorderedMap<const char*, int32>	_nameToId;		// 락 이름으로 고유 ID를 조회하기 위한 맵
	CUnorderedMap<int32, const char*>	_idToName;		// 락 ID로 고유 이름을 조회하기 위한 맵
	CMap<int32, CSet<int32>>			_lockHistory;	// 락 간의 획득 순서 관계를 저장하는 인접 리스트 구조의 그래프

	std::mutex	_mutex;									// 프로파일러 내부 데이터 보호를 위한 뮤텍스

private:
	CVector<int32>	_discoveredOrder;					// 노드가 발견된 순서를 기록하는 배열
	int32			_discoveredCount = 0;				// 노드가 발견된 순서 카운트
	CVector<bool>	_finished;							// Dfs(i)가 종료 되었는지 여부
	CVector<int32>	_parent;
};

#endif // ndef __DEADLOCKPROFILER_H__


