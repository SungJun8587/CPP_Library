# CBaseODBC 클래스 설명서

## 1. 개념

`CBaseODBC`는 Microsoft ODBC API(`sql.h`, `sqlext.h`)를 감싸는 저수준 래퍼(wrapper) 클래스로, 하나의 ODBC 연결(HDBC)과 하나의 Statement 핸들(HSTMT)을 소유하며 다음을 캡슐화한다.

- 환경/연결/구문 핸들(`SQLHENV`, `SQLHDBC`, `SQLHSTMT`)의 생성·해제
- 파라미터 바인딩(`SQLBindParameter`), 컬럼 바인딩(`SQLBindCol`), 직접 조회(`SQLGetData`)
- 쿼리 준비/실행(`SQLPrepare`, `SQLExecute`, `SQLExecDirect`)
- 트랜잭션 제어(`SQLEndTran`) 및 대량 처리(`SQLBulkOperations`)
- C++ 타입 ↔ ODBC C/SQL 타입 자동 매핑(템플릿 기반)

`COdbcConnPool`(또는 유사한 커넥션 풀)이 실제로 대여/반납하는 객체가 바로 이 `CBaseODBC` 인스턴스이며, 풀은 연결의 생명주기(재연결·격리 등)를 관리하고 `CBaseODBC`는 그 연결 위에서 SQL 실행의 실무를 담당하는 구조로 이해할 수 있다.

부속 파일 구성은 다음과 같다.

| 파일 | 역할 |
|---|---|
| `BaseODBC.h` | 클래스 선언, `COL_DESCRIPTION` 구조체 |
| `BaseODBC.cpp` | 비템플릿 멤버 함수 구현 |
| `BaseODBC.inl` | 템플릿 멤버 함수(`BindParamInput/Output`, `BindCol`, `GetData`) 구현 |
| `DB_ParamAttr.inl` | 입력/출력 파라미터의 C++↔ODBC 타입 매핑 (`odbc_param_attr`, `CDBParamAttr`, `CDBParamAttrMgr`) |
| `DB_ColAttr.inl` | 결과 컬럼의 C++↔ODBC 타입 매핑 (`odbc_col_attr`, `CDBColAttr`, `CDBColAttrMgr`) |
| `DB_Error.inl` | `SQLGetDiagRec` 기반 에러 메시지 조회 (`CDBError`) |

---

## 2. 특징

- **타입 안전 바인딩**: `template<typename _TMain>` 오버로드와 타입 특수화 매크로(`ODBC_PARAM_ATTR`, `ODBC_COL_ATTR`)를 이용해, 호출자가 ODBC C/SQL 타입 상수를 직접 지정하지 않아도 C++ 타입으로부터 자동으로 올바른 타입이 선택된다.
- **순번 자동 증가 vs 인덱스 명시 오버로드 이원화**: 대부분의 바인딩 함수는 "인자 없이 호출하면 내부 카운터(`m_nParamNum`/`m_nColNum`)가 자동 증가"하는 버전과, "인덱스를 직접 지정"하는 버전 두 가지가 공존한다.
- **유니코드/멀티바이트 분기**: `_UNICODE` 매크로에 따라 `SQLWCHAR`/`SQLCHAR` 경로를 컴파일 타임에 분기 처리(`#ifdef _UNICODE`).
- **가변 길이 문자열/바이너리 자동 타입 승격**: 문자열이 `DATABASE_VARCHAR_MAX`(또는 `DATABASE_WVARCHAR_MAX`)를 넘으면 `SQL_VARCHAR`→`SQL_LONGVARCHAR`(또는 W버전)로, 바이너리가 `DATABASE_BINARY_MAX`를 넘으면 `SQL_BINARY`→`SQL_LONGVARBINARY`로 자동 승격한다.
- **DBMS 종류별 분기 로직 내장**: `GetServerCharacterSet()`처럼 `m_DbClass`(MSSQL/MySQL/Oracle)에 따라 다른 조회 쿼리를 사용하는 함수가 존재한다.
- **에러 로깅 일원화**: 실패 시 대부분의 함수가 `CDBError`로 `SQLGetDiagRec` 메시지를 얻어 `LOG_ERROR`로 통일된 형식(`함수명, QueryInfo[...], ErrorMsg : ...`)으로 남긴다.
- **엑셀 파일 모드(`m_bLoadExcelFile`)**: 이 플래그가 켜져 있으면 쿼리 타임아웃을 설정하지 않고, `Fetch()` 실패 시에도 에러 로그를 남기지 않는다(엑셀 ODBC 드라이버 특유의 정상적인 조회 실패를 억제하기 위함으로 추정).
- **RAII 성격의 소멸자**: 소멸자에서 `Disconnect()`를 호출해 핸들 누수를 방지한다.

---

## 3. 주요 타입

### 3.1 `COL_DESCRIPTION`

`DescribeCol()` 호출 결과를 담는 구조체 (`BaseODBC.h`).

| 필드 | 타입 | 설명 |
|---|---|---|
| `tszColName[128]` | `TCHAR[]` | 컬럼명 |
| `NameLength` | `short` | 컬럼명 길이 |
| `EDataType` | `short` | SQL 데이터 타입 |
| `dwColSize` | `DWORD` | 컬럼 크기(Precision) |
| `DigitSize` | `short` | 소수 자릿수 |
| `Nullable` | `short` | NULL 허용 여부 |
| `DispLength` | `long` | 화면 표시 길이(`SQL_DESC_DISPLAY_SIZE`) |

