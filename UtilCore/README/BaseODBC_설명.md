# CBaseODBC 설계 문서

## 1. 개념

`CBaseODBC`는 Windows ODBC C API(`SQLHENV`/`SQLHDBC`/`SQLHSTMT`, `SQLBindParameter`,
`SQLBindCol`, `SQLExecute` 등)를 감싸서 하나의 커넥션이 담당하는 전체 쿼리 생명주기
(연결 → 문 준비 → 파라미터/컬럼 바인딩 → 실행 → Fetch → 커밋)를 C++ 클래스 하나로
제공한다. `COdbcConnPool`이 슬롯 단위로 대여/반납하는 실제 커넥션 객체가 바로
`CBaseODBC`다.

핵심 설계 방향은 다음과 같다.

- **C++ 타입 → ODBC 타입 자동 매핑**: `int32`, `double`, `TCHAR*` 같은 C++ 타입을 넘기면
  그에 대응하는 ODBC C 데이터 타입/SQL 데이터 타입/정밀도를 템플릿 특수화로 자동 결정해,
  매 바인딩마다 SQL 타입 상수를 손으로 지정할 필요가 없다.
- **하나의 재사용 스크래치 객체로 바인딩 정보 전달**: 파라미터/컬럼 바인딩 시마다
  새 객체를 할당하지 않고, 클래스가 들고 있는 단일 `CDBParamAttr`/`CDBColAttr` 인스턴스를
  갱신해 참조로 돌려주는 방식으로 힙 할당을 없앤다.
- **일관된 에러 로깅**: 모든 ODBC API 실패 지점에서 `CDBError`로 SQLSTATE/네이티브
  에러코드/메시지를 뽑아, 실패한 함수 이름과 직전 쿼리 텍스트까지 포함해 로그로 남긴다.
- **Unicode/ANSI 이중 빌드 지원**: `_UNICODE` 정의 여부에 따라 `SQLWCHAR`/`SQLCHAR`
  경로를 `#ifdef`로 분기한다.

## 2. 구성 요소

| 구성 요소 | 역할 |
|---|---|
| `CBaseODBC` | 커넥션 하나(env/conn/stmt 핸들 묶음)를 표현하는 메인 클래스 |
| `CDBError` | `SQLGetDiagRec(W)`를 감싸 SQLSTATE/에러코드/메시지를 하나의 문자열로 포맷하는 함수 객체 |
| `odbc_param_attr_base` / `odbc_param_attr<T>` | C++ 타입 → (C 데이터 타입, SQL 데이터 타입, 파라미터 크기) 매핑 템플릿 (파라미터 바인딩용) |
| `CDBParamAttr` / `CDBParamAttrMgr` | 위 매핑 결과와 버퍼 포인터/크기를 담는 값 객체와, 타입별 오버로드로 이를 채워 반환하는 매니저 |
| `odbc_col_attr_base` / `odbc_col_attr<T>` | C++ 타입 → ODBC C 타입 매핑 템플릿 (컬럼 바인딩용) |
| `CDBColAttr` / `CDBColAttrMgr` | 컬럼 바인딩용 값 객체와 매니저 (파라미터 쪽과 동일한 구조) |
| `COL_DESCRIPTION` | `DescribeCol()`이 채우는 컬럼 메타데이터(이름/타입/크기/자릿수/NULL 허용/표시 길이) 구조체 |

## 3. 멤버 변수 설명 (`CBaseODBC`)

