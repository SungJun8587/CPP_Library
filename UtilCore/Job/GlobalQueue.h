
//***************************************************************************
// GlobalQueue.h : interface for the CGlobalQueue class.
//
//***************************************************************************

#ifndef __GLOBALQUEUE_H__
#define __GLOBALQUEUE_H__

#pragma once

#ifndef __SPINLOCKQUEUE_H__
#include <ThreadSafeContainers/SpinLockQueue.h>
#endif

class CGlobalQueue
{
public:
	CGlobalQueue();
	~CGlobalQueue();

	void					Push(CJobQueueRef jobQueue);
	CJobQueueRef			Pop();

private:
	CSpinLockQueue<CJobQueueRef> _jobQueues;
};

#endif // ndef __GLOBALQUEUE_H__

