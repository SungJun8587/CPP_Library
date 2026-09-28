
//***************************************************************************
// ServiceConfig .h : interface for the CServiceConfig class.
//
//***************************************************************************

#ifndef UC_SERVICECONFIG_H
#define UC_SERVICECONFIG_H

#include <ServerConfig.h>
#include <Memory/Singleton.h>

//***************************************************************************
// @brief 서버 설정 정보 관리 클래스
// @details JSON 기반의 설정을 로드하고 관리합니다.
//***************************************************************************
class CServiceConfig : public CServerConfig, public CSingleton<CServiceConfig>
{
public:
	CServiceConfig() {};
	virtual ~CServiceConfig() {};

	bool Init(const TCHAR* tszServerInfo);
	void PrintServerSettingInfo();

	//***************************************************************************
	// @brief 서버 설정 객체를 JSON 형태로 직렬화합니다(베이스 필드 +
	//        이 클래스 고유 필드 전부).
	//***************************************************************************
	void ToJSON(_tValue& value, _tDocument::AllocatorType& allocator) const
	{
		value.SetObject();
		value.AddMember(_T("ServiceName"), _tValue(_tszServiceName, allocator), allocator);
		value.AddMember(_T("DisplayName"), _tValue(_tszDisplayName, allocator), allocator);
		value.AddMember(_T("Name"), _tValue(_tszServerName, allocator), allocator);
		value.AddMember(_T("GroupId"), _nServerGroupId, allocator);
		value.AddMember(_T("ChannelId"), _nServerChannelId, allocator);
		value.AddMember(_T("IP"), _tValue(_tszIP, allocator), allocator);
		value.AddMember(_T("Port"), _nServerPort, allocator);
		value.AddMember(_T("KeepAliveSec"), _nKeepAliveSec, allocator);
		value.AddMember(_T("MaxSessionCount"), _nMaxSessionCount, allocator);
		value.AddMember(_T("WorkerThreadCnt"), _nWorkerThreadCnt, allocator);
	}

	//***************************************************************************
	// @brief JSON 객체로부터 서버 설정 정보를 역직렬화합니다.
	//***************************************************************************
	void FromJSON(const _tValue& value)
	{
		_tcsncpy_s(_tszServiceName, _countof(_tszServiceName), value[_T("ServiceName")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszDisplayName, _countof(_tszDisplayName), value[_T("DisplayName")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszServerName, _countof(_tszServerName), value[_T("Name")].GetString(), _TRUNCATE);
		_nServerGroupId = value[_T("GroupId")].GetInt();
		_nServerChannelId = value[_T("ChannelId")].GetInt();
		_tcsncpy_s(_tszIP, _countof(_tszIP), value[_T("IP")].GetString(), _TRUNCATE);
		_nServerPort = value[_T("Port")].GetInt();
		_nKeepAliveSec = value[_T("KeepAliveSec")].GetInt();
		_nMaxSessionCount = value[_T("MaxSessionCount")].GetInt();
		_nWorkerThreadCnt = value[_T("WorkerThreadCnt")].GetInt();
	}
};

#endif // ndef UC_SERVICECONFIG_H