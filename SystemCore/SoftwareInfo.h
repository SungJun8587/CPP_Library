
//***************************************************************************
// SoftwareInfo.h: interface for the Software Information Class.
//
//***************************************************************************

#ifndef __SOFTWAREINFO_H__
#define __SOFTWAREINFO_H__

#ifndef __SAFELINKEDLIST_H__
#include <SafeLinkedList.h>
#endif

BOOL GetVersionLangOfFile(TCHAR *ptszAppName, TCHAR *ptszVersion, TCHAR *ptszLanguage);

//***************************************************************************
//
typedef struct _SWINFO_IE
{
public:
	_SWINFO_IE() {
		m_tszBuild[0] = '\0';
		m_tszVersion[0] = '\0';
	}

	TCHAR	m_tszBuild[IE_BUILD_STRLEN];
	TCHAR	m_tszVersion[IE_VERSION_STRLEN];

} SWINFO_IE, *PSWINFO_IE;

//***************************************************************************
//
typedef struct _SWINFO_DIRECTX
{
public:
	_SWINFO_DIRECTX() {
		m_tszVersion[0] = '\0';
		m_tszInstallVersion[0] = '\0';
		m_tszDescription[0] = '\0';
	}

	TCHAR	m_tszVersion[DIRECTX_VERSION_STRLEN];
	TCHAR	m_tszInstallVersion[DIRECTX_INSTALLVERSION_STRLEN];
	TCHAR   m_tszDescription[DIRECTX_DESCRIPTION_STRLEN];

} SWINFO_DIRECTX, *PSWINFO_DIRECTX;

//***************************************************************************
//
typedef struct _INSTALL_SWINFO
{
public:
	_INSTALL_SWINFO() {
		m_tszDisplayName[0] = '\0';
		m_tszInstallSource[0] = '\0';
		m_tszUninstallString[0] = '\0';
	}

	TCHAR	m_tszDisplayName[INSTALL_SWINFO_DISPLAYNAME_STRLEN];
	TCHAR	m_tszInstallSource[INSTALL_SWINFO_INSTALLSOURCE_STRLEN];
	TCHAR	m_tszUninstallString[INSTALL_SWINFO_UNINSTALLSTRING_STRLEN];

} INSTALL_SWINFO, *PINSTALL_SWINFO;

//***************************************************************************
//
class CIeInfo
{
public:
	CIeInfo();
	~CIeInfo();

	BOOL	GetInformation();

	const TCHAR* GetBuild() const {
		return m_Ie.m_tszBuild;
	}
	const TCHAR* GetVersion() const {
		return m_Ie.m_tszVersion;
	}

private:
	SWINFO_IE	m_Ie;
};

//***************************************************************************
//
class CDirectXInfo
{
public:
	CDirectXInfo();
	~CDirectXInfo();

	BOOL	GetInformation();

	const TCHAR* GetVersion() const {
		return m_DirectX.m_tszVersion;
	}
	const TCHAR* GetInstallVersion() const {
		return m_DirectX.m_tszInstallVersion;
	}
	const TCHAR* GetDescription() const {
		return m_DirectX.m_tszDescription;
	}

private:
	SWINFO_DIRECTX	m_DirectX;
};

//***************************************************************************
//
class CJavaVMInfo
{
public:
	CJavaVMInfo();
	~CJavaVMInfo();

	BOOL	GetInformation();

	BOOL	GetVersionMsJVM(TCHAR *ptszMsJVMVersion);
	BOOL	GetVersionSunJVM(TCHAR *ptszSunJVMVersion);

	int		IsJVM() const {
		return m_nIsJVM;
	}

private:
	int		m_nIsJVM;
};

//***************************************************************************
//
class CInstallSwInfo
{
public:
	CInstallSwInfo();
	~CInstallSwInfo();

	BOOL	GetInformation();

	CBaseLinkedList<INSTALL_SWINFO *>* GetInstallSwInfoArray() {
		return &m_sInstallSwInfoArray;
	}

private:
	CBaseLinkedList<INSTALL_SWINFO *> m_sInstallSwInfoArray;
};

#endif // ndef __SOFTWAREINFO_H__
