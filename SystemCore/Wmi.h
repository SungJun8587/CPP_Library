//***************************************************************************
// Wmi.h: interface for the CWmi class.
//
//***************************************************************************

#ifndef __WMI_H__
#define __WMI_H__

#ifndef __ENCODINGCONVERT_H__
#include <Util/EncodingConvert.h>
#endif

#include <WbemIdl.h>
#include <comdef.h>
#include <tchar.h>
#include <string>
#include <vector>

#pragma comment(lib, "wbemuuid.lib")

//***************************************************************************
// @struct WMI_CLASSPROPERTIES
// @brief WMI 클래스 속성 정보(이름 및 VARIANT 값)를 저장하는 구조체입니다.
//***************************************************************************
typedef struct _WMI_CLASSPROPERTIES
{
public:
	_WMI_CLASSPROPERTIES() {
		m_tszName[0] = _T('\0');
		VariantInit(&m_vtValue);
	}

	~_WMI_CLASSPROPERTIES() {
		VariantClear(&m_vtValue);
	}

	//***************************************************************************
	// @brief 속성 이름을 설정합니다. (Unicode -> TCHAR 변환 처리)
	// @param pwszName 설정할 유니코드 속성명 (WCHAR*)
	// @return 성공 시 TRUE, 실패 시 FALSE
	//***************************************************************************
	BOOL SetName(WCHAR* pwszName)
	{
		if( !pwszName ) return FALSE;

#ifdef _UNICODE
		_tcscpy_s(m_tszName, _countof(m_tszName), pwszName);
#else
		std::string strAnsi = UnicodeToAnsi(pwszName);
		strcpy_s(m_tszName, _countof(m_tszName), strAnsi.c_str());
#endif

		return TRUE;
	}

	TCHAR		m_tszName[32];	// 속성명 버퍼
	VARIANT		m_vtValue;		// 속성 값

} WMI_CLASSPROPERTIES, * PWMI_CLASSPROPERTIES;

//***************************************************************************
// @class CWmi
// @brief WMI 서비스 연결 및 WQL 쿼리 실행/결과 조회를 담당하는 클래스입니다.
// @note  COM 라이브러리 초기화(CoInitializeEx)/종료(CoUninitialize)는
//        호출자(예: main)가 책임집니다. CWmi 인스턴스를 생성/사용/소멸하는
//        전체 구간 동안 호출 스레드에서 COM이 초기화된 상태여야 합니다.
//***************************************************************************
class CWmi
{
public:
	CWmi();
	~CWmi();

	//***************************************************************************
	// @brief WMI 서비스에 연결합니다.
	// @param ptszHost 접속할 호스트명 또는 IP 주소 (NULL일 경우 로컬 컴퓨터)
	// @param ptszUserName 접속할 사용자 계정명 (NULL일 경우 현재 사용자)
	// @param ptszUserPass 접속할 사용자 비밀번호 (NULL일 경우 현재 사용자)
	// @return 성공 시 TRUE, 실패 시 FALSE
	//***************************************************************************
	BOOL	Connect(TCHAR* ptszHost = NULL, TCHAR* ptszUserName = NULL, TCHAR* ptszUserPass = NULL);

	//***************************************************************************
	// @brief WQL 쿼리를 실행하여 WMI 오브젝트 목록을 수집합니다.
	// @param ptszQuery 조회할 WMI 클래스명 또는 WQL 조건절 대상
	// @return 수집된 결과 개수 (실패 시 -1)
	//***************************************************************************
	int		ExecQuery(const TCHAR* ptszQuery);

	//***************************************************************************
	// @brief 특정 인덱스의 오브젝트에서 속성 값을 VARIANT 형태로 가져옵니다.
	// @param nIndex 오브젝트 인덱스 (0-based)
	// @param ptszProperty 가져올 속성명
	// @param vtVal 결과를 전달받을 VARIANT 참조
	// @return 성공 시 TRUE, 실패 시 FALSE
	//***************************************************************************
	BOOL	GetProperties(int nIndex, const TCHAR* ptszProperty, VARIANT& vtVal);

	//***************************************************************************
	// @brief 특정 인덱스의 오브젝트에서 문자열 속성 값을 가져옵니다.
	// @param nIndex 오브젝트 인덱스 (0-based)
	// @param ptszProperty 가져올 속성명
	// @param ptszValue 결과 문자열을 저장할 버퍼
	// @param dwSize 버퍼 크기 (문자 단위)
	// @return 성공 시 TRUE, 실패 시 FALSE
	//***************************************************************************
	BOOL	GetProperties(int nIndex, const TCHAR* ptszProperty, TCHAR* ptszValue, DWORD dwSize);

	//***************************************************************************
	// @brief 특정 인덱스의 오브젝트에서 정수(long) 속성 값을 가져옵니다.
	// @param nIndex 오브젝트 인덱스 (0-based)
	// @param ptszProperty 가져올 속성명
	// @param plValue 결과 값을 저장할 long 포인터
	// @return 성공 시 TRUE, 실패 시 FALSE
	//***************************************************************************
	BOOL	GetProperties(int nIndex, const TCHAR* ptszProperty, long* plValue);

private:
	CComPtr<IWbemLocator> m_pIWbemLocator;						// WMI Locator 인터페이스 포인터
	CComPtr<IWbemServices> m_pIWbemServices;					// WMI Services 인터페이스 포인터

	std::vector<CComPtr<IWbemClassObject>> m_vecClassObject;	// 조회된 WMI 오브젝트 컨테이너
};

#endif // ndef __WMI_H__