| 변수 | 설명 |
|---|---|
| `m_hEnv` | ODBC 환경 핸들 (`SQLHENV`) |
| `m_hConn` | DB 연결 핸들 (`SQLHDBC`) |
| `m_hStmt` | SQL 문 핸들 (`SQLHSTMT`) — `PrepareQuery`/`Execute`/`ExecDirect` 시 필요하면 지연 생성됨 |
| `m_DbClass` | DB 종류 (MSSQL 등) |
| `m_bLoadExcelFile` | Excel 파일을 ODBC로 여는 특수 모드 여부 — 쿼리 타임아웃 설정과 `Fetch()` 에러 로깅을 이 값에 따라 다르게 처리 |
| `m_nParamNum` | 현재까지 바인딩된 입력 파라미터 개수 (자동 증가 오버로드가 사용) |
| `m_nColNum` | 현재까지 바인딩된 컬럼 개수 (자동 증가 오버로드가 사용) |
| `m_nFetchedRows[1]` | `AllSets()`로 행 단위 일괄 바인딩 시, 한 번의 Fetch로 실제 가져온 행 수를 ODBC 드라이버가 써주는 출력 버퍼 |
| `m_DBParamAttrMgr` | 파라미터 바인딩용 타입 매핑 매니저 (재사용 스크래치 객체 보유) |
| `m_DBColAttrMgr` | 컬럼 바인딩용 타입 매핑 매니저 (재사용 스크래치 객체 보유) |
| `m_tszDSN` | 접속 DSN 문자열 |
| `m_tszQueryInfo` | 마지막으로 준비/실행한 쿼리 텍스트 (에러 로그에 함께 남김) |
| `m_tszLastError` | 마지막 에러 메시지 버퍼 |

## 4. 멤버 함수 설명

### 4.1 연결 관리
| 함수 | 설명 |
|---|---|
| `CBaseODBC(dbClass, bLoadExcelFile)` / `CBaseODBC(dbClass, dsn, bLoadExcelFile)` | DSN을 나중에 줄지 생성 시점에 줄지에 따른 두 생성자 오버로드 |
| `Connect(loginTimeout, connTimeout)` | ENV 핸들 생성 → ODBC 3.0 버전 설정 → DBC 핸들 생성 → 로그인/연결 타임아웃 설정 → `SQLDriverConnect`로 실제 접속 → 드라이버 버전 조회까지 한 번에 수행 |
| `Disconnect()` | stmt → conn → env 순으로 핸들을 역순 해제 |
| `IsConnected()` | `SQL_ATTR_CONNECTION_DEAD` 속성을 조회해 연결이 살아있는지 확인 (쿼리를 직접 날리지 않고 드라이버가 캐시한 상태로 판단) |
| `IsConnectionValid()` | 세 핸들이 전부 유효(non-null)한지만 보는 가벼운 검사 (인라인) |
| `GetDBClass()` | 현재 DB 종류 조회 (인라인) |
| `DBMSInfo(server, dbmsName, dbmsVersion)` | `SQLGetInfo`로 서버명/DBMS명/DBMS버전 조회 |

### 4.2 문(Statement) 핸들 관리
| 함수 | 설명 |
|---|---|
| `InitStmtHandle(queryTimeout)` | stmt 핸들 할당 + 읽기 전용 동시성 모드 설정 + (Excel 모드가 아니면) 쿼리 타임아웃 설정 |
| `FreeStmt(option)` | `SQLFreeStmt`를 감싸 `SQL_CLOSE`/`SQL_UNBIND`/`SQL_RESET_PARAMS`/`SQL_DROP` 중 하나를 적용 |
| `ClearStmt()` | 파라미터 리셋 + 컬럼 언바인드 + 커서 닫기를 한 번에 수행하고, 쿼리 텍스트/파라미터·컬럼 카운터를 초기화 (새 쿼리 준비 전 호출) |
| `ResetParamStmt()` | 파라미터 바인딩만 리셋 |
| `UnBindColStmt()` | 컬럼 바인딩만 해제 |

### 4.3 파라미터 바인딩
| 함수 | 설명 |
|---|---|
| `BindParameter(...)` | `SQLBindParameter`를 그대로 감싼 저수준 범용 오버로드 |
| `BindParamInput(tValue)` / `BindParamInput(idx, tValue)` (템플릿) | `odbc_param_attr<T>`로 타입을 자동 판별해 입력 파라미터 바인딩. 인덱스 생략형은 `++m_nParamNum`으로 자동 증가 |
| `BindParamInput(ptszValue)` / `BindParamInput(idx, ptszValue, lRetSize)` | 문자열 전용 오버로드. 문자열 길이(유니코드는 ×2바이트)로 `SQL_VARCHAR`/`SQL_WVARCHAR` 또는 `LONGVARCHAR`/`WLONGVARCHAR` 자동 선택 |
| `BindParamInput(idx, pbData, size, lRetSize)` | 바이너리 전용 오버로드. 크기가 `DATABASE_BINARY_MAX`를 넘으면 `SQL_LONGVARBINARY`, 아니면 `SQL_BINARY`. `pbData == nullptr`이면 `SQL_NULL_DATA`로 바인딩 |
| `BindParamOutput(...)` 계열 | 위와 동일한 패턴의 출력 파라미터(OUT/INOUT) 버전 |

