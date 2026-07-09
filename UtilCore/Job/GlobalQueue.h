
//***************************************************************************
// GlobalQueue.h : interface for the CGlobalQueue class.
//
//***************************************************************************

#ifndef __GLOBALQUEUE_H__
#define __GLOBALQUEUE_H__

#pragma once

#ifndef __LOCKQUEUE_H__
#include <Job/LockQueue.h>
#endif

class CGlobalQueue
{
public:
	CGlobalQueue();
	~CGlobalQueue();

	void					Push(CJobQueueRef jobQueue);
	CJobQueueRef			Pop();

private:
	CLockQueue<CJobQueueRef> _jobQueues;
};

#endif // ndef __GLOBALQUEUE_H__

