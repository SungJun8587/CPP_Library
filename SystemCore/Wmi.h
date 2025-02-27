
//***************************************************************************
// Wmi.h: interface for the CWmi Class.
//
//***************************************************************************

#ifndef __WMI_H__
#define __WMI_H__

#include <WbemIdl.h>

#pragma  comment(lib, "wbemuuid.lib")

//***************************************************************************
//
typedef struct _WMI_CLASSPROPERTIES
{
public:
	_WMI_CLASSPROPERTIES() {
		m_tszName[0] = '\0';
		VariantInit(&m_vtValue);
	}

	~_WMI_CLASSPROPERTIES() {
		VariantClear(&m_vtValue);
	}

	BOOL SetName(WCHAR *pwszName)
	{
#ifdef _UNICODE
		StringCbCopy(m_tszName, _countof(m_tszName), pwszName);
#else
		CMemBuffer<char>	CName;

		if( !UnicodeToAnsi(CName, pwszName, wcslen(pwszName)) ) return false;

		StringCbCopy(m_tszName, _countof(m_tszName), CName.GetBuffer());
#endif

		return true;
	}

	TCHAR		m_tszName[32];
	VARIANT		m_vtValue;

} WMI_CLASSPROPERTIES, *PWMI_CLASSPROPERTIES;

//***************************************************************************
//
class CWmi
{
public:
	CWmi();
	~CWmi();

	BOOL	Connect(TCHAR *ptszHost = NULL, TCHAR *ptszUserName = NULL, TCHAR *ptszUserPass = NULL);
	int		ExecQuery(const TCHAR *ptszQuery);

	BOOL	GetProperties(int nIndex, const TCHAR *ptszProperty, VARIANT &vtVal);
	BOOL	GetProperties(int nIndex, const TCHAR *ptszProperty, TCHAR *ptszValue, DWORD dwSize);
	BOOL	GetProperties(int nIndex, const TCHAR *ptszProperty, long *plValue);

private:
	IWbemLocator	*m_pIWbemLocator;
	IWbemServices	*m_pIWbemServices;

	CBaseLinkedList<IWbemClassObject *> MemClassObject;
};

#endif // ndef __WMI_H__