### 4.4 컬럼 바인딩 및 조회
| 함수 | 설명 |
|---|---|
| `BindCol(...)` (저수준) | `SQLBindCol` 그대로 감싼 범용 오버로드 |
| `BindCol(tValue)` / `BindCol(idx, tValue, lRetSize)` (템플릿) | `odbc_col_attr<T>`로 타입 자동 판별 후 컬럼 바인딩. 인덱스 생략형은 `++m_nColNum` 자동 증가 |
| `BindCol(ptszValue, iBuffSize)` / `BindCol(idx, ptszValue, iBuffSize, lRetSize)` | 문자열 컬럼 전용 오버로드 |
| `BindCol(idx, targetType, int64/uint64&, lRetSize)` | 64비트 정수 컬럼 전용 오버로드 (호출자가 타깃 타입을 직접 지정) |
| `GetData(iColNum, tValue)` (템플릿) / `GetData(iColNum, ptszData, iBuffSize)` | 미리 바인딩하지 않고 `Fetch()` 이후 컬럼 값을 즉석에서 읽어오는 `SQLGetData` 경로 |

### 4.5 쿼리 준비/실행
| 함수 | 설명 |
|---|---|
| `PrepareQuery(query)` | stmt 핸들이 없으면 할당 → `ClearStmt()`로 이전 바인딩 정리 → `SQLPrepare`로 준비 (재사용 가능한 파라미터 바인딩과 함께 반복 실행할 때 사용) |
| `Execute()` | 준비된 문을 `SQLExecute`로 실행 |
| `ExecDirect(query)` | 준비 없이 즉시 `SQLExecDirect`로 실행 (1회성 쿼리) |
| `BulkOperations(operation)` | 북마크 기반 일괄 추가/수정/삭제/조회(`SQL_ADD`/`SQL_UPDATE_BY_BOOKMARK` 등)를 수행하는 `SQLBulkOperations` 래퍼 |
| `SetStmtAttr(...)` | 임의의 문 속성을 설정하는 범용 `SQLSetStmtAttr` 래퍼 |
| `AllSets(recordSize, maxRowSize)` | 행 단위 일괄 바인딩(`SQL_ATTR_ROW_BIND_TYPE`) + 배열 크기(`SQL_ATTR_ROW_ARRAY_SIZE`) + 실제 fetch된 행 수 저장 위치(`SQL_ATTR_ROWS_FETCHED_PTR` → `m_nFetchedRows`)를 한 번에 설정해 대량 Fetch를 준비 |

### 4.6 결과 순회 및 트랜잭션
| 함수 | 설명 |
|---|---|
| `Fetch()` | `SQLFetch` 래퍼. `SQL_NO_DATA`는 정상 종료로 처리(로그 없음), 그 외 실패는 `m_bLoadExcelFile`이 아닐 때만 에러 로그를 남김 |
| `GetFetch()` / `MoreResults()` | 에러 처리 없이 `SQLFetch`/`SQLMoreResults`를 그대로 반환 — 호출자가 `SQLRETURN`을 직접 해석해야 하는 다중 결과셋 순회용 |
| `GetFetchedRows()` | `AllSets()` 이후 마지막 Fetch에서 실제로 가져온 행 수 조회 (인라인) |
| `SetAutoCommitMode(valuePtr)` | 자동 커밋 on/off 설정 |
| `Commit()` / `Rollback()` | `SQLEndTran`으로 커밋/롤백 |