### 3.2 `EDBClass`

`BaseODBC.h`에는 선언이 직접 보이지 않으나(외부 헤더에서 정의된 것으로 추정), `NONE`, `MSSQL`, `MYSQL`, `ORACLE` 등의 값이 `GetServerCharacterSet()`에서 사용된다. 연결 대상 DBMS 종류를 구분하는 용도이다.

---

## 4. 멤버 변수

| 변수 | 타입 | 설명 |
|---|---|---|
| `m_hEnv` | `SQLHENV` | ODBC 환경 핸들 |
| `m_hConn` | `SQLHDBC` | 데이터베이스 연결 핸들 |
| `m_hStmt` | `SQLHSTMT` | SQL 구문(statement) 핸들 |
| `m_DbClass` | `EDBClass` | 연결 대상 DBMS 종류 |
| `m_bLoadExcelFile` | `bool` | 엑셀 파일 로드 모드 여부 (타임아웃 미설정, Fetch 실패 로그 억제) |
| `m_nParamNum` | `int16` | 자동 증가 파라미터 바인딩 순번 카운터 |
| `m_nColNum` | `int16` | 자동 증가 컬럼 바인딩 순번 카운터 |
| `m_nFetchedRows[1]` | `SQLINTEGER` | `SQL_ATTR_ROWS_FETCHED_PTR`에 바인딩되어 실제 페치된 행 수를 저장 |
| `m_DBParamAttrMgr` | `CDBParamAttrMgr` | 파라미터 타입→ODBC 속성 변환기 |
| `m_DBColAttrMgr` | `CDBColAttrMgr` | 컬럼 타입→ODBC 속성 변환기 |
| `m_tszDSN[DATABASE_DSN_STRLEN]` | `TCHAR[]` | 연결 DSN 문자열 |
| `m_tszQueryInfo[SQL_MAX_MESSAGE_LENGTH]` | `TCHAR[]` | 현재 준비/실행 중인 쿼리 텍스트(에러 로깅용) |

> 이전 리비전에 있던 `m_tszLastError[DATABASE_ERRORMSG_STRLEN]` 필드는 제거되었다. `.cpp`에서 값을 채우는 코드가 없어 실질적으로 미사용 상태였던 필드였는데, 헤더 정리 과정에서 함께 삭제된 것으로 보인다.

---

## 5. 멤버 함수

### 5.1 생성/소멸

| 함수 | 설명 |
|---|---|
| `CBaseODBC(EDBClass dbClass = NONE, bool bLoadExcelFile = false)` | DSN 없이 생성. 각 버퍼를 0으로 초기화 |
| `CBaseODBC(EDBClass dbClass, const TCHAR* ptszDSN, bool bLoadExcelFile = false)` | DSN을 즉시 복사(`_tcsncpy_s`, `_TRUNCATE`)하며 생성 |
| `~CBaseODBC()` | `Disconnect()` 호출 |

### 5.2 연결 관리

| 함수 | 설명 |
|---|---|
| `Connect(lLoginTimeOut, lConnectionTimeOut)` | 환경 핸들 생성 → ODBC 3.0 버전 설정 → 연결 핸들 생성 → 로그인/연결 타임아웃 설정 → `SQLDriverConnect`(`SQL_DRIVER_NOPROMPT`) → 드라이버 버전 확인 → 서버명/DBMS명/버전/캐릭터셋 조회까지 순서대로 진행하며, 중간에 하나라도 실패하면 `throw 0` 후 `catch(...)`에서 `Disconnect()`를 호출하고 `false` 반환 |
| `Disconnect()` | Stmt 핸들 해제 → `SQLDisconnect` → Conn 핸들 해제 → Env 핸들 해제. 항상 `true` 반환(내부 에러는 로그만 남김) |
| `IsConnected()` | `SQL_ATTR_CONNECTION_DEAD` 속성을 조회해 연결의 생존 여부 확인 |
| `IsConnectionValid()` | (헤더 인라인) `m_hEnv`, `m_hConn`, `m_hStmt`가 모두 유효한 핸들인지만 확인 — 실제 서버 통신 없이 로컬 상태만 검사 |
| `GetDBClass()` | `m_DbClass` 반환 |
| `GetServerName / GetDBMSName / GetDBMSVersion / GetServerCharacterSet` | `SQLGetInfo` 또는 (캐릭터셋의 경우) DBMS별 쿼리 실행으로 서버 정보 조회 |

### 5.2.1 DBMS별 연결 문자열 형식

`Connect()`는 `SQLDriverConnect`를 `SQL_DRIVER_NOPROMPT` 옵션으로 호출하므로, 별도 다이얼로그 없이 바로 접속이 성사되려면 `m_tszDSN`에 담기는 연결 문자열이 DBMS 드라이버가 요구하는 키-값 쌍을 빠짐없이 갖추고 있어야 한다. `m_DbClass`로 구분되는 세 DBMS는 드라이버 이름과 필수 키가 서로 달라 아래와 같이 형식이 갈린다.

