# CBaseMySQL 설계 문서

## 개요

`CBaseMySQL`은 libmysql C API를 감싼 MySQL 접속/쿼리 래퍼 클래스이다. 연결 관리, 일반 쿼리 실행, Prepared Statement 실행, 결과 데이터 추출, 에러 로깅까지 하나의 클래스에서 담당하도록 설계되었다. `BaseAllocator`를 상속하여 커스텀 메모리 할당자 체계에 편입된다.

추가로, Prepared Statement의 파라미터 바인딩을 타입 안전하게 처리하기 위한 템플릿 기반 타입 트레이트 시스템(`mysql_param_attr`, `CMySQLParamAttrMgr`)이 함께 정의되어 있다.

---

## 1. CBaseMySQL 클래스

### 1.1 멤버 변수

| 멤버 | 역할 |
|---|---|
| `m_bConnected` | 연결 상태 플래그 |
| `m_szDBHost / m_szDBUserId / m_szDBPasswd / m_szDBName` | 접속 정보 (고정 크기 버퍼) |
| `m_szCharacterSet` | 문자셋 이름 캐시 |
| `m_szSelectDBName` | 마지막으로 선택된 DB 이름 |
| `m_uiPort` | 접속 포트 |
| `m_pConn` | `MYSQL*` 커넥션 핸들 |
| `m_pStmt` | `MYSQL_STMT*` Prepared Statement 핸들 |

### 1.2 멤버 함수 설명 (헤더 선언 순서)

#### 생성/소멸

| 함수 | 설명 |
|---|---|
| `CBaseMySQL()` | 기본 생성자. 접속 정보 버퍼를 모두 0으로 초기화하고 핸들·플래그를 미연결 상태로 세팅. |
| `CBaseMySQL(host, userId, passwd, dbName, port)` | 접속 정보를 즉시 세팅하는 생성자. 문자셋/선택 DB 버퍼는 0으로 초기화. |
| `~CBaseMySQL()` | `Disconnect()` 호출로 연결 자원 정리. |

#### 연결/자원 관리

| 함수 | 반환값 | 설명 |
|---|---|---|
| `Connect(connectTimeout, readTimeout, writeTimeout, pluginDir)` | `bool` | `mysql_init` 후 옵션(플러그인 경로, 각종 타임아웃)을 설정하고 `mysql_real_connect`로 접속. 이미 연결돼 있으면 즉시 `true`. 접속 성공 시 문자셋을 적용하거나 서버 기본값을 로그로 남김. 예외 발생 시 `Disconnect()` 후 `false`. |
| `Disconnect()` | `bool` | `mysql_close`로 연결 종료, 핸들을 `nullptr`로, 플래그를 `false`로 리셋. 항상 `true` 반환. |
| `StmtClose()` | `void` | `m_pStmt`가 있으면 `mysql_stmt_close` 호출. |
| `FreeResult(res)` | `void` | 전달된 `MYSQL_RES*`가 있으면 `mysql_free_result` 호출. |
| `GetConnPtr()` | `MYSQL*` | 내부 커넥션 핸들 반환. |
| `IsConnected()` | `bool` | `m_bConnected` 값 반환. |

#### 서버/클라이언트 정보

| 함수 | 반환값 | 설명 |
|---|---|---|
| `GetServerInfo(ptszServerInfo)` | `bool` | `mysql_get_server_info` / `mysql_get_host_info` / `mysql_get_server_version`을 조합해 문자열로 포맷 후 `TCHAR` 버퍼에 기록(유니코드 빌드 시 변환 포함). |
| `GetClientInfo(ptszClientInfo)` | `bool` | `mysql_get_client_info` / `mysql_get_client_version`을 조합해 동일한 방식으로 기록. |

#### 문자셋

| 함수 | 반환값 | 설명 |
|---|---|---|
| `SetCharacterSetName(ptszName)` | `bool` | 지정한 문자셋 이름을 `m_szCharacterSet`에 저장(유니코드 빌드 시 `WideCharToMultiByte`로 변환). 실제 서버 적용은 다음 `Connect()` 시점에 이루어짐. |
| `GetCharacterSetName(ptszName)` | `bool` | `m_szCharacterSet`이 비어 있으면 `mysql_character_set_name()`으로 채운 뒤 반환. |
| `GetCharacterSetInfo(charset)` | `bool` | `mysql_get_character_set_info()`로 `MY_CHARSET_INFO` 구조체를 채움. |
| `GetEscapeString(dest, src, len)` | `bool` | `mysql_real_escape_string()` 래핑. |