### 4.7 메타데이터
| 함수 | 설명 |
|---|---|
| `GetNumCols()` | 결과셋의 컬럼 수 (`SQLNumResultCols`) |
| `RowCount()` | INSERT/UPDATE/DELETE 영향 행 수, 또는 SELECT일 경우 드라이버에 따라 Fetch 이후 유효한 행 수 (`SQLRowCount`) |
| `RowNumber()` | 현재 커서의 행 번호 (`SQL_ATTR_ROW_NUMBER`) |
| `DescribeCol(iColNum, desc)` | 컬럼 이름/타입/크기/자릿수/NULL 허용 여부(`SQLDescribeCol`) + 표시 길이(`SQLColAttribute`)를 `COL_DESCRIPTION`에 채움 |

## 5. 타입 매핑 테이블

### 5.1 파라미터용 (`ODBC_PARAM_ATTR`)

`odbc_param_attr<T>`는 아래 매크로 호출로 특수화되어, C++ 타입마다 (C 데이터 타입,
SQL 데이터 타입, 파라미터 정밀도)를 컴파일 타임에 고정한다.

| C++ 타입 | C 데이터 타입 | SQL 데이터 타입 | 크기/정밀도 |
|---|---|---|---|
| `bool` | `SQL_C_BIT` | `SQL_BIT` | 2 |
| `INT8` / `UINT8` | `SQL_C_S/UTINYINT` | `SQL_TINYINT` | 3 |
| `INT16` / `UINT16` | `SQL_C_S/USHORT` | `SQL_SMALLINT` | 5 |
| `INT32` / `UINT32` | `SQL_C_S/ULONG` | `SQL_INTEGER` | 10 |
| `INT64` / `UINT64` | `SQL_C_S/UBIGINT` | `SQL_BIGINT` | 19 |
| `FLOAT` | `SQL_C_FLOAT` | `SQL_REAL` | 7 |
| `DOUBLE` | `SQL_C_DOUBLE` | `SQL_DOUBLE` | 15 |
| `CHAR*` | `SQL_C_CHAR` | `SQL_VARCHAR` | 254 (기본값, 실제 바인딩 시 문자열 길이로 재계산) |
| `WCHAR*` | `SQL_C_WCHAR` | `SQL_WVARCHAR` | 254 (위와 동일) |
| `SQL_TIMESTAMP_STRUCT` | `SQL_C_TYPE_TIMESTAMP` | `SQL_TYPE_TIMESTAMP` | 23 |

특수화가 없는 타입은 `odbc_param_attr_base<SQL_C_DEFAULT, SQL_VARCHAR>` 기본값으로 처리된다.

### 5.2 컬럼용 (`ODBC_COL_ATTR`)

`odbc_col_attr<T>`는 SQL 데이터 타입 없이 C 데이터 타입(타깃 타입)만 매핑한다 —
컬럼 바인딩은 이미 결정된 결과셋 스키마에 맞춰 애플리케이션이 어떤 C 타입으로
받을지만 지정하면 되기 때문이다.

| C++ 타입 | C 데이터 타입(타깃) |
|---|---|
| `bool` | `SQL_C_TINYINT` |
| `INT8`/`UINT8` | `SQL_C_S/UTINYINT` |
| `INT16`/`UINT16` | `SQL_C_S/USHORT` |
| `INT32`/`UINT32` | `SQL_C_S/ULONG` |
| `INT64`/`UINT64` | `SQL_C_S/UBIGINT` |
| `FLOAT` | `SQL_C_FLOAT` |
| `DOUBLE` | `SQL_C_DOUBLE` |
| `CHAR*` | `SQL_C_CHAR` |
| `WCHAR*` | `SQL_C_WCHAR` |
| `SQL_TIMESTAMP_STRUCT` | `SQL_C_TYPE_TIMESTAMP` |

### 5.3 `CDBParamAttrMgr` / `CDBColAttrMgr`의 동작 방식

두 매니저는 각각 멤버로 단 하나의 `CDBParamAttr`/`CDBColAttr` 스크래치 객체
(`m_dbParamAttr`/`m_dbColAttr`)를 갖고 있다. `operator()`가 호출될 때마다:

1. 넘겨받은 값의 타입에 맞는 `odbc_param_attr<T>`/`odbc_col_attr<T>` 특수화에서
   C 데이터 타입/SQL 타입/크기를 읽어 스크래치 객체에 설정하고,
2. 값의 주소와 크기(POD는 `sizeof`, 문자열은 `strlen`/`wcslen` 기반)를 채운 뒤,
3. 그 스크래치 객체에 대한 **참조**를 반환한다.

호출자(`BindParamInput`/`BindCol` 등)는 반환된 참조를 그 자리에서 즉시
`SQLBindParameter`/`SQLBindCol` 호출에 사용하므로 별도 힙 할당 없이 매 바인딩을
처리한다. 이 참조는 다음 `operator()` 호출 전까지만 유효하다.

## 6. 동작 흐름 (일반적인 쿼리 실행)

```
Connect()
  └─ PrepareQuery(sql)         // ClearStmt()로 이전 바인딩 정리 후 SQLPrepare
       └─ BindParamInput(...)  // 파라미터마다 반복 (m_nParamNum 자동 증가)
            └─ Execute()
                 └─ BindCol(...) 또는 GetData(...)로 결과 조회
                      └─ Fetch()  // 반복 호출, SQL_NO_DATA면 false 반환
                           └─ Commit() / Rollback()
```

1회성 쿼리는 `PrepareQuery`+`Execute` 대신 `ExecDirect(sql)` 하나로 대체할 수 있다.
대량 결과를 한 번에 받아야 하면 `BindCol` 반복 대신 `AllSets()`로 행 단위 배열
바인딩을 설정한 뒤 `Fetch()` 한 번으로 여러 행을 가져오고 `GetFetchedRows()`로
실제 가져온 행 수를 확인한다.

`Connect()` 내부는 각 단계 실패 시 `throw 0`으로 즉시 `catch` 블록으로 점프해
`Disconnect()`를 호출하고 `false`를 반환하는 구조다 — C++ 예외를 실제 예외 타입
구분 없이 "실패 시 정리 후 즉시 빠져나가기"용 지역 제어 흐름으로만 사용한다.

## 7. 장단점

### 장점
- 템플릿 기반 타입→ODBC 속성 매핑 덕분에 호출부에서 `SQL_C_SLONG` 같은 상수를
  직접 나열할 필요가 없고, 타입 오기입으로 인한 바인딩 실수를 컴파일 타임에 줄인다.
- 파라미터/컬럼 속성 객체를 재사용해 바인딩마다 힙 할당이 없다.
- 모든 실패 지점에서 함수명 + 쿼리 텍스트 + SQLSTATE/에러코드/메시지를 일관된
  포맷으로 로깅해 장애 원인 추적이 쉽다.
- `AllSets()`/`BulkOperations()`로 행 단위 배치 Fetch와 북마크 기반 일괄 조작을
  지원해 대량 데이터 처리 경로를 따로 마련해 준다.
- Unicode/ANSI 빌드를 코드 중복 없이 `#ifdef` 분기로 함께 지원한다.

### 단점 / 트레이드오프
- `CDBParamAttrMgr`/`CDBColAttrMgr`가 각각 스크래치 객체 하나만 재사용하므로,
  한 커넥션(`CBaseODBC` 인스턴스)은 동시에 한 스레드만 사용해야 한다 — 이 제약은
  `COdbcConnPool`의 슬롯 단위 배타적 대여/반납 모델과 정확히 맞물려서 지켜진다.
- `Connect()`의 `throw 0` / `catch(...)` 패턴은 어느 단계에서 실패했는지에 대한
  타입 정보 없이 뭉뚱그려 처리한다 — 실패 원인은 그 지점에서 남긴 로그로만 구분 가능.
- 인덱스 자동 증가 오버로드(`++m_nParamNum`/`++m_nColNum`)와 인덱스 명시 오버로드가
  공존하므로, 한 문(statement) 안에서 두 방식을 섞어 쓰면 인덱스가 꼬일 수 있어
  스타일을 통일해서 사용해야 한다.