| DBMS (`EDBClass`) | 드라이버 키 | 필수 키 | 예시 |
|---|---|---|---|
| `MSSQL` | `DRIVER={SQL Server}` 또는 `{ODBC Driver 17/18 for SQL Server}` | `SERVER`, `DATABASE`, `UID`, `PWD` | `DRIVER={ODBC Driver 17 for SQL Server};SERVER=192.168.0.10,1433;DATABASE=GameDB;UID=sa;PWD=****;` |
| `MYSQL` | `DRIVER={MySQL ODBC 8.0 Unicode Driver}` | `SERVER`, `DATABASE`, `USER`, `PASSWORD`, `PORT` | `DRIVER={MySQL ODBC 8.0 Unicode Driver};SERVER=192.168.0.20;PORT=3306;DATABASE=game;USER=root;PASSWORD=****;OPTION=3;` |
| `ORACLE` | `DRIVER={Oracle in OraClient19Home1}` (또는 Instant Client 드라이버명) | `DBQ`, `UID`, `PWD` | `DRIVER={Oracle in instantclient_19_18};DBQ=192.168.0.30:1521/ORCLPDB;UID=game_user;PWD=****;` |

- **DSN 방식**도 병행 가능하다. 시스템/사용자 DSN을 미리 등록해 두면 `DSN=MyDSN;UID=...;PWD=...;` 형태로 축약할 수 있으며, 이 경우 서버 주소·포트 등의 나머지 정보는 DSN 설정에 위임된다. `CBaseODBC(EDBClass, const TCHAR* ptszDSN, ...)` 생성자는 이 문자열을 그대로 `m_tszDSN`에 복사해 두었다가 `Connect()`에서 사용한다.
- **MySQL의 `OPTION` 플래그**는 비트마스크로 동작을 조합하며, `OPTION=3`(`FLAG_FOUND_ROWS`(1) + `FLAG_MULTI_STATEMENTS`(2) 등 드라이버 버전에 따라 의미가 다름)처럼 필요한 값만 조합해 사용한다.
- **Oracle의 `DBQ`**는 TNS 별칭(`DBQ=ORCLPDB`, `tnsnames.ora`에 등록된 이름) 또는 Easy Connect 문자열(`host:port/service_name`) 두 방식 모두 지원되며, TNS 파일 관리를 피하고 싶을 때는 Easy Connect 방식이 더 간단하다.
- 세 DBMS 모두 비밀번호에 `;`, `{`, `}` 문자가 포함되면 드라이버가 값 경계를 오인식할 수 있으므로, 필요시 `PWD={...}`처럼 중괄호로 감싸 이스케이프해야 한다.
- `GetServerCharacterSet()`이 `m_DbClass`에 따라 다른 조회 쿼리를 쓰는 것과 마찬가지로, 연결 문자열 조립 로직 역시 `m_DbClass` 분기가 필요하다는 점에서 두 기능은 같은 축으로 묶여 있다.

### 5.3 Statement 핸들 관리

| 함수 | 설명 |
|---|---|
| `InitStmtHandle(lQueryTimeOut)` | `SQLAllocHandle(SQL_HANDLE_STMT)` → `SQL_ATTR_CONCURRENCY = SQL_CONCUR_READ_ONLY` 설정 → (엑셀 모드가 아니면) 쿼리 타임아웃 설정 |
| `FreeStmt(Option)` | `SQLFreeStmt` 래퍼. `Option`은 `SQL_CLOSE`(커서 닫기) / `SQL_UNBIND`(컬럼 바인딩 해제) / `SQL_RESET_PARAMS`(파라미터 바인딩 초기화) / `SQL_DROP`(핸들 완전 제거) |
| `ClearStmt()` | `SQL_RESET_PARAMS` + `SQL_UNBIND` + `SQL_CLOSE`를 모두 수행하고 `m_tszQueryInfo`, `m_nParamNum`, `m_nColNum`을 초기화 |
| `ResetParamStmt()` | 파라미터 바인딩만 초기화(`SQL_RESET_PARAMS`) + `m_nParamNum = 0` |
| `UnBindColStmt()` | 컬럼 바인딩만 해제(`SQL_UNBIND`) + `m_nColNum = 0` |

### 5.4 파라미터 바인딩 (입력/출력)

| 함수 | 설명 |
|---|---|
| `BindParameter(ipar, fParamType, fCType, fSqlType, cbColDef, ibScale, rgbValue, cbValueMax, pcbValue)` | `SQLBindParameter` 원형 그대로 노출하는 저수준 래퍼. 모든 타입/크기를 호출자가 직접 지정 |
| `template<_TMain> BindParamInput(_TMain& tValue)` | 순번 자동 증가(`++m_nParamNum`). `CDBParamAttrMgr`가 타입에 맞는 C/SQL 타입·버퍼·길이를 계산해 바인딩 |
| `BindParamInput(const TCHAR* ptszValue, SQLLEN* plDataLength = nullptr)` | 문자열 전용 오버로드, 순번 자동 증가 |
| `template<_TMain> BindParamInput(int32 iParamIndex, _TMain& tValue)` | 인덱스 지정판 |
| `BindParamInput(int32 iParamIndex, const TCHAR* ptszValue, SQLLEN& lDataLength)` | 인덱스 지정 + 문자열 길이에 따라 `SQL_VARCHAR`/`SQL_WVARCHAR` ↔ `SQL_LONGVARCHAR`/`SQL_WLONGVARCHAR` 자동 선택 |
| `BindParamInput(int32 iParamIndex, const BYTE* pbData, int32 nBufferLength, SQLLEN& lDataLength)` | 바이너리 입력. `pbData == nullptr`이면 `SQL_NULL_DATA` 처리, 크기에 따라 `SQL_BINARY`/`SQL_LONGVARBINARY` 자동 선택 |
| `template<_TMain> BindParamOutput(_TMain& tValue)` | 출력 파라미터, 순번 자동 증가 |
| `BindParamOutput(TCHAR* ptszValue, int32& nBufferLength)` | 문자열 출력, 순번 자동 증가 |
| `template<_TMain> BindParamOutput(int32 iParamIndex, _TMain& tValue)` | 인덱스 지정판 |
| `BindParamOutput(int32 iParamIndex, TCHAR* ptszValue, int32& nBufferLength, SQLLEN& lDataLength)` | 인덱스 지정 문자열 출력 |
| `BindParamOutput(int32 iParamIndex, BYTE* pbData, int32 nBufferLength, SQLLEN& lDataLength)` | 인덱스 지정 바이너리 출력 |

