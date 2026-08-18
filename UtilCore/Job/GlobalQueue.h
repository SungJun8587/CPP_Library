
//***************************************************************************
// GlobalQueue.h : interface for the CGlobalQueue class.
//
//***************************************************************************

#ifndef __GLOBALQUEUE_H__
#define __GLOBALQUEUE_H__

#pragma once

#ifndef __SPINLOCKQUEUE_H__
#include <Containers/Queue/SpinLockQueue.h>
#endif

//***************************************************************************
// @brief 전역 작업 큐(Global Job Queue)를 관리하는 클래스입니다.
// @detail 멀티스레드 환경에서 안전하게 작업 큐(Job Queue)들을 적재하고 인출할 수 있는
//         스핀락 기반의 큐 컨테이너를 제공합니다.
//***************************************************************************
class CGlobalQueue
{
public:
	CGlobalQueue();
	~CGlobalQueue();

	void					Push(CJobQueueRef jobQueue);
	CJobQueueRef			Pop();

private:
	CSpinLockQueue<CJobQueueRef> _jobQueues; // 작업 큐(Job Queue)들을 안전하게 저장하는 스핀락 큐
};

#endif // ndef __GLOBALQUEUE_H__