- `Fetch()`는 `m_bLoadExcelFile`일 때 에러 로그 자체를 남기지 않으므로, Excel
  파일 로딩 중 진짜 문제가 생겨도 조용히 넘어갈 수 있다.

## 8. 사용법

### 8.1 파라미터 바인딩 + 컬럼 바인딩으로 결과 순회

```cpp
// COdbcConnPool에서 빌린 커넥션(CBaseODBC)을 사용하는 일반적인 흐름
OdbcConnGuard guard(&pool);
if( guard == nullptr ) return false;

if( !guard->PrepareQuery(_T("SELECT name, age FROM users WHERE id = ?")) )
    return false;

int32 userId = 42;
guard->BindParamInput(userId);              // odbc_param_attr<int32> 자동 적용

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

- 파라미터/컬럼 인덱스를 생략하는 템플릿 오버로드(`BindParamInput(tValue)`,
  `BindCol(tValue)`)를 쓰면 `++m_nParamNum`/`++m_nColNum`이 자동으로 순서를
  매겨주므로, 바인딩 순서를 SQL의 `?` 순서/컬럼 순서와 맞추기만 하면 된다.
- `BindCol`은 실행 전에 한 번만 묶어두면 매 `Fetch()`마다 자동으로 값이 갱신된다.
  반대로 컬럼 구성이 매번 달라지거나 일부 컬럼만 조건부로 읽으면 되는 경우에는,
  사전 바인딩 없이 `Fetch()` 이후 그때그때 값을 읽는 `GetData`가 더 간단하다:

```cpp
while( guard->Fetch() )
{
    guard->GetData(1, tszName, nNameBuf);          // 문자열 전용 오버로드
    guard->GetData(2, age);                        // 템플릿 오버로드: odbc_col_attr<int64> 자동 적용
}
```

  다만 `GetData`는 `BindCol`과 달리 컬럼마다 매 `Fetch()` 이후 직접 호출해야 하므로,
  반복 횟수가 많은 루프에서는 `BindCol` 쪽이 호출 비용이 적다.

### 8.2 1회성 쿼리 — `ExecDirect`

파라미터 바인딩 없이 한 번만 실행하고 버릴 쿼리는 `PrepareQuery`+`Execute` 대신
`ExecDirect` 하나로 끝낸다.

```cpp
OdbcConnGuard guard(&pool);
if( guard == nullptr ) return false;

if( !guard->ExecDirect(_T("DELETE FROM session_cache WHERE expire_at < GETDATE()")) )
    return false;

int64 nDeleted = guard->RowCount(); // 영향받은 행 수
```

### 8.3 트랜잭션 — `Commit` / `Rollback`

여러 쿼리를 하나의 트랜잭션으로 묶을 때는 자동 커밋을 끄고, 각 단계 실패 시
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

guard->SetAutoCommitMode((SQLPOINTER)SQL_AUTOCOMMIT_ON); // 다음 대여자를 위해 원복
```

- `OdbcConnGuard`로 빌린 커넥션은 반납 후 다른 호출자가 재사용하므로, 자동 커밋
  모드를 바꿨다면 트랜잭션이 끝난 뒤 원래 모드로 되돌려 두는 것이 안전하다.

### 8.4 출력 파라미터 바인딩 (저장 프로시저 OUT 인자)

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

### 8.5 바이너리 데이터 바인딩

```cpp
guard->PrepareQuery(_T("UPDATE user_profile SET avatar = ? WHERE id = ?"));

BYTE avatarData[4096];
int32 nAvatarSize = LoadAvatarBytes(avatarData, sizeof(avatarData));
SQLLEN lRetSize = 0;

guard->BindParamInput(1, avatarData, nAvatarSize, lRetSize); // 크기에 따라 BINARY/LONGVARBINARY 자동 선택
guard->BindParamInput(2, userId);

guard->Execute();
```

- 크기가 `DATABASE_BINARY_MAX`를 넘으면 자동으로 `SQL_LONGVARBINARY`로 바인딩되므로,
  작은 썸네일이든 큰 첨부 파일이든 같은 호출 형태로 처리할 수 있다.
