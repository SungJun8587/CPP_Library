
//***************************************************************************
// GlobalQueue.cpp : implementation of the CGlobalQueue class.
//
//***************************************************************************

#include "pch.h"
#include "GlobalQueue.h"

CGlobalQueue::CGlobalQueue()
{

}

CGlobalQueue::~CGlobalQueue()
{

}

void CGlobalQueue::Push(CJobQueueRef jobQueue)
{
	_jobQueues.Push(jobQueue);
}

CJobQueueRef CGlobalQueue::Pop()
{
	return _jobQueues.Pop();
}
