
//***************************************************************************
// ThreadManager.h : interface for the CThreadManager class.
//
//***************************************************************************

#ifndef __THREADMANAGER_H__
#define __THREADMANAGER_H__

class CThreadManager
{
public:
	// Thread Local Storage
	inline static void SetTlsValue(const __int32& nIdx)
	{
		m_nTlsIdx = nIdx;
	}

	inline static int GetTlsValue(void)
	{
		return m_nTlsIdx;
	}

private:
	static __declspec(thread) __int32	m_nTlsIdx;
};

#endif // ndef __THREADMANAGER_H__