#### 트랜잭션

| 함수 | 반환값 | 설명 |
|---|---|---|
| `AutoCommit(bSetValue)` | `bool` | `mysql_autocommit(conn, 0 또는 1)` 호출. |
| `StartTransaction()` | `bool` | `"START TRANSACTION"` 쿼리 실행. |
| `Commit()` | `bool` | `mysql_commit()` 호출. |
| `Rollback()` | `bool` | `mysql_rollback()` 호출. |

#### DB 선택

| 함수 | 반환값 | 설명 |
|---|---|---|
| `SelectDB(const char*)` | `bool` | `mysql_select_db()` 호출. 성공 시 `m_szSelectDBName`에 캐시, 실패 시 에러 로그. |
| `SelectDB(const wchar_t*)` | `bool` | 와이드 문자열을 `WideCharToMultiByte(CP_ACP, ...)`로 변환 후 위 오버로드와 동일 로직 수행. |

#### Prepared Statement

| 함수 | 반환값 | 설명 |
|---|---|---|
| `Prepare(const char*)` | `bool` | 미연결 시 `Connect()` 자동 시도. `mysql_stmt_init` → `mysql_stmt_prepare`. 서버 연결 끊김 에러면 `Disconnect()`만 하고 반환, 그 외 에러는 `StmtErrorQuery()`로 로깅. |
| `Prepare(const wchar_t*)` | `bool` | UTF-8 변환 후 `Prepare(const char*)`에 위임. |
| `PrepareBindParam(MYSQL_BIND*)` | `bool` | `m_pStmt`가 없으면 실패. `mysql_stmt_bind_param()` 호출, 실패 시 `StmtErrorMessage` 로깅. |
| `PrepareBindParam(CVector<MYSQL_BIND>&)` | `bool` | 위와 동일하되 `CVector`의 내부 배열(`.data()`)을 전달. |
| `PrepareAttSet(attr_type, attr)` | `bool` | `mysql_stmt_attr_set()` 호출. 커서 타입, prefetch row 수, array size 등 설정. |
| `PrepareExecute(pnIdx)` | `bool` | `mysql_stmt_execute()` 실행. `pnIdx`가 주어지면 실행 성공 여부와 무관하게 `mysql_stmt_insert_id()` 값을 기록. |

#### 일반 쿼리 실행

| 함수 | 반환값 | 설명 |
|---|---|---|
| `Execute(const char*)` / `Execute(const wchar_t*)` | `bool` | `Query()`에 그대로 위임. |
| `Query(const char*)` | `bool` | 미연결 시 `Connect()` 시도. 최대 2회 재시도 루프: 실패 시 `CR_SERVER_GONE_ERROR`/`CR_SERVER_LOST`면 재연결 후 재시도, 그 외 에러는 즉시 로깅 후 중단. |
| `Query(const wchar_t*)` | `bool` | UTF-8 변환 후 `Query(const char*)`에 위임. |
| `Query(const char*, MYSQL_RES*&)` | `bool` | `Query()` 실행 후 `mysql_use_result()`로 결과 핸들 획득. 실패 시 에러 로깅. |
| `Query(const wchar_t*, MYSQL_RES*&)` | `bool` | UTF-8 변환 후 위 오버로드에 위임. |
| `Query(const char*, pclsData, FetchRow)` | `bool` | `mysql_use_result()` 후 `mysql_fetch_row` 루프를 돌며 매 행마다 콜백 호출. 콜백이 `false`를 반환하면 루프 중단. 종료 시 `mysql_free_result()`로 결과 해제. |
| `Query(const wchar_t*, pclsData, FetchRow)` | `bool` | UTF-8 변환 후 위와 동일한 콜백 순회 로직을 직접 수행. |

#### 결과/스테이트먼트 메타 정보