### 5.5 컬럼 바인딩 / 데이터 조회 (결과 셋)

| 함수 | 설명 |
|---|---|
| `BindCol(ColumnNumber, TargetType, TargetValue, BufferLength, plDataLength)` | `SQLBindCol` 원형 래퍼 |
| `template<_TMain> BindCol(_TMain& tValue)` | 순번 자동 증가, `CDBColAttrMgr`로 타입 매핑 |
| `BindCol(TCHAR* ptszValue, int32& nBufferLength, SQLLEN* plDataLength = nullptr)` | 문자열 전용, 순번 자동 증가 |
| `template<_TMain> BindCol(int32 iColIndex, _TMain& tValue, SQLLEN& lDataLength)` | 인덱스 지정판 |
| `BindCol(int32 iColIndex, TCHAR* ptszValue, int32& nBufferLength, SQLLEN& lDataLength)` | 인덱스 지정 문자열 |
| `BindCol(int32 iColIndex, SQLSMALLINT targetType, int64& tValue, SQLLEN& lDataLength)` | int64 전용 (부호 있는 64비트 정수 명시적 처리) |
| `BindCol(int32 iColIndex, SQLSMALLINT targetType, uint64& tValue, SQLLEN& lDataLength)` | uint64 전용 |
| `GetData(ColumnNumber, TargetType, TargetValue, BufferLength, plDataLength)` | `SQLGetData` 원형 래퍼(바인딩 없이 즉시 조회) |
| `template<_TMain> GetData(int32 iColNum, _TMain& tValue)` | 타입 매핑 후 즉시 조회. `lDataLength`가 `SQL_NO_TOTAL`/`SQL_NULL_DATA`면 `false` |
| `GetData(int32 iColNum, TCHAR* ptszData, int32& nBufferLength)` | 문자열 조회, 유니코드/멀티바이트 분기 |

### 5.6 쿼리 준비/실행

| 함수 | 설명 |
|---|---|
| `PrepareQuery(ptszQueryInfo)` | Stmt 핸들이 없으면 생성 → `ClearStmt()`로 초기화 → 쿼리 텍스트 저장 → `SQLPrepare` |
| `Execute()` | Stmt 핸들이 없으면 생성 → `SQLExecute`. `SQL_NO_DATA`도 성공으로 간주 |
| `ExecDirect(ptszQueryInfo)` | Stmt 핸들이 없으면 생성 → `SQLExecDirect`(준비 없이 즉시 실행). `SQL_NO_DATA`도 성공으로 간주 |
| `BulkOperations(operation)` | `SQLBulkOperations` 래퍼. `SQL_ADD`(삽입), `SQL_UPDATE_BY_BOOKMARK`, `SQL_DELETE_BY_BOOKMARK`, `SQL_FETCH_BY_BOOKMARK` 등의 대량 작업 수행. 일반적인 사용 흐름은 "AutoCommit Off → `AllSets()`로 배열 바인딩 설정 → `ExecDirect` → `BindCol` → `BulkOperations` → `Commit`" |
| `SetStmtAttr(fAttribute, rgbValue, cbValueMax)` | 임의의 `SQLSetStmtAttr` 호출을 감싼 범용 함수 |
| `AllSets(nQueryResultRecordSize, nMaxRowSize)` | `SQL_ATTR_ROW_BIND_TYPE`(바인딩 방향), `SQL_ATTR_ROW_ARRAY_SIZE`(한 번에 처리할 행 수), `SQL_ATTR_ROWS_FETCHED_PTR`(`m_nFetchedRows`)를 설정해 배열/행 단위 대량 바인딩을 준비 |

### 5.7 결과 페치

| 함수 | 설명 |
|---|---|
| `Fetch()` | `SQLFetch` 래퍼. `SQL_NO_DATA`는 조용히 `false`, 그 외 실패는 (엑셀 모드가 아닐 때만) 에러 로그 후 `false` |
| `GetFetch()` | `::SQLFetch`의 `SQLRETURN` 원본 값을 그대로 반환(성공/실패 판단 없이 호출자가 직접 처리하고 싶을 때 사용) |
| `MoreResults()` | `SQLMoreResults` 래퍼 — 다중 결과 셋(예: 저장 프로시저)의 다음 결과 셋으로 이동 |
| `GetFetchedRows()` | (헤더 인라인) `m_nFetchedRows[0]` 반환 — `AllSets()`로 배열 바인딩을 설정했을 때 실제 페치된 행 수 |

### 5.8 트랜잭션

