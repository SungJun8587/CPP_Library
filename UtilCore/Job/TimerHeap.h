
//***************************************************************************
// TimerHeap.h : interface for the CTimerHeap class.
//
//***************************************************************************

#ifndef UC_TIMERHEAP_H
#define UC_TIMERHEAP_H

#include <algorithm>
#include <cstdint>
#include <unordered_set>
#include <utility>

#include <Memory/Containers.h>

//***************************************************************************
// @class CTimerHeap
// @brief 취소 가능한 지연 작업 힙. 시간 단위는 호출자가 정한다(존 논리 시간 ms 등).
// @details 스레드 안전하지 않으므로 소유 스레드(존 스레드)에서만 사용한다.
//          Cancel()은 지연 삭제 방식이다 — 힙에서 즉시 빼지 않고 "유효 ID 집합"에서만 지우며,
//          만료 시점에 건너뛴다. 취소가 많아 힙이 부풀면 자동으로 재구성한다.
//          같은 시각의 작업은 등록 순서(FIFO)로 실행된다.
//          RunDue()의 콜백 안에서 Schedule()/Cancel()을 호출해도 안전하다.
// @tparam TJob 실행할 작업 타입 (이동 가능해야 함)
//***************************************************************************
template <typename TJob>
class CTimerHeap
{
public:
	CTimerHeap() = default;

	CTimerHeap(const CTimerHeap&) = delete;
	CTimerHeap& operator=(const CTimerHeap&) = delete;

	//***************************************************************************
	// @brief 작업을 예약합니다.
	// @param dueMs 실행 예정 시각
	// @param job   실행할 작업
	// @return 취소에 쓰는 타이머 ID (항상 1 이상)
	//***************************************************************************
	uint64_t Schedule(uint64_t dueMs, TJob job)
	{
		const uint64_t id = ++_lastId;

		SEntry entry;
		entry.dueMs = dueMs;
		entry.id = id;
		entry.job = std::move(job);

		_heap.push_back(std::move(entry));
		std::push_heap(_heap.begin(), _heap.end(), &CTimerHeap::Greater);
		_live.insert(id);
		return id;
	}

	//***************************************************************************
	// @brief 예약을 취소합니다.
	// @param id Schedule()이 반환한 타이머 ID
	// @return 아직 실행되지 않은 예약을 취소했으면 true
	//***************************************************************************
	bool Cancel(uint64_t id)
	{
		if( _live.erase(id) == 0 )
			return false;

		// 취소 항목이 힙의 대부분이 되면 재구성해 메모리를 회수한다.
		if( _heap.size() > kCompactMinSize && _heap.size() > _live.size() * 2 )
			Compact();

		return true;
	}

	//***************************************************************************
	// @brief 만료된 작업을 시각 순으로 모두 꺼내 실행합니다.
	// @tparam F void(TJob&) 형태의 실행 함수 (예외 격리는 호출자가 담당)
	// @param nowMs 현재 시각
	// @param run   작업 실행 함수
	// @return 실행한 작업 수 (취소된 항목 제외)
	//***************************************************************************
	template <typename F>
	size_t RunDue(uint64_t nowMs, F&& run)
	{
		size_t count = 0;

		while( !_heap.empty() && _heap.front().dueMs <= nowMs )
		{
			std::pop_heap(_heap.begin(), _heap.end(), &CTimerHeap::Greater);
			SEntry entry = std::move(_heap.back());
			_heap.pop_back();

			if( _live.erase(entry.id) == 0 )
				continue;

			run(entry.job);
			++count;
		}

		return count;
	}

	//***************************************************************************
	// @brief 대기 중인 (취소되지 않은) 작업 수를 반환합니다.
	// @return 대기 작업 수
	//***************************************************************************
	size_t Size() const { return _live.size(); }

	//***************************************************************************
	// @brief 모든 예약을 버립니다.
	//***************************************************************************
	void Clear()
	{
		_heap.clear();
		_live.clear();
	}

private:
	struct SEntry
	{
		uint64_t dueMs = 0;		// 실행 예정 시각
		uint64_t id = 0;		// 타이머 ID (동일 시각 FIFO 보장용 순번 겸용)
		TJob     job;			// 실행할 작업
	};

	static constexpr size_t kCompactMinSize = 64;		// 재구성을 고려하는 최소 힙 크기

	static bool Greater(const SEntry& a, const SEntry& b)
	{
		if( a.dueMs != b.dueMs )
			return a.dueMs > b.dueMs;
		return a.id > b.id;
	}

	void Compact()
	{
		size_t out = 0;
		for( size_t i = 0; i < _heap.size(); ++i )
		{
			if( _live.find(_heap[i].id) == _live.end() )
				continue;

			if( out != i )
				_heap[out] = std::move(_heap[i]);
			++out;
		}

		_heap.erase(_heap.begin() + out, _heap.end());
		std::make_heap(_heap.begin(), _heap.end(), &CTimerHeap::Greater);
	}

	CVector<SEntry>				_heap;				// 최소 힙 (dueMs, id 순)
	std::unordered_set<uint64_t>	_live;				// 취소되지 않은 대기 타이머 ID
	uint64_t					_lastId = 0;		// 마지막으로 발급한 타이머 ID
};

#endif // ndef UC_TIMERHEAP_H