| 함수 | 반환값 | 설명 |
|---|---|---|
| `GetAffectedRow()` | `uint64` | `mysql_affected_rows()`. 미연결 시 0. |
| `GetFieldCount()` | `uint32` | `mysql_field_count()`. 미연결 시 0. |
| `GetStmtNumRows()` | `uint64` | `mysql_stmt_num_rows()`. `m_pStmt` 없으면 0. |
| `GetStmtAffectedRow()` | `uint64` | `mysql_stmt_affected_rows()`. `m_pStmt` 없으면 0. |
| `GetStmtFieldCount()` | `uint32` | `mysql_stmt_field_count()`. `m_pStmt` 없으면 0. |
| `GetNumRows(res)` | `uint64` | `mysql_num_rows()`. `res`가 `NULL`이면 0. |
| `GetNumFields(res)` | `uint64` | `mysql_num_fields()`. `res`가 `NULL`이면 0. |
| `GetFetchField(res, pFields, fieldCount)` | `bool` | `GetNumFields()`로 컬럼 수를 채우고 `mysql_fetch_field()`로 필드 정보 포인터를 반환. |

#### 데이터 추출 (`GetData` 오버로드 7종, 헤더 선언 순)

| 시그니처 | 설명 |
|---|---|
| `GetData(Row, col, bool&)` | `atoi()`로 정수 변환 후 `bool`에 대입. |
| `GetData(Row, col, char*, bufSize)` | `strncpy_s`로 원본 문자열 복사. |
| `GetData(Row, col, wchar_t*, bufSize)` | `MultiByteToWideChar(CP_UTF8, ...)`로 변환해 와이드 버퍼에 기록. |
| `GetData(Row, col, int32&)` | `atoi()`. |
| `GetData(Row, col, uint32&)` | `strtoul(..., 10)`. |
| `GetData(Row, col, int64&)` | `atoll()`. |
| `GetData(Row, col, uint64&)` | `strtoull(..., 10)`. |

모든 `GetData` 오버로드는 `Row[nColNum]`이 `nullptr`(SQL NULL)이면 아무 것도 하지 않고 반환하므로, 호출자는 출력 변수를 사전에 원하는 기본값으로 초기화해 두어야 한다.

#### 에러 조회 (public)

| 함수 | 반환값 | 설명 |
|---|---|---|
| `GetErrorNo()` | `uint32` | `mysql_errno()` 그대로 반환. |
| `GetErrorMessage(ptszMessage)` | `bool` | 미연결 시 `false`. `mysql_error()` 결과를 (유니코드 빌드 시 변환하여) 버퍼에 기록. |
| `GetStmtErrorMessage(pStmt, ptszMessage)` | `bool` | 미연결 시 `false`. `mysql_stmt_error(pStmt)` 결과를 동일한 방식으로 기록. |

#### 정적 바인딩 헬퍼

| 함수 | 설명 |
|---|---|
| `static BindParam(const char*, ulong*)` | 원본 문자열 포인터를 그대로 `bind.buffer`에 저장(복사 없음). 반복 호출 시 각 호출은 서로 다른 메모리를 가리켜야 함. |
| `static BindParam(const wchar_t*, ulong)` | UTF-8 변환 후 `new`로 버퍼를 할당해 복사본 저장. `ClearBindParam()`으로 짝을 맞춰 해제해야 함. |
| `static BindParam<T>(const T&)` | `if constexpr` 분기로 산술 타입(`bool`, `INT8`~`UINT64`, `float`, `double`, `nullptr_t`)별 `buffer_type`/`is_unsigned` 설정, 원본 변수의 주소를 그대로 바인딩(복사 없음). |
| `static ClearBindParam(bind)` | `MYSQL_TYPE_STRING`인 경우에 한해 `buffer`/`length`를 `SAFE_DELETE_ARRAY`/`SAFE_DELETE`로 해제. |

#### 에러 로깅 (private)

| 함수 | 설명 |
|---|---|
| `ErrorQuery(func, sql, errno, message)` | `errno`가 지정되지 않으면(`< 1`) 현재 커넥션에서 `mysql_errno`/`mysql_error`를 직접 조회. 함수명/SQL/에러메시지를 (유니코드 빌드 시 변환하여) `LOG_ERROR`로 출력. |
| `StmtErrorQuery(pStmt, func, sql, errno, message)` | 위와 동일하되 `mysql_stmt_errno`/`mysql_stmt_error`를 사용. |

---

## 2. 사용 예시