| 함수 | 설명 |
|---|---|
| `SetAutoCommitMode(valuePtr)` | `SQL_ATTR_AUTOCOMMIT` 설정. `SQL_AUTOCOMMIT_ON`(기본, 각 문장마다 자동 커밋) / `SQL_AUTOCOMMIT_OFF`(명시적 커밋/롤백 필요) |
| `Commit()` | `SQLEndTran(SQL_COMMIT)` |
| `Rollback()` | `SQLEndTran(SQL_ROLLBACK)` |

### 5.9 메타 정보

| 함수 | 설명 |
|---|---|
| `GetNumCols()` | `SQLNumResultCols` — 결과 셋의 컬럼 수 |
| `RowCount()` | `SQLRowCount` — 영향받은/조회된 행 수. 실패 시 `-1` |
| `RowNumber()` | `SQL_ATTR_ROW_NUMBER` 조회 — 현재 커서 위치의 행 번호 |
| `DescribeCol(iColNum, ColDescription)` | `SQLDescribeCol` + `SQLColAttribute(SQL_DESC_DISPLAY_SIZE)`로 `COL_DESCRIPTION` 채움 |

---

## 6. 타입 매핑 헬퍼

### 6.1 `CDBParamAttr` / `CDBParamAttrMgr` (`DB_ParamAttr.inl`)

- `odbc_param_attr_base<CDATA_TYPE, SQL_DATA_TYPE, COLUMN_SIZE>` : C++ 타입 하나에 대응하는 (C타입, SQL타입, 컬럼크기) 세 값을 컴파일 타임 상수로 보관하는 템플릿.
- `ODBC_PARAM_ATTR(d_type, c_type, sql_type, p_size)` 매크로로 아래 타입들이 특수화되어 있다.

| C++ 타입 | C 데이터 타입 | SQL 데이터 타입 | 컬럼 크기 |
|---|---|---|---|
| `bool` | `SQL_C_BIT` | `SQL_BIT` | 2 |
| `INT8` | `SQL_C_STINYINT` | `SQL_TINYINT` | 3 |
| `INT16` | `SQL_C_SSHORT` | `SQL_SMALLINT` | 5 |
| `INT32` | `SQL_C_SLONG` | `SQL_INTEGER` | 10 |
| `INT64` | `SQL_C_SBIGINT` | `SQL_BIGINT` | 19 |
| `UINT8/16/32/64` | 대응하는 `SQL_C_U*` | 대응하는 `SQL_*` | 3/5/10/19 |
| `FLOAT` | `SQL_C_FLOAT` | `SQL_REAL` | 7 |
| `DOUBLE` | `SQL_C_DOUBLE` | `SQL_DOUBLE` | 15 |
| `CHAR*` | `SQL_C_CHAR` | `SQL_VARCHAR` | 254 |
| `WCHAR*` | `SQL_C_WCHAR` | `SQL_WVARCHAR` | 254 |
| `SQL_TIMESTAMP_STRUCT` | `SQL_C_TYPE_TIMESTAMP` | `SQL_TYPE_TIMESTAMP` | 23 |

- `CDBParamAttrMgr::operator()`는 인자 타입에 따라 오버로드 선택:
  - 일반 POD 타입(`template<EDataType>`): `sizeof(EDataType)`을 버퍼 길이/데이터 길이로 사용.
  - `CHAR*`, `WCHAR*`(길이 미지정): `strlen`/`wcslen`으로 실제 길이를 계산.
  - `CHAR*`/`WCHAR*` + `nBufferLength` 참조: 버퍼 최대 크기를 외부에서 지정(출력 파라미터용).
- 결과물은 `CDBParamAttr` 하나에 담겨 `SQLBindParameter` 인자로 그대로 전달된다.

### 6.2 `CDBColAttr` / `CDBColAttrMgr` (`DB_ColAttr.inl`)

- 파라미터 매핑과 유사하지만 결과 컬럼용이라 SQL 타입/컬럼 크기 없이 **타겟 C 타입만** 매핑한다(`odbc_col_attr_base<TARGET_TYPE>`).
- 매핑 목록은 파라미터 쪽과 거의 동일하되, `CHAR*`/`WCHAR*`는 항상 `nBufferLength` 참조를 받는 오버로드만 존재(컬럼 바인딩은 항상 버퍼 크기를 사전에 알아야 하므로).

### 6.3 `CDBError` (`DB_Error.inl`)

- `operator()(nHandleType, hStatement, ptszMessage, ptszSQLState = NULL)` 하나만 제공.
- 내부적으로 `SQLGetDiagRecW`/`SQLGetDiagRec`(유니코드 분기)를 호출해 **첫 번째 진단 레코드**(레코드 번호 `1`)만 조회한다. 여러 개의 에러/경고가 쌓인 경우 두 번째 이후 레코드는 무시된다.
- 최종 메시지를 `"STATE[%s], ERROR[%ld], MSG[%s]"` 형식으로 포맷.

---

## 7. 동작 흐름

### 7.1 연결 ~ 단일 쿼리 조회

```
CBaseODBC odbc(EDBClass::MSSQL, dsn);
odbc.Connect(loginTimeout, connTimeout);   // Env→Conn→Driver Connect→서버정보 조회
odbc.InitStmtHandle(queryTimeout);         // Stmt 핸들 생성 + 옵션 설정
odbc.PrepareQuery(sql);                    // ClearStmt() → SQLPrepare
odbc.BindParamInput(param1);               // 순번 자동 증가 바인딩(반복)
odbc.Execute();                            // SQLExecute
while (odbc.Fetch()) {                     // SQLFetch
    odbc.GetData(1, value);                // 또는 사전에 BindCol()
}
odbc.ClearStmt();                          // 재사용을 위한 초기화
```

