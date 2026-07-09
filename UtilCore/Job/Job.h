
//***************************************************************************
// Job.h : interface for the CJob class.
//
//***************************************************************************

#ifndef __JOB_H__
#define __JOB_H__

#pragma once

#include <functional>

using CallbackType = std::function<void()>;

class CJob
{
public:
	CJob(function<void()> callback) : _callback(move(callback)) {

	}
	virtual ~CJob() = default;

	void Execute() {
		_callback();
	}

private:
	function<void()> _callback;
};

#endif // ndef __JOB_H__