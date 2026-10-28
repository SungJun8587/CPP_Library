
//***************************************************************************
// ServiceConfig .h : interface for the CServiceConfig class.
//
//***************************************************************************

#ifndef UC_SERVICECONFIG_H
#define UC_SERVICECONFIG_H

#include <ServerConfig.h>
#include <Memory/Singleton.h>

//***************************************************************************
// @class CServiceConfig
// @brief 서버 설정 정보 관리 클래스
// @detail JSON 설정을 CJSONConfigFieldReader로 읽고 CJSONConfigFieldWriter로 저장합니다.
//         키가 없으면 기본값을 유지하고, 타입/범위 오류는 로그로 남기며 Init이 false를 돌려줍니다.
//***************************************************************************
class CServiceConfig : public CServerConfig, public CSingleton<CServiceConfig>
{
public:
	CServiceConfig() {};
	virtual ~CServiceConfig() {};

	bool Init(const TCHAR* tszServerInfo);
	bool Save(const TCHAR* tszServerInfo) const;
	void PrintServerSettingInfo();
};

#endif // ndef UC_SERVICECONFIG_H