### 2.1 기본 연결 + 단순 SELECT (콜백 없이 결과 핸들 직접 순회)

```cpp
CBaseMySQL db("127.0.0.1", "root", "password", "game_db", 3306);

if( !db.Connect() )
{
    // 에러 처리 - db.GetErrorMessage() / db.GetErrorNo() 로 조회 가능
    return;
}

MYSQL_RES* pRes = nullptr;
if( db.Query("SELECT user_id, nickname FROM users WHERE level > 10", pRes) )
{
    MYSQL_ROW Row;
    while( (Row = mysql_fetch_row(pRes)) )
    {
        int32 nUserId = 0;
        char szNickname[64] = { 0, };

        db.GetData(Row, 0, nUserId);
        db.GetData(Row, 1, szNickname, sizeof(szNickname));

        // nUserId, szNickname 사용
    }

    db.FreeResult(pRes);
}
```

### 2.2 콜백 기반 Query (행마다 자동 처리, 결과 해제도 내부에서 처리)

```cpp
struct FetchContext
{
    int32 nTotalCount = 0;
};

bool OnFetchUserRow(void* pclsData, MYSQL_ROW& Row)
{
    FetchContext* pCtx = static_cast<FetchContext*>(pclsData);

    // GetData는 static이 아니므로, Row 파싱만 필요하면 atoi 등을 직접 쓰거나
    // db 인스턴스를 캡처 가능한 람다/펑터로 바꿔서 GetData를 호출한다.
    int32 nUserId = Row[0] ? atoi(Row[0]) : 0;

    ++pCtx->nTotalCount;
    return true; // false를 반환하면 순회 중단
}

FetchContext ctx;
db.Query("SELECT user_id FROM users", &ctx, OnFetchUserRow);
```

### 2.3 Prepared Statement 전체 흐름 (INSERT + auto-increment 조회)

```cpp
db.Prepare("INSERT INTO logs (user_id, message) VALUES (?, ?)");

int32 nUserId = 1001;
char szMessage[128] = "login";
unsigned long ulMsgLen = static_cast<unsigned long>(strlen(szMessage));

std::vector<MYSQL_BIND> binds;
binds.push_back(CBaseMySQL::BindParam(nUserId));
binds.push_back(CBaseMySQL::BindParam(szMessage, &ulMsgLen));

db.PrepareBindParam(binds.data());

uint64_t nInsertId = 0;
if( db.PrepareExecute(&nInsertId) )
{
    // nInsertId 사용
}

db.StmtClose();

for( auto& bind : binds )
{
    CBaseMySQL::ClearBindParam(bind); // BindParam(char*) 계열은 실제로는 복사본이 없어 해제 대상 아님,
                                       // BindParam(wchar_t*) 사용 시에는 반드시 호출 필요
}
```

### 2.4 와이드 문자열 파라미터 바인딩 (복사본 생성 + 명시적 해제)

```cpp
std::wstring wstrName = L"홍길동";
MYSQL_BIND bind = CBaseMySQL::BindParam(wstrName.c_str(), static_cast<ulong>(wstrName.size()));

// ... PrepareBindParam(&bind) 등으로 사용 ...

CBaseMySQL::ClearBindParam(bind); // new로 할당된 buffer/length를 반드시 해제
```

### 2.5 CMySQLParamAttrMgr 사용 예시

```cpp
CMySQLParamAttrMgr attrMgr;

int32 nAge = 25;
CMySQLParamAttr& attr1 = attrMgr(nAge); // target_type = MYSQL_TYPE_LONG, is_unsigned = false

char szName[32] = "hong";
int32 nNameLen = static_cast<int32>(strlen(szName));
CMySQLParamAttr& attr2 = attrMgr(szName, nNameLen); // target_type = MYSQL_TYPE_STRING

// attr1, attr2를 이용해 MYSQL_BIND 구성에 필요한 타입/버퍼 정보를 조회
```

> 주의: `CMySQLParamAttrMgr`는 내부에 `m_dbParamAttr` 멤버 하나만 두고 매 호출마다 값을 덮어써 참조로 반환하므로, 위 예시처럼 `attr1`을 보관한 상태에서 `attrMgr(...)`를 다시 호출하면 `attr1`의 내용도 함께 바뀐다. 반드시 호출 즉시 값을 소비하는 방식으로만 사용해야 한다.

