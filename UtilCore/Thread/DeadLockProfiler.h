
//***************************************************************************
// DeadLockProfiler.h : interface for the CDeadLockProfiler class.
//
//***************************************************************************

#ifndef UC_DEADLOCKPROFILER_H
#define UC_DEADLOCKPROFILER_H

#include <stack>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <set>

#include <BaseRedefineDataType.h>
#include <BaseTLS.h>
#include <Memory/Containers.h>

extern thread_local std::stack<int32>	LLockStack;

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
	std::unordered_map<const char*, int32>	_nameToId;		// 락 이름으로 고유 ID를 조회하기 위한 맵
	std::unordered_map<int32, const char*>	_idToName;		// 락 ID로 고유 이름을 조회하기 위한 맵
	std::map<int32, std::set<int32>>		_lockHistory;	// 락 간의 획득 순서 관계를 저장하는 인접 리스트 구조의 그래프

	std::mutex	_mutex;										// 프로파일러 내부 데이터 보호를 위한 뮤텍스

private:
	std::vector<int32>	_discoveredOrder;					// 노드가 발견된 순서를 기록하는 배열
	int32				_discoveredCount = 0;				// 노드가 발견된 순서 카운트
	std::vector<bool>	_finished;							// Dfs(i)가 종료 되었는지 여부
	std::vector<int32>	_parent;
};

#endif // ndef UC_DEADLOCKPROFILER_H