### 7.2 대량 처리(BulkOperations) 흐름

```
odbc.SetAutoCommitMode((SQLPOINTER)SQL_AUTOCOMMIT_OFF);
odbc.AllSets(rowBindType, maxRowSize);     // 배열 바인딩 + ROWS_FETCHED_PTR 설정
odbc.ExecDirect(sql);
odbc.BindCol(...);                        // 배열 버퍼 바인딩
odbc.BulkOperations(SQL_ADD);              // 혹은 북마크 기반 UPDATE/DELETE/FETCH
odbc.Commit();
```

### 7.3 종료

```
// 소멸자에서 자동으로 Disconnect() 호출 → Stmt/Conn/Env 핸들 순서대로 해제
```

---

## 8. 장단점

### 장점

- ODBC C API의 방대한 인자를 감춰 C++ 타입 기반의 간결한 바인딩 코드 작성이 가능하다.
- 순번 자동 증가 오버로드 덕분에 파라미터/컬럼 개수 관리(`ipar`, `ColumnNumber` 수동 증가)를 잊어버려서 생기는 실수를 줄인다.
- 에러 발생 시 쿼리 텍스트와 함께 로그를 남겨 디버깅 정보가 비교적 풍부하다.
- 문자열/바이너리 크기에 따른 타입 자동 승격(`VARCHAR`↔`LONGVARCHAR` 등)으로 대용량 데이터도 별도 분기 없이 처리 가능하다.

### 단점 / 유의할 점

- **스레드 안전성 없음**: `m_hStmt` 하나, 카운터(`m_nParamNum`, `m_nColNum`) 하나를 인스턴스가 단독으로 갖고 있어 여러 스레드가 동시에 같은 인스턴스를 사용할 수 없다(그래서 커넥션 풀에서 인스턴스 단위로 대여/반납하는 구조가 필요하다).
- **자동 증가 카운터의 상태 의존성**: `BindParamInput(tValue)`류 함수는 호출 순서에 결과가 좌우된다. 중간에 `ResetParamStmt()`를 호출하지 않고 재사용하면 순번이 꼬일 수 있다.
- **`SQL_DEFAULT_PARAM`/`SQL_LEN_DATA_AT_EXEC` 등 데이터 지연 전송(Data-At-Execution) 미지원**: 헤더 주석에 언급만 되어 있고 관련 별도 처리 로직은 보이지 않는다.
- **`CDBError`가 첫 번째 진단 레코드만 조회**: 다중 에러/경고 상황에서 일부 정보가 로그에서 누락될 수 있다.
- **`Disconnect()`가 실패해도 항상 `true` 반환**: `SQLDisconnect`/`SQLFreeHandle` 실패는 로그만 남기고 반환값에는 반영되지 않는다.

---

## 9. 사용법 예시

### 9.1 단독 사용 — 연결부터 종료까지 전체 흐름

풀 없이 `CBaseODBC`를 직접 생성해서 쓰는 가장 기본적인 형태다.

```cpp
// 1) 연결 생성 및 접속
CBaseODBC odbc(EDBClass::MSSQL, _T("MyDSN"));
if (!odbc.Connect())
{
    // 연결 실패 처리
    return;
}

// 2) Statement 핸들 초기화
odbc.InitStmtHandle();

// 3) 파라미터 바인딩 쿼리 준비 및 실행
odbc.PrepareQuery(_T("SELECT Name, Age FROM Users WHERE Id = ?"));

int32 userId = 1001;
odbc.BindParamInput(userId);          // 1번 파라미터, 순번 자동 증가

odbc.Execute();

// 4) 결과 컬럼 바인딩 후 페치
TCHAR tszName[128] = { 0, };
int32 nNameBufLen = _countof(tszName);
int32 nAge = 0;

odbc.BindCol(tszName, nNameBufLen);   // 1번 컬럼, 순번 자동 증가
odbc.BindCol(nAge);                   // 2번 컬럼, 순번 자동 증가

while (odbc.Fetch())
{
    // tszName, nAge 사용
}

// 5) 재사용을 위한 초기화
odbc.ClearStmt();

// 6) 종료 (또는 소멸자에서 자동 처리)
odbc.Disconnect();
```

### 9.2 커넥션 풀에서 빌려 쓰는 형태

실제 서비스 코드에서는 `CBaseODBC`를 직접 생성하지 않고 `COdbcConnPool`(또는 유사한
풀)에서 슬롯 단위로 빌려 쓰는 경우가 대부분이다. 이후 예시들은 이 패턴(가드 객체로
빌리고, 스코프를 벗어나면 자동 반납)을 기준으로 한다.

```cpp
OdbcConnGuard guard(&pool);
if( guard == nullptr ) return false;   // 대여 실패(풀 고갈 등)

if( !guard->PrepareQuery(_T("SELECT name, age FROM users WHERE id = ?")) )
    return false;

int32 userId = 42;
guard->BindParamInput(userId);         // odbc_param_attr<int32> 자동 적용

if( !guard->Execute() )
    return false;

TCHAR tszName[64] = { 0, };
int32 nNameBuf = sizeof(tszName);
int64 age = 0;
SQLLEN lRetSize = 0;

guard->BindCol(1, tszName, nNameBuf, lRetSize);
guard->BindCol(2, SQL_C_SBIGINT, age, lRetSize);

while( guard->Fetch() )
{
    // tszName, age 사용
}
```

