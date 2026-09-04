
//***************************************************************************
// ServerConnectInfo.h: Implementation for Server Connection Information Management.
//
//***************************************************************************

#ifndef UC_SERVERCONNECTINFO_H
#define UC_SERVERCONNECTINFO_H

#include <Util/CommonUtil.h>
#include <JSON/RapidJSONUtil.h>

//***************************************************************************
// @brief 서버 접속 정보 노드 클래스
// @details 서버의 ID, 이름, IP, 포트 정보 관리 및 JSON 직렬화/역직렬화를 수행합니다.
//***************************************************************************
class CServerNode
{
public:
	//***************************************************************************
	// @brief 기본 생성자
	//***************************************************************************
	CServerNode(void)
		: _nID(0), _nPort(0)
	{
		memset(_tszServerName, 0, sizeof(_tszServerName));
		memset(_tszIP, 0, sizeof(_tszIP));
	}

	//***************************************************************************
	// @brief 서버 노드 정보를 초기화한다.
	// @param ptszServerName 서버 이름
	// @param nPort 서버 포트 번호
	// @param ptszIP 서버 IP 주소
	//***************************************************************************
	void Init(const TCHAR* ptszServerName, const unsigned int nPort, const TCHAR* ptszIP)
	{
		_tcsncpy_s(_tszServerName, _countof(_tszServerName), ptszServerName, _TRUNCATE);
		_tcsncpy_s(_tszIP, _countof(_tszIP), ptszIP, _TRUNCATE);
		_nPort = nPort;
	}

	//***************************************************************************
	// @brief 서버 노드 정보를 JSON 객체로 직렬화한다.
	// @param value 결과가 저장될 JSON Value 객체
	// @param allocator JSON 문서의 메모리 할당자
	//***************************************************************************
	void ToJSON(_tValue& value, _tDocument::AllocatorType& allocator) const
	{
		value.SetObject();
		value.AddMember(_T("ID"), _nID, allocator);
		value.AddMember(_T("Name"), _tValue(_tszServerName, allocator), allocator);
		value.AddMember(_T("IP"), _tValue(_tszIP, allocator), allocator);
		value.AddMember(_T("Port"), _nPort, allocator);
	}

	//***************************************************************************
	// @brief JSON 객체로부터 서버 노드 정보를 역직렬화한다.
	// @param value 파싱할 데이터가 담긴 JSON Value 객체
	//***************************************************************************
	void FromJSON(const _tValue& value)
	{
		_nID = value[_T("ID")].GetInt();
		_tcsncpy_s(_tszServerName, _countof(_tszServerName), value[_T("Name")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszIP, _countof(_tszIP), value[_T("IP")].GetString(), _TRUNCATE);
		_nPort = value[_T("Port")].GetInt();
	};

public:
	int16			_nID;                             // 서버 ID

	TCHAR			_tszServerName[HOSTNAME_STRLEN];  // 서버 이름
	TCHAR			_tszIP[HOSTNAME_STRLEN];          // 서버 IP 주소
	uint16			_nPort;                           // 서버 포트 번호
};

//***************************************************************************
// @brief 데이터베이스 접속 정보 노드 클래스
// @details DB 종류, 접속 정보, 계정 정보를 관리하며 DSN 생성 및 JSON 직렬화를 수행합니다.
//***************************************************************************
class CDBNode
{
public:
	//***************************************************************************
	// @brief 기본 생성자
	//***************************************************************************
	CDBNode(void)
		: _nID(0), _dbClass(EDBClass::NONE), _nPort(0)
	{
		memset(_tszDSN, 0, sizeof(_tszDSN));
		memset(_tszDSNDriver, 0, sizeof(_tszDSNDriver));
		memset(_tszDBHost, 0, sizeof(_tszDBHost));
		memset(_tszDBName, 0, sizeof(_tszDBName));
		memset(_tszDBUserId, 0, sizeof(_tszDBUserId));
		memset(_tszDBPasswd, 0, sizeof(_tszDBPasswd));
	}