- `avatarData`가 `nullptr`이면 `lRetSize`가 `SQL_NULL_DATA`로 설정되어 NULL 값으로
  바인딩된다.

### 8.6 대량 결과 일괄 Fetch — `AllSets`

수천~수만 행을 한 번에 받아야 하는 배치 처리는 컬럼별 `BindCol` 반복 대신 행
단위 배열 바인딩을 쓴다. ODBC의 행 단위(row-wise) 바인딩은 각 컬럼의 데이터
포인터뿐 아니라 널/길이 지시자(indicator) 포인터도 **같은 보폭(stride)**으로
전진해야 하므로, 지시자 필드를 행 구조체 안에 함께 두는 것이 안전하다.

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
//    건너뛰며 데이터/지시자를 채워 넣는다 (데이터와 지시자 모두 같은 보폭 적용).
guard->BindCol(1, SQL_C_SLONG, &rows[0].nNo, sizeof(rows[0].nNo), &rows[0].indNo);
#ifdef _UNICODE
guard->BindCol(2, SQL_C_WCHAR, rows[0].tszName, sizeof(rows[0].tszName), &rows[0].indName);
#else
guard->BindCol(2, SQL_C_CHAR, rows[0].tszName, sizeof(rows[0].tszName), &rows[0].indName);
#endif

// 3. Fetch 한 번으로 최대 MAX_ROWS개까지 한꺼번에 채워진다.
while( guard->Fetch() )
{
    int32 nFetched = guard->GetFetchedRows(); // 이번 호출에서 실제로 채워진 행 수
    for( int32 i = 0; i < nFetched; ++i )
    {
        if( rows[i].indName == SQL_NULL_DATA )
            continue; // name이 NULL인 행

        // rows[i].nNo, rows[i].tszName 사용
    }
}
```

- `indNo`/`indName`처럼 지시자를 컬럼 옆에 나란히 두면, 드라이버가 데이터와
  지시자 양쪽 모두를 `sizeof(ConsumerRow)` 보폭으로 정확히 함께 전진시킨다.
  지시자를 행 구조체 밖의 별도 배열(`SQLLEN indNo[MAX_ROWS]`)로 두면 그 배열은
  기본적으로 `sizeof(SQLLEN)` 보폭으로 취급되어 행 단위 보폭과 어긋나므로 피한다.
- `AllSets()`는 `SQL_ATTR_ROWS_FETCHED_PTR`를 내부 `m_nFetchedRows`에 연결해두므로,
  매 `Fetch()` 이후 `GetFetchedRows()`로 이번에 몇 행이 채워졌는지 확인하고 그
  개수만큼만 순회해야 한다 (배열 전체가 항상 다 채워지는 것은 아님, 마지막 배치는
  더 적을 수 있음).
- 행 수가 많지 않거나(수십~수백 행) 코드 단순함이 더 중요하면, §8.7처럼 `BindCol`을
  한 행짜리 변수에 걸어두고 `Fetch()`를 반복하며 `std::vector`에 쌓는 방식이 더
  읽기 쉽다. `AllSets()` 방식은 그 반복마다의 함수 호출/커서 이동 오버헤드를
  줄이고 싶은, 진짜 대량(수천 행 이상) 처리에 적합하다.


### 8.7 여러 행을 한 번에 컨테이너에 담기

한 행씩 처리하지 않고, 조회 결과 전체를 리스트로 모아서 반환하는 흔한 패턴이다.
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
        result.push_back(row); // 현재 채워진 값을 복사해 쌓음
    }

    return result;
}
```

- 컨테이너에 쌓는 시점은 반드시 `Fetch()` 성공 직후여야 한다 — `row` 변수 자체는
  다음 `Fetch()` 호출에서 덮어써지므로, 참조나 포인터가 아니라 값 복사(`push_back`)로
  담아야 한다.
- 행 수가 아주 많고(수천 행 이상) 매 행마다 함수 호출 오버헤드를 줄이고 싶다면,
  §8.6의 `AllSets()` 기반 행 단위 배열 바인딩으로 여러 행을 한 번의 `Fetch()`
  호출로 가져오는 쪽이 더 적합하다.


