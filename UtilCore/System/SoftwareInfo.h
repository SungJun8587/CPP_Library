
//***************************************************************************
// SoftwareInfo.h: interface for the Software Information Class.
//
//***************************************************************************

#ifndef __SOFTWAREINFO_H__
#define __SOFTWAREINFO_H__

#include <vector>

#ifndef __SYSTEMBASEDEFINE_H__
#include <System/SystemBaseDefine.h>
#endif

#ifndef __OSINFO_H__
#include <System/OsInfo.h>
#endif

BOOL GetVersionLangOfFile(TCHAR* ptszAppName, TCHAR* ptszVersion, TCHAR* ptszLanguage);

//***************************************************************************
// @struct  _SWINFO_IE
// @brief   Internet Explorer의 버전 및 빌드 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _SWINFO_IE
{
public:
	_SWINFO_IE() {
		m_tszBuild[0] = '\0';
		m_tszVersion[0] = '\0';
	}

	TCHAR	m_tszBuild[IE_BUILD_STRLEN];     // IE 빌드 번호 문자열
	TCHAR	m_tszVersion[IE_VERSION_STRLEN]; // IE 버젼 문자열

} SWINFO_IE, * PSWINFO_IE;

//***************************************************************************
// @struct  _SWINFO_DIRECTX
// @brief   DirectX 설치 버전 및 설명 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _SWINFO_DIRECTX
{
public:
	_SWINFO_DIRECTX() {
		m_tszVersion[0] = '\0';
		m_tszInstallVersion[0] = '\0';
		m_tszDescription[0] = '\0';
	}

	TCHAR	m_tszVersion[DIRECTX_VERSION_STRLEN];               // DirectX 버전 문자열
	TCHAR	m_tszInstallVersion[DIRECTX_INSTALLVERSION_STRLEN];  // DirectX 설치 버전 수치 문자열
	TCHAR	m_tszDescription[DIRECTX_DESCRIPTION_STRLEN];     // DirectX 버전에 대한 상세 설명

} SWINFO_DIRECTX, * PSWINFO_DIRECTX;

//***************************************************************************
// @struct  _INSTALL_SWINFO
// @brief   시스템에 설치된 응용 프로그램의 레지스트리 정보를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _INSTALL_SWINFO
{
public:
	_INSTALL_SWINFO() {
		m_tszDisplayName[0] = '\0';
		m_tszInstallSource[0] = '\0';
		m_tszUninstallString[0] = '\0';
	}

	TCHAR	m_tszDisplayName[INSTALL_SWINFO_DISPLAYNAME_STRLEN];         // 소프트웨어 표시 이름
	TCHAR	m_tszInstallSource[INSTALL_SWINFO_INSTALLSOURCE_STRLEN];     // 설치 소스 경로
	TCHAR	m_tszUninstallString[INSTALL_SWINFO_UNINSTALLSTRING_STRLEN]; // 제거 명령어 문자열

} INSTALL_SWINFO, * PINSTALL_SWINFO;

//***************************************************************************
// @class   CIeInfo
// @brief   Internet Explorer 정보 수집 클래스입니다.
//***************************************************************************
class CIeInfo
{
public:
	CIeInfo();
	~CIeInfo();

	BOOL	GetInformation();

	//***************************************************************************
	// @brief   수집된 IE 빌드 번호를 반환합니다.
	// @return  const TCHAR* 빌드 번호 문자열 포인터
	//***************************************************************************
	const TCHAR* GetBuild() const {
		return m_Ie.m_tszBuild;
	}

	//***************************************************************************
	// @brief   수집된 IE 버전을 반환합니다.
	// @return  const TCHAR* 버전 문자열 포인터
	//***************************************************************************
	const TCHAR* GetVersion() const {
		return m_Ie.m_tszVersion;
	}

private:
	SWINFO_IE	m_Ie; // IE 수집 정보 구조체
};

//***************************************************************************
// @class   CDirectXInfo
// @brief   DirectX 설치 정보를 관리하는 클래스입니다.
//***************************************************************************
class CDirectXInfo
{
public:
	CDirectXInfo();
	~CDirectXInfo();

	BOOL	GetInformation();

	//***************************************************************************
	// @brief   수집된 DirectX 버전을 반환합니다.
	// @return  const TCHAR* 버전 문자열 포인터
	//***************************************************************************
	const TCHAR* GetVersion() const {
		return m_DirectX.m_tszVersion;
	}

	//***************************************************************************
	// @brief   수집된 DirectX 설치 버전 수치 문자열을 반환합니다.
	// @return  const TCHAR* 설치 버전 문자열 포인터
	//***************************************************************************
	const TCHAR* GetInstallVersion() const {
		return m_DirectX.m_tszInstallVersion;
	}

	//***************************************************************************
	// @brief   수집된 DirectX 명칭/설명 문자열을 반환합니다.
	// @return  const TCHAR* 설명 문자열 포인터
	//***************************************************************************
	const TCHAR* GetDescription() const {
		return m_DirectX.m_tszDescription;
	}

private:
	SWINFO_DIRECTX	m_DirectX; // DirectX 수집 정보 구조체
};

//***************************************************************************
// @class   CJavaVMInfo
// @brief   시스템 내 Java Virtual Machine (MS JVM / Sun JVM) 설치 상태 및 정보를 관리하는 클래스입니다.
//***************************************************************************
class CJavaVMInfo
{
public:
	CJavaVMInfo();
	~CJavaVMInfo();

	BOOL	GetInformation();

	BOOL	GetVersionMsJVM(TCHAR* ptszMsJVMVersion);
	BOOL	GetVersionSunJVM(TCHAR* ptszSunJVMVersion);

	//***************************************************************************
	// @brief   설치된 JVM 유형 코드를 반환합니다.
	// @return  int (0: 미설치, 1: MS JVM, 2: Sun JVM, 3: 둘 다 설치)
	//***************************************************************************
	int		IsJVM() const {
		return m_nIsJVM;
	}

private:
	int		m_nIsJVM; // JVM 존재 및 유형 플래그
};

//***************************************************************************
// @class   CInstallSwInfo
// @brief   레지스트리를 탐색하여 설치된 소프트웨어 목록을 관리하는 클래스입니다.
//***************************************************************************
class CInstallSwInfo
{
public:
	CInstallSwInfo();
	~CInstallSwInfo();

	BOOL	GetInformation();

	//***************************************************************************
	// @brief   설치된 소프트웨어 정보 포인터 배열을 반환합니다.
	// @return  std::vector<INSTALL_SWINFO*>* 설치된 소프트웨어 정보 벡터 포인터
	//***************************************************************************
	std::vector<INSTALL_SWINFO*>* GetInstallSwInfoArray() {
		return &m_sInstallSwInfoArray;
	}

private:
	std::vector<INSTALL_SWINFO*> m_sInstallSwInfoArray; // 설치된 소프트웨어 정보 포인터 벡터
};

#endif // ndef __SOFTWAREINFO_H__