	//***************************************************************************
	// @brief DB 노드 정보를 초기화하고 DSN 문자열을 생성한다.
	// @param dbClass DB 종류 (EDBClass)
	// @param ptszDSNDriver DSN 드라이버 이름
	// @param ptszDBHost DB 호스트 주소
	// @param nPort DB 포트 번호
	// @param ptszDBUserId DB 사용자 ID
	// @param ptszDBPasswd DB 비밀번호
	// @param ptszDBName DB 이름
	//***************************************************************************
	void Init(EDBClass dbClass, const TCHAR* ptszDSNDriver, const TCHAR* ptszDBHost, const unsigned int nPort, const TCHAR* ptszDBUserId, const TCHAR* ptszDBPasswd, const TCHAR* ptszDBName)
	{
		_dbClass = dbClass;
		_tcsncpy_s(_tszDSNDriver, _countof(_tszDSNDriver), ptszDSNDriver, _TRUNCATE);
		_tcsncpy_s(_tszDBHost, _countof(_tszDBHost), ptszDBHost, _TRUNCATE);
		_nPort = nPort;
		_tcsncpy_s(_tszDBName, _countof(_tszDBName), ptszDBName, _TRUNCATE);
		_tcsncpy_s(_tszDBUserId, _countof(_tszDBUserId), ptszDBUserId, _TRUNCATE);
		_tcsncpy_s(_tszDBPasswd, _countof(_tszDBPasswd), ptszDBPasswd, _TRUNCATE);

		GetDBDSNString(_tszDSN, _dbClass, _tszDSNDriver, _tszDBHost, _nPort, _tszDBUserId, _tszDBPasswd, _tszDBName);
	}

	//***************************************************************************
	// @brief DB 노드 정보를 JSON 객체로 직렬화한다.
	// @param value 결과가 저장될 JSON Value 객체
	// @param allocator JSON 문서의 메모리 할당자
	//***************************************************************************
	void ToJSON(_tValue& value, _tDocument::AllocatorType& allocator) const
	{
		value.SetObject();
		value.AddMember(_T("ID"), _nID, allocator);
		value.AddMember(_T("DBClass"), static_cast<int>(_dbClass), allocator);
		value.AddMember(_T("DSNDriver"), _tValue(_tszDSNDriver, allocator), allocator);
		value.AddMember(_T("IP"), _tValue(_tszDBHost, allocator), allocator);
		value.AddMember(_T("Port"), _nPort, allocator);
		value.AddMember(_T("DBName"), _tValue(_tszDBName, allocator), allocator);
		value.AddMember(_T("UID"), _tValue(_tszDBUserId, allocator), allocator);
		value.AddMember(_T("PWD"), _tValue(_tszDBPasswd, allocator), allocator);
	}