---

## 3. MySQL 파라미터 타입 트레이트 시스템

Prepared Statement 바인딩 시 C++ 타입 → MySQL `enum_field_types` 매핑을 컴파일 타임에 결정하기 위한 별도의 트레이트 기반 설계이다 (위 `BindParam<T>`의 `if constexpr` 방식과는 별개의, 매크로 기반 특수화 방식).

### 3.1 `mysql_param_attr_base` / `mysql_param_attr`

```cpp
template<enum_field_types TARGET_TYPE, bool IS_UNSIGNED = false>
struct mysql_param_attr_base { ... };

template<typename _TMain>
struct mysql_param_attr : mysql_param_attr_base<MYSQL_TYPE_NULL> {};
```

`mysql_param_attr<T>`의 기본 템플릿은 `MYSQL_TYPE_NULL`로 폴백되며, `MYSQL_PARAM_ATTR` 매크로로 타입별 특수화를 등록한다.

### 3.2 `MYSQL_PARAM_ATTR` 매크로

```cpp
#define MYSQL_PARAM_ATTR(d_type, t_type, is_unsigned) \
	template<> struct mysql_param_attr<d_type> : mysql_param_attr_base<t_type, is_unsigned> {}
```

`bool`, `INT8`~`UINT64`, `FLOAT`, `DOUBLE`, `CHAR*`, `WCHAR*`, `MYSQL_TIME`까지 각 타입에 대응하는 `MYSQL_TYPE_*`와 부호 여부를 특수화 목록으로 등록한다. 이렇게 하면 이후 `mysql_param_attr<T>::target_type` / `::is_unsigned`를 컴파일 타임 상수로 조회할 수 있다.

### 3.3 `CMySQLParamAttr`

바인딩 대상 하나의 정보(타입, 버퍼 포인터, 버퍼 크기, 부호 여부)를 담는 값 객체. `SetParamAttr()`로 타입 정보를, `SetValue()`로 버퍼 정보를 설정한다.

### 3.4 `CMySQLParamAttrMgr`

`operator()`를 오버로드하여 함수 호출 문법으로 파라미터 바인딩 정보를 생성하는 팩토리 겸 매니저이다.

- **`operator()(EDataType& data)`**: 템플릿 버전. `mysql_param_attr<EDataType>`에서 타입/부호 정보를 조회하고, `&data`와 `sizeof(EDataType)`를 버퍼로 설정.
- **`operator()(CHAR* data, int32& nBuffSize)`**: 문자열 특수 처리. 버퍼 크기를 그대로 사용.
- **`operator()(WCHAR* data, int32& nBuffSize)`**: 와이드 문자열 특수 처리. 버퍼 크기를 `nBuffSize * 2`로 계산(문자당 2바이트 가정).

내부에 멤버 `m_dbParamAttr` 하나만 두고 매 호출마다 값을 갱신해 참조로 반환하는 구조이므로, 동시에 여러 파라미터를 살아있는 상태로 보관해야 하는 호출 패턴에는 적합하지 않고, 호출 즉시 바인딩 정보를 소비하는 단발성 사용을 전제로 한 설계이다.

---

## 4. 전체 구조 요약

```
CBaseMySQL
 ├─ 연결 관리: Connect / Disconnect / IsConnected
 ├─ 일반 쿼리: Query (3계열 × char/wchar_t) / Execute
 ├─ Prepared Statement: Prepare → PrepareBindParam → PrepareAttSet → PrepareExecute → StmtClose
 ├─ 바인딩 헬퍼: BindParam (char*/wchar_t*/템플릿 산술형) / ClearBindParam
 ├─ 결과 메타/데이터: GetNumRows/Fields, GetFetchField, GetData × 7
 ├─ 에러 처리: GetErrorNo/Message, ErrorQuery/StmtErrorQuery (private)
 └─ 부가 기능: 문자셋, DB 선택, escape string, 트랜잭션 제어

mysql_param_attr 시스템
 ├─ mysql_param_attr_base<TARGET_TYPE, IS_UNSIGNED>
 ├─ mysql_param_attr<T> (매크로 특수화)
 ├─ CMySQLParamAttr (값 객체)
 └─ CMySQLParamAttrMgr (operator() 팩토리)
```