- 인덱스를 생략하는 자동 증가 오버로드(`BindParamInput(tValue)`, `BindCol(tValue)`)를
  쓰면 `++m_nParamNum`/`++m_nColNum`이 알아서 순서를 매겨주므로, 바인딩 순서를
  SQL의 `?`/컬럼 순서와만 맞추면 된다.
- 컬럼 구성이 매번 달라지거나 일부만 조건부로 읽을 때는, 사전 바인딩 없이
  `Fetch()` 이후 그때그때 읽는 `GetData`가 더 간단하다(다만 컬럼마다 매 `Fetch()`
  이후 직접 호출해야 하므로 반복이 많은 루프에서는 `BindCol` 쪽이 호출 비용이 적다).

```cpp
while( guard->Fetch() )
{
    guard->GetData(1, tszName, nNameBuf);   // 문자열 전용 오버로드
    guard->GetData(2, age);                 // 템플릿 오버로드
}
```

### 9.3 1회성 쿼리 — `ExecDirect`

파라미터 바인딩 없이 한 번만 실행하고 버릴 쿼리는 `PrepareQuery`+`Execute` 대신
`ExecDirect` 하나로 끝낸다.

```cpp
OdbcConnGuard guard(&pool);
if( guard == nullptr ) return false;

if( !guard->ExecDirect(_T("DELETE FROM session_cache WHERE expire_at < GETDATE()")) )
    return false;

int64 nDeleted = guard->RowCount();   // 영향받은 행 수
```

### 9.4 트랜잭션 — `Commit` / `Rollback`

여러 쿼리를 하나의 트랜잭션으로 묶을 때는 자동 커밋을 끄고, 실패 시
`Rollback()`으로 되돌린다.

```cpp
OdbcConnGuard guard(&pool);
if( guard == nullptr ) return false;

guard->SetAutoCommitMode((SQLPOINTER)SQL_AUTOCOMMIT_OFF);

bool bOk = true;
bOk &= guard->ExecDirect(_T("UPDATE accounts SET gold = gold - 100 WHERE id = 1"));
bOk &= guard->ExecDirect(_T("UPDATE accounts SET gold = gold + 100 WHERE id = 2"));

if( bOk )
    guard->Commit();
else
    guard->Rollback();

guard->SetAutoCommitMode((SQLPOINTER)SQL_AUTOCOMMIT_ON);   // 다음 대여자를 위해 원복
```

- 풀에서 빌린 커넥션은 반납 후 다른 호출자가 재사용하므로, 자동 커밋 모드를
  바꿨다면 트랜잭션이 끝난 뒤 원래 모드로 되돌려 두는 것이 안전하다.

### 9.5 출력 파라미터 바인딩 (저장 프로시저 OUT 인자)

```cpp
guard->PrepareQuery(_T("{CALL GetUserRank(?, ?)}"));

int32 userId = 42;
guard->BindParamInput(userId);   // 입력: 유저 ID

int32 rank = 0;
guard->BindParamOutput(rank);    // 출력: 프로시저가 채워줄 랭크 값

if( guard->Execute() )
{
    // rank에 프로시저 실행 결과가 채워짐
}
```

### 9.6 바이너리 데이터 바인딩

```cpp
guard->PrepareQuery(_T("UPDATE user_profile SET avatar = ? WHERE id = ?"));

BYTE avatarData[4096];
int32 nAvatarSize = LoadAvatarBytes(avatarData, sizeof(avatarData));
SQLLEN lRetSize = 0;

guard->BindParamInput(1, avatarData, nAvatarSize, lRetSize);   // 크기에 따라 BINARY/LONGVARBINARY 자동 선택
guard->BindParamInput(2, userId);

guard->Execute();
```

- 크기가 `DATABASE_BINARY_MAX`를 넘으면 자동으로 `SQL_LONGVARBINARY`로 바인딩되므로,
  작은 썸네일이든 큰 첨부 파일이든 같은 호출 형태로 처리할 수 있다.
- `avatarData`가 `nullptr`이면 `lRetSize`가 `SQL_NULL_DATA`로 설정되어 NULL 값으로
  바인딩된다.

### 9.7 대량 결과 일괄 Fetch — `AllSets`

수천~수만 행을 한 번에 받아야 하는 배치 처리는 컬럼별 `BindCol` 반복 대신 행 단위
배열 바인딩을 쓴다. ODBC의 행 단위(row-wise) 바인딩은 각 컬럼의 데이터 포인터뿐
아니라 널/길이 지시자(indicator) 포인터도 같은 보폭(stride)으로 전진해야 하므로,
지시자 필드를 행 구조체 안에 함께 두는 것이 안전하다.