	//***************************************************************************
	// @brief JSON 객체로부터 DB 노드 정보를 역직렬화한다.
	// @param value 파싱할 데이터가 담긴 JSON Value 객체
	//***************************************************************************
	void FromJSON(const _tValue& value)
	{
		_nID = value[_T("ID")].GetInt();
		_dbClass = ConvertDBClassToInt(value[_T("DBClass")].GetInt());
		_tcsncpy_s(_tszDSNDriver, _countof(_tszDSNDriver), value[_T("DSNDriver")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszDBHost, _countof(_tszDBHost), value[_T("IP")].GetString(), _TRUNCATE);
		_nPort = value[_T("Port")].GetInt();
		_tcsncpy_s(_tszDBName, _countof(_tszDBName), value[_T("DBName")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszDBUserId, _countof(_tszDBUserId), value[_T("UID")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszDBPasswd, _countof(_tszDBPasswd), value[_T("PWD")].GetString(), _TRUNCATE);

		GetDBDSNString(_tszDSN, _dbClass, _tszDSNDriver, _tszDBHost, _nPort, _tszDBUserId, _tszDBPasswd, _tszDBName);
	};

	//***************************************************************************
	// @brief 정수형 값에 대응되는 EDBClass 열거형 값을 반환한다.
	// @param dbClass DB 종류 정수값
	// @return 변환된 EDBClass 열거형 값
	//***************************************************************************
	EDBClass ConvertDBClassToInt(int dbClass)
	{
		switch( dbClass )
		{
		case 1:
			return EDBClass::MSSQL;
			break;
		case 2:
			return EDBClass::MYSQL;
			break;
		case 3:
			return EDBClass::ORACLE;
			break;
		}

		return EDBClass::NONE;
	}

public:
	int16			_nID;                                             // DB 노드 ID

	EDBClass        _dbClass;											// DB 종류 (MSSQL, MYSQL, ORACLE 등)
	TCHAR			_tszDSN[DATABASE_DSN_STRLEN];						// 생성된 DSN 문자열
	TCHAR			_tszDSNDriver[DATABASE_DSN_DRIVER_STRLEN];			// DSN 드라이버 이름
	TCHAR			_tszDBHost[DATABASE_SERVER_NAME_STRLEN];			// DB 호스트 주소/IP
	uint16			_nPort;												// DB 포트 번호
	TCHAR			_tszDBName[DATABASE_NAME_STRLEN];					// 데이터베이스 이름
	TCHAR			_tszDBUserId[DATABASE_DSN_USER_ID_STRLEN];			// DB 사용자 계정 ID
	TCHAR			_tszDBPasswd[DATABASE_DSN_USER_PASSWORD_STRLEN];	// DB 계정 비밀번호
};

//***************************************************************************
// @brief Redis 접속 정보 노드 클래스
// @details Redis 서버의 접속 정보, 인증 계정 및 데이터베이스 인덱스를 관리하고 JSON 직렬화를 수행합니다.
//***************************************************************************
class CRedisNode
{
public:
	//***************************************************************************
	// @brief 기본 생성자
	//***************************************************************************
	CRedisNode(void)
		: _nID(0), _nPort(0), _nDbIndex(0)
	{
		memset(_tszDBHost, 0, sizeof(_tszDBHost));
		memset(_tszDBUserId, 0, sizeof(_tszDBUserId));
		memset(_tszDBPasswd, 0, sizeof(_tszDBPasswd));
	}

	//***************************************************************************
	// @brief Redis 노드 정보를 초기화한다.
	// @param ptszHost Redis 호스트 주소
	// @param nPort Redis 포트 번호
	// @param ptszUserId Redis 사용자 ID
	// @param ptszPasswd Redis 비밀번호
	// @param nDbIndex Redis DB 인덱스 번호
	//***************************************************************************
	void Init(const TCHAR* ptszHost, const unsigned int nPort, const TCHAR* ptszUserId, const TCHAR* ptszPasswd, const unsigned int nDbIndex)
	{
		_tcsncpy_s(_tszDBHost, _countof(_tszDBHost), ptszHost, _TRUNCATE);
		_nPort = nPort;
		_tcsncpy_s(_tszDBUserId, _countof(_tszDBUserId), ptszUserId, _TRUNCATE);
		_tcsncpy_s(_tszDBPasswd, _countof(_tszDBPasswd), ptszPasswd, _TRUNCATE);
		_nDbIndex = nDbIndex;
	}

	//***************************************************************************
	// @brief Redis 노드 정보를 JSON 객체로 직렬화한다.
	// @param value 결과가 저장될 JSON Value 객체
	// @param allocator JSON 문서의 메모리 할당자
	//***************************************************************************
	void ToJSON(_tValue& value, _tDocument::AllocatorType& allocator) const
	{
		value.SetObject();
		value.AddMember(_T("ID"), _nID, allocator);
		value.AddMember(_T("IP"), _tValue(_tszDBHost, allocator), allocator);
		value.AddMember(_T("Port"), _nPort, allocator);
		value.AddMember(_T("UID"), _tValue(_tszDBUserId, allocator), allocator);
		value.AddMember(_T("PWD"), _tValue(_tszDBPasswd, allocator), allocator);
		value.AddMember(_T("DBIndex"), _nDbIndex, allocator);
	}

	//***************************************************************************
	// @brief JSON 객체로부터 Redis 노드 정보를 역직렬화한다.
	// @param value 파싱할 데이터가 담긴 JSON Value 객체
	//***************************************************************************
	void FromJSON(const _tValue& value)
	{
		_nID = value[_T("ID")].GetInt();
		_tcsncpy_s(_tszDBHost, _countof(_tszDBHost), value[_T("IP")].GetString(), _TRUNCATE);
		_nPort = value[_T("Port")].GetInt();
		_tcsncpy_s(_tszDBUserId, _countof(_tszDBUserId), value[_T("UID")].GetString(), _TRUNCATE);
		_tcsncpy_s(_tszDBPasswd, _countof(_tszDBPasswd), value[_T("PWD")].GetString(), _TRUNCATE);
		_nDbIndex = value[_T("DBIndex")].GetInt();
	};

public:
	int16			_nID;                                             // Redis 노드 ID

	TCHAR			_tszDBHost[DATABASE_SERVER_NAME_STRLEN];          // Redis 호스트 주소
	uint16			_nPort;                                           // Redis 포트 번호
	TCHAR			_tszDBUserId[DATABASE_DSN_USER_ID_STRLEN];        // Redis 사용자 계정 ID
	TCHAR			_tszDBPasswd[DATABASE_DSN_USER_PASSWORD_STRLEN];  // Redis 계정 비밀번호
	int32			_nDbIndex;                                        // Redis DB 인덱스
};

#endif // ndef UC_SERVERCONNECTINFO_H