
//***************************************************************************
// JobQueue.h : interface for the CJobQueue class.
//
//***************************************************************************

#ifndef __JOBQUEUE_H__
#define __JOBQUEUE_H__

#pragma once

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#ifndef __BASETLS_H__
#include <BaseTLS.h>
#endif

#ifndef __JOB_H__
#include <Job/Job.h>
#endif

#ifndef __SPINLOCKQUEUE_H__
#include <Containers/Queue/SpinLockQueue.h>
#endif

#ifndef __JOBTIMER_H__
#include <Job/JobTimer.h>
#endif

#ifndef __GLOBALQUEUE_H__
#include <Job/GlobalQueue.h>
#endif

//***************************************************************************
// @brief 전역 타이머 관리 클래스 전방 선언
// @detail 작업 예약 및 시간 지연 실행(Timer) 기능을 담당하는 CJobTimer 클래스입니다.
//***************************************************************************
class CJobTimer;

//***************************************************************************
// @brief 전역 타이머 관리 객체 포인터
// @detail 시스템 전역에서 타이머 작업을 관리하는 CJobTimer 싱글톤 또는 전역 인스턴스입니다.
//***************************************************************************
extern CJobTimer* gpJobTimer;

//***************************************************************************
// @brief 전역 작업 큐 클래스 전방 선언
// @detail 스레드 풀에서 처리할 작업 큐들을 전역적으로 관리하는 CGlobalQueue 클래스입니다.
//***************************************************************************
class CGlobalQueue;

//***************************************************************************
// @brief 전역 작업 큐 객체 포인터
// @detail 시스템 전역에서 대기 중인 작업 큐들을 관리하는 CGlobalQueue 싱글톤 또는 전역 인스턴스입니다.
//***************************************************************************
extern CGlobalQueue* gpGlobalQueue;

//***************************************************************************
// @brief 작업 큐 클래스 전방 선언
// @detail 비동기 작업을 순차적으로 실행하기 위한 CJobQueue 클래스입니다.
//***************************************************************************
class CJobQueue;

//***************************************************************************
// @brief 스레드 지역(TLS) 현재 작업 큐 포인터
// @detail 현재 실행 중인 스레드가 처리하고 있는 작업 큐의 컨텍스트를 유지하기 위한 TLS 변수입니다.
//***************************************************************************
extern thread_local CJobQueue* LCurrentJobQueue;

//***************************************************************************
// @brief 비동기 작업들을 큐에 적재하고 순차적으로 실행하는 작업 큐(Job Queue) 클래스입니다.
// @detail 스레드 안전하게 작업들을 관리하며, 타이머 예약 작업 및 실행 시간 제한(QPC 기준) 등의 기능을 제공합니다.
// 
// @example 사용 예시 (Usage Example):
// @code
//     // 1. CJobQueue를 상속받거나 스마트 포인터로 생성합니다.
//     class CPlayer : public CJobQueue
//     {
//     public:
//         void ProcessMove(int x, int y) {
//             // 플레이어 이동 로직 처리 (싱글스레드 안전성 보장)
//         }
//         
//         void BroadcastChat(std::string message) {
//             // 채팅 메시지 브로드캐스트 처리
//         }
//     };
//
//     auto player = std::make_shared<CPlayer>();
//
//     // 2. 람다식을 이용한 일반 비동기 작업 등록 (DoAsync)
//     player->DoAsync([]() {
//         // 비동기로 처리할 작업 내용
//     });
//
//     // 3. 멤버 함수와 인자를 바인딩하여 비동기 작업 등록 (DoAsync 템플릿)
//	   // 플레이어 객체(player)가 가진 멤버 함수(ProcessMove)와 그에 전달할 인자(100, 200)를 
//     // 비동기 작업(Job)으로 묶어서 작업 큐에 등록하는 코드
//     player->DoAsync(&CPlayer::ProcessMove, 100, 200);
//
//     // 4. 지정된 시간(예: 3000ms = 3초) 뒤에 실행되는 타이머 작업 예약 (DoTimer)
//     player->DoTimer(3000, &CPlayer::BroadcastChat, std::string("환영합니다!"));
// @endcode
//***************************************************************************
class CJobQueue : public enable_shared_from_this<CJobQueue>
{
	enum : uint64
	{
		// 스레드당 최대 연속 실행 시간 (ms 단위, 내부에서 QPC 카운트로 변환)
		EXECUTE_TIME_LIMIT_MS = 10ULL	// 10ms
	};

public:
	void DoAsync(CallbackType&& callback);

	template<typename T, typename Ret, typename... Args>
	void DoAsync(Ret(T::* memFunc)(Args...), Args... args);

	void DoTimer(uint64 tickAfterMs, CallbackType&& callback);

	template<typename T, typename Ret, typename... Args>
	void DoTimer(uint64 tickAfterMs, Ret(T::* memFunc)(Args...), Args... args);

	void ClearJobs() { _jobs.Clear(); }

public:
	void Push(CJobRef job, bool pushOnly = false);
	void Execute();

protected:
	CSpinLockQueue<CJobRef>	_jobs;			// 스레드 안전한 큐에 저장된 작업 목록
	Atomic<int32>			_jobCount = 0;	// 현재 큐에 대기 중인 작업 개수
};

#include <Job/JobQueue.inl>

#endif // ndef __JOBQUEUE_H__