```cpp
constexpr int32 MAX_ROWS = 1000;

struct ConsumerRow
{
    int32  nNo;
    SQLLEN indNo;          // nNo 컬럼의 널/길이 지시자
    TCHAR  tszName[50];
    SQLLEN indName;        // tszName 컬럼의 널/길이 지시자
};

ConsumerRow rows[MAX_ROWS];

guard->PrepareQuery(_T("SELECT no, name FROM consumers"));
guard->Execute();

// 1. 행 크기(stride)와 한 번에 받을 최대 행 수를 먼저 설정
guard->AllSets(sizeof(ConsumerRow), MAX_ROWS);

// 2. 각 컬럼은 "첫 번째 행(rows[0])의 필드 주소"만 넘긴다.
//    SQL_ATTR_ROW_BIND_TYPE(= sizeof(ConsumerRow))이 이미 설정돼 있으므로,
//    드라이버가 이 주소를 시작점으로 매 행마다 sizeof(ConsumerRow)만큼씩
//    건너뛰며 데이터/지시자를 채워 넣는다.
guard->BindCol(1, SQL_C_SLONG, &rows[0].nNo, sizeof(rows[0].nNo), &rows[0].indNo);
#ifdef _UNICODE
guard->BindCol(2, SQL_C_WCHAR, rows[0].tszName, sizeof(rows[0].tszName), &rows[0].indName);
#else
guard->BindCol(2, SQL_C_CHAR, rows[0].tszName, sizeof(rows[0].tszName), &rows[0].indName);
#endif

// 3. Fetch 한 번으로 최대 MAX_ROWS개까지 한꺼번에 채워진다.
while( guard->Fetch() )
{
    int32 nFetched = guard->GetFetchedRows();   // 이번 호출에서 실제로 채워진 행 수
    for( int32 i = 0; i < nFetched; ++i )
    {
        if( rows[i].indName == SQL_NULL_DATA )
            continue;   // name이 NULL인 행

        // rows[i].nNo, rows[i].tszName 사용
    }
}
```

- 지시자를 행 구조체 밖의 별도 배열(`SQLLEN indNo[MAX_ROWS]`)로 두면 그 배열은
  기본적으로 `sizeof(SQLLEN)` 보폭으로 취급되어 행 단위 보폭과 어긋나므로 피한다.
- `GetFetchedRows()`로 이번에 몇 행이 채워졌는지 반드시 확인하고 그 개수만큼만
  순회해야 한다 — 배열 전체가 항상 다 채워지는 것은 아니며, 마지막 배치는 더 적을 수 있다.
- 행 수가 많지 않거나(수십~수백 행) 코드 단순함이 더 중요하면 9.8처럼 `BindCol`을
  한 행짜리 변수에 걸어두고 `Fetch()`를 반복하며 컨테이너에 쌓는 방식이 더 읽기
  쉽다. `AllSets()` 방식은 매 반복의 함수 호출/커서 이동 오버헤드를 줄이고 싶은
  진짜 대량(수천 행 이상) 처리에 적합하다.

### 9.8 여러 행을 한 번에 컨테이너에 담기

한 행씩 즉시 처리하지 않고, 조회 결과 전체를 리스트로 모아서 반환하는 흔한 패턴이다.
`BindCol`은 반복문 시작 전 한 번만 걸어두면 되고, `Fetch()`가 `true`를 반환하는
동안 매번 최신 값이 채워진 변수를 읽어 컨테이너에 복사해 쌓는다.

```cpp
struct UserInfo
{
    int32 id;
    TCHAR tszName[64];
    int32 level;
};

std::vector<UserInfo> ReadGuildMembers(COdbcConnPool* pPool, int32 guildId)
{
    std::vector<UserInfo> result;

    OdbcConnGuard guard(pPool);
    if( guard == nullptr ) return result;

    if( !guard->PrepareQuery(_T("SELECT id, name, level FROM users WHERE guild_id = ?")) )
        return result;

    guard->BindParamInput(guildId);
    if( !guard->Execute() ) return result;

    // Fetch가 매번 같은 변수에 다음 행 값을 덮어써주므로,
    // BindCol은 루프 밖에서 딱 한 번만 호출한다.
    UserInfo row = {};
    int32 nNameBuf = sizeof(row.tszName);
    SQLLEN lRetSize = 0;

    guard->BindCol(1, SQL_C_SLONG, reinterpret_cast<int64&>(row.id), lRetSize);
    guard->BindCol(2, row.tszName, nNameBuf, lRetSize);
    guard->BindCol(3, SQL_C_SLONG, reinterpret_cast<int64&>(row.level), lRetSize);

    while( guard->Fetch() )
    {
        result.push_back(row);   // 현재 채워진 값을 복사해 쌓음
    }

    return result;
}
```

- 컨테이너에 쌓는 시점은 반드시 `Fetch()` 성공 직후여야 한다 — `row` 변수 자체는
  다음 `Fetch()` 호출에서 덮어써지므로, 참조나 포인터가 아니라 값 복사(`push_back`)로
  담아야 한다.

---

## 10. 참고 상수 (외부 정의로 추정)

아래 매크로/상수들은 본 파일들에는 정의되어 있지 않고 외부 헤더(공통 정의 헤더)에서 온 것으로 보인다.

- `DATABASE_DEFAULT_LOGIN_TIMEOUT`, `DATABASE_DEFAULT_CONNECTION_TIMEOUT`, `DATABASE_DEFAULT_QUERY_TIMEOUT`
- `DATABASE_DSN_STRLEN`, `DATABASE_ERRORMSG_STRLEN`, `DATABASE_BUFFER_SIZE`, `DATABASE_COLUMN_NAME_STRLEN`
- `DATABASE_VARCHAR_MAX`, `DATABASE_WVARCHAR_MAX`, `DATABASE_BINARY_MAX`
- `EDBClass` (enum: `NONE`, `MSSQL`, `MYSQL`, `ORACLE` 등)
- `LOG_ERROR`, `LOG_INFO`, `LOG_DEBUG` (프로젝트 공통 로깅 매크로)
