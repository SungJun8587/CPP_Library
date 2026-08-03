# CAdoDB 설계 문서

## 개요

`CAdoDB`는 MS ADO(ActiveX Data Objects, `msado15.dll`)를 감싼 C++ 래퍼 클래스이다. MSSQL / MySQL / Oracle 세 DBMS에 대해 동일한 인터페이스로 연결, 일반 쿼리 실행, 저장 프로시저 실행, Recordset 순회, Command 파라미터 바인딩, 서버 메타정보(호스트/제품명/버전/문자셋) 조회까지 하나의 클래스에서 담당하도록 설계되었다.

내부적으로 `_ConnectionPtr` / `_CommandPtr` / `_RecordsetPtr` 세 개의 ADO COM 스마트 포인터를 보유하며, 연결 시 `CoInitialize`, 소멸 시 `CoUninitialize`를 직접 호출해 COM 라이프사이클을 관리한다.

---

## 1. CAdoDB 클래스

### 1.1 멤버 변수

| 멤버 | 역할 |
|---|---|
| `m_DbClass` | 현재 연결 대상 DBMS 종류 (`EDBClass`: MSSQL / MYSQL / ORACLE) |
| `m_pRs` | `_RecordsetPtr` — ADO Recordset 스마트 포인터 |
| `m_pCmd` | `_CommandPtr` — ADO Command 스마트 포인터 |
| `m_pCon` | `_ConnectionPtr` — ADO Connection 스마트 포인터 |

### 1.2 매크로 상수

| 이름 | 값 | 의미 |
|---|---|---|
| `ADO_COM_CREATE_ERROR` | -3 | COM 객체 생성 실패 |
| `ADO_OPEN_ERROR` | -2 | 연결 열기 실패 |
| `ADO_COINITIALIZE_ERROR` | -1 | CoInitialize 실패 |
| `ADO_OPEN_COMMAND_CREATE` | 1 | 연결 및 Command/Recordset 생성 성공 |

### 1.3 멤버 함수 설명 (헤더 선언 순서)

#### 생성/소멸

| 함수 | 설명 |
|---|---|
| `CAdoDB()` | `CoInitialize(NULL)` 호출로 COM 라이브러리 초기화, `m_pRs`/`m_pCmd`/`m_pCon`을 `NULL`로 초기화. |
| `~CAdoDB()` | `ISOpen()`이면 `m_pCon->Close()`, `ISRSCon()`이면 `m_pRs->Close()`, `ISCommand()`이면 `m_pCmd->Cancel()`을 각각 호출해 정리한 뒤 `CoUninitialize()` 호출. |

#### 연결 관리

| 함수 | 반환값 | 설명 |
|---|---|---|
| `Connect(dbClass, lptszConnstring, nTimeOut)` | `int` | `Connection` COM 인스턴스를 생성하고 `ConnectionTimeout` 설정, `m_DbClass`에 DBMS 종류 저장 후 `m_pCon->Open(lptszConnstring, L"", L"", -1)` 실행. 성공 시 `Command`/`Recordset` 인스턴스를 생성하고 `ADO_OPEN_COMMAND_CREATE` 반환, 이어서 `GetHostInfo`/`GetDBMSName`/`GetServerVersion`/`GetCharacterSetName`을 호출해 네 값이 모두 조회되면 `LOG_DEBUG`로 서버 정보를 기록. 연결 실패 또는 예외 발생 시 `ADO_OPEN_ERROR` 반환. |
| `GetDBCon()` | `BOOL` | `ISOpen()` 결과를 그대로 반환. |
| `GetDBClass()` (헤더 인라인) | `EDBClass` | `m_DbClass` 반환. |
| `ConClose()` | `void` | `ISOpen()`이면 `m_pCon->Close()`. |
| `RSClose()` | `void` | `ISRSCon()`이면 `m_pRs->Close()`. |

#### 트랜잭션

| 함수 | 반환값 | 설명 |
|---|---|---|
| `ConBeginTrans()` | `long` | `m_pCon->BeginTrans()` 호출, 트랜잭션 중첩 레벨 반환. |
| `ConCommitTrans()` | `void` | `m_pCon->CommitTrans()` 호출. |
| `ConRollbackTrans()` | `void` | `m_pCon->RollbackTrans()` 호출. |
| `ConCancel()` | `void` | `m_pCon->Cancel()` 호출, 진행 중인 비동기 실행 취소. |

#### 쿼리/프로시저 실행

| 함수 | 반환값 | 설명 |
|---|---|---|
| `Open(lptszSourceBuf, lOption = -1)` | `BOOL` | `ISOpen()`일 때만 동작. `m_pRs`가 `NULL`이면 새 인스턴스 생성 후 `PutRefActiveConnection(m_pCon)`으로 바인딩, `CursorType = adOpenStatic` 설정 후 `m_pRs->Open(lptszSourceBuf, vtMissing, adOpenKeyset, adLockReadOnly, lOption)` 실행. |
| `Execute(lptszSourceBuf, lOption = -1)` | `BOOL` | `ISOpen()`일 때만 동작. `m_pCmd->CommandText` 설정, `PutRefActiveConnection(m_pCon)` 바인딩 후 `m_pCmd->Execute(NULL, NULL, adCmdText)` 결과를 `m_pRs`에 대입. |
| `StoredProcedureExecute(lptszStoredName, lOption)` | `BOOL` | `Execute()`와 동일한 흐름이나 `m_pCmd->Execute(NULL, NULL, adCmdStoredProc)`로 저장 프로시저 실행. |

#### Command 파라미터 관리

| 함수 | 반환값 | 설명 |
|---|---|---|
| `GetReturnValue()` | `long` | `m_pCmd->Parameters->Item[_variant_t("Return")]->Value` 반환 — 저장 프로시저 리턴 값 조회. |
| `CreateReturnParamAppend()` | `void` | 이름 `"Return"`, 타입 `adInteger`, 방향 `adParamReturnValue`, 크기 4인 파라미터를 생성해 `m_pCmd->Parameters`에 추가. |
| `CreateArgParamAppend(bstrName, enumType, lSize, vt, bInOutCheck)` | `void` | `bInOutCheck`가 참(Input)이면: `adVarChar && lSize==0`인 경우 빈 문자열이면 `VT_NULL` variant로, 아니면 문자열 길이를 크기로 생성 / `adInteger`면 크기 4로 고정 / 그 외는 전달받은 `lSize`·`vt` 그대로 사용. `bInOutCheck`가 거짓(Output)이면: `adInteger`면 크기 4, 그 외는 전달받은 `lSize`로 출력 파라미터 생성. 생성된 파라미터는 `Parameters->Append()`로 추가. |

#### Recordset 커서 제어

| 함수 | 반환값 | 설명 |
|---|---|---|
| `IsEOF()` | `BOOL` | `m_pRs->adoEOF` 반환. |
| `Next()` | `BOOL` | `MoveNext()` 실패 시 `FALSE`, 성공 시 `TRUE`. |
| `Prev()` | `BOOL` | `MovePrevious()` 실패 시 `FALSE`, 성공 시 `TRUE`. |
| `First()` | `BOOL` | `MoveFirst()` 실패 시 `FALSE`, 성공 시 `TRUE`. |
| `Last()` | `BOOL` | `MoveLast()` 실패 시 `FALSE`, 성공 시 `TRUE`. |

#### 결과 메타 정보 / 핸들 접근자

| 함수 | 반환값 | 설명 |
|---|---|---|
| `GetCmdPointer()` | `_CommandPtr` | `m_pCmd` 반환. |
| `GetRecPointer()` | `_RecordsetPtr` | `m_pRs` 반환. |
| `GetRecCount()` | `int` | `m_pRs->GetRecordCount()` 캐스팅 반환. |
| `GetFieldCount()` | `int` | `m_pRs->Fields->GetCount()` 캐스팅 반환. |

#### 필드 값 조회 — 인덱스 기반 (`GetFieldByIndex`, 오버로드 7종)

| 시그니처 | 설명 |
|---|---|
| `(x, LPTSTR lptszValue, int nValueLen)` | `m_pRs->Fields->Item[x]->Value` 조회 후 `_bstr_t` 경유해 `_tcscpy_s`로 복사. NULL/EMPTY면 빈 문자열. |
| `(x, long& lFieldValue)` | `VT_I2`는 `iVal`, `VT_I4`/`VT_INT`는 `lVal`, 그 외는 `static_cast<long>`. NULL이면 0. |
| `(x, int32& nFieldValue)` | 내부적으로 `long` 오버로드 호출 후 캐스팅. |
| `(x, ulong& ulFieldValue)` | `static_cast<ulong>(vData)`. NULL이면 0. |
| `(x, uint32& uFieldValue)` | 내부적으로 `ulong` 오버로드 호출 후 캐스팅. |
| `(x, double& dblFieldValue)` | `static_cast<double>(vData)`. NULL이면 0.0. |
| `(x, _tstring& strFieldValue)` | `UNICODE` 정의 시 `_bstr_t`를 그대로, 아니면 `const char*`로 변환. NULL이면 빈 문자열. |

`_variant_t x`는 인덱스뿐 아니라 필드명 문자열도 받을 수 있다(ADO `Fields->Item`이 variant를 받는 특성).

#### 필드 값 조회 — 이름 기반 (`GetFieldByName`, 오버로드 7종)

`m_pRs->GetCollect((_variant_t)lptszFieldName)`을 호출하는 것을 제외하면 `GetFieldByIndex`와 동일한 7종 시그니처 구성과 NULL 처리 방식을 그대로 따른다.

#### 필드 값 조회 — `GetRs` (Recordset 직접 참조, 오버로드 5종)

| 시그니처 | 설명 |
|---|---|
| `GetRs(x, _bstr_t& ret)` | `m_pRs->Fields->Item[x]->Value`를 `_bstr_t`로 대입. |
| `GetRs(x, _variant_t& ret)` | `_variant_t`로 그대로 대입. |
| `GetRs(x, float& ret)` | `float`로 대입. |
| `GetRs(x, long& ret)` | `long`으로 대입. |
| `GetRs(x, double& ret)` | `double`로 대입. |

> `GetFieldByIndex`/`GetFieldByName`과 달리 NULL/EMPTY 여부를 별도로 검사하지 않고 `_variant_t`의 대입 연산자에 그대로 위임하는 단순 접근자다.

#### 서버 메타정보 조회 (DBMS별 분기 쿼리)

네 함수 모두 `ISOpen()` 및 출력 버퍼 유효성을 먼저 검사하고, `m_DbClass`에 따라 쿼리문을 분기 구성한 뒤 `m_pCon->Execute(...)`로 실행해 결과 첫 컬럼(또는 MySQL의 `GetCharacterSetName`은 두 번째 컬럼)을 문자열로 변환해 출력 버퍼에 복사한다. 예외는 `catch(...)`로 흡수하고 기본값을 채운 뒤 `FALSE`를 반환한다.

| 함수 | 반환값 | MSSQL | MYSQL | ORACLE | 실패 시 기본값 |
|---|---|---|---|---|---|
| `GetHostInfo(out, nMaxLen)` | `BOOL` | `SELECT @@SERVERNAME` | `SELECT @@hostname` | `SELECT SYS_CONTEXT('USERENV','SERVER_HOST') FROM DUAL` | `"Localhost"` |
| `GetDBMSName(out, nMaxLen)` | `BOOL` | `SELECT 'Microsoft SQL Server'` | `SELECT 'MySQL'` | `SELECT 'Oracle'` | `"Unknown DBMS"` / `"Unknown"` |
| `GetServerVersion(out, nMaxLen)` | `BOOL` | `SELECT CONVERT(varchar(128), SERVERPROPERTY('ProductVersion'))` | `SELECT VERSION()` | `SELECT BANNER FROM V$VERSION WHERE ROWNUM = 1` | 없음 — 빈 문자열(`out[0]='\0'`) 상태로 `FALSE`만 반환 |
| `GetCharacterSetName(out, nMaxLen)` | `BOOL` | `SELECT CONVERT(varchar(128), SERVERPROPERTY('Collation'))` | `SHOW VARIABLES LIKE 'character_set_server'` (컬럼 인덱스 1 사용, 나머지 셋은 0) | `SELECT VALUE FROM NLS_DATABASE_PARAMETERS WHERE PARAMETER='NLS_CHARACTERSET'` | `"UTF-8"` |

#### 내부 상태 확인 (private)

| 함수 | 반환값 | 설명 |
|---|---|---|
| `ISConnect()` | `BOOL` | `m_pCon->GetState() != 0` 여부 반환. |
| `ISRSCon()` | `BOOL` | `m_pRs->GetState() != 0` 여부 반환. |
| `ISOpen()` | `BOOL` | `m_pCon->GetState() != 0` 여부 반환 (`ISConnect()`와 동일 로직). |
| `ISCommand()` | `BOOL` | `m_pCmd->GetState() != 0` 여부 반환. |

### 1.4 DBMS별 연결 문자열 형식 (`Connect()`의 `lptszConnstring` 인자)

`Connect()`는 연결 문자열 자체를 파싱하지 않고 `m_pCon->Open()`에 그대로 전달하므로, OLE DB 프로바이더가 요구하는 형식에 맞춰 호출부에서 구성해야 한다. `m_DbClass`로 넘기는 `EDBClass` 값과 연결 문자열의 프로바이더가 서로 일치해야 `GetHostInfo` 등에서 사용하는 DBMS별 분기 쿼리도 올바르게 동작한다.

| DBMS | 프로바이더 예시 | 연결 문자열 예시 |
|---|---|---|
| MSSQL | `SQLOLEDB` (레거시) / `MSOLEDBSQL` (신규 권장) | `Provider=MSOLEDBSQL;Server=127.0.0.1,1433;Database=game_db;Uid=sa;Pwd=password;` |
| MySQL | `MySQLProv` (MySQL Connector/ODBC OLE DB) | `Provider=MySQLProv;Data Source=game_db;Server=127.0.0.1;Port=3306;User Id=root;Password=password;` |
| Oracle | `OraOLEDB.Oracle` (Oracle 제공) / `MSDAORA` (레거시, MS 제공) | `Provider=OraOLEDB.Oracle;Data Source=ORCLPDB1;User Id=game;Password=password;` |

> 위 표의 프로바이더/키 이름은 설치된 OLE DB 드라이버 종류와 버전에 따라 달라질 수 있다 (예: MSSQL은 `SQLNCLI11` 등 SQL Native Client 계열도 사용 가능). `Connect()`를 호출하는 쪽에서 대상 환경에 설치된 프로바이더에 맞는 연결 문자열을 구성해 전달해야 한다.

---

## 2. 사용 예시

### 2.1 연결 + 일반 SELECT (Open 사용, Recordset 커서 직접 순회)

```cpp
CAdoDB db;

if( db.Connect(EDBClass::MSSQL, _T("Provider=SQLOLEDB;Data Source=...;"), 10) != ADO_OPEN_COMMAND_CREATE )
{
    // 연결 실패 처리
    return;
}

if( db.Open(_T("SELECT user_id, nickname FROM users WHERE level > 10")) )
{
    while( !db.IsEOF() )
    {
        int32   nUserId = 0;
        _tstring strNickname;

        db.GetFieldByIndex((long)0, nUserId);
        db.GetFieldByIndex((long)1, strNickname);

        // nUserId, strNickname 사용

        db.Next();
    }

    db.RSClose();
}
```

### 2.2 Execute + 필드명 기반 조회

```cpp
if( db.Execute(_T("SELECT order_id, amount FROM orders WHERE status = 'PENDING'")) )
{
    while( !db.IsEOF() )
    {
        long   lOrderId = 0;
        double dblAmount = 0.0;

        db.GetFieldByName(_T("order_id"), lOrderId);
        db.GetFieldByName(_T("amount"), dblAmount);

        db.Next();
    }
}
```

### 2.3 저장 프로시저 실행 + 파라미터 바인딩 + 리턴 값 조회

```cpp
db.CreateArgParamAppend(_bstr_t("@UserId"), adInteger, 4, _variant_t(1001), TRUE);
db.CreateArgParamAppend(_bstr_t("@Nickname"), adVarChar, 0, _variant_t(_T("hong")), TRUE);
db.CreateReturnParamAppend();

if( db.StoredProcedureExecute(_T("usp_UpdateUserNickname"), -1) )
{
    long lRet = db.GetReturnValue();
    // lRet으로 프로시저 처리 결과 판단
}
```

### 2.4 트랜잭션 처리

```cpp
db.ConBeginTrans();

if( db.Execute(_T("UPDATE accounts SET balance = balance - 100 WHERE id = 1"))
    && db.Execute(_T("UPDATE accounts SET balance = balance + 100 WHERE id = 2")) )
{
    db.ConCommitTrans();
}
else
{
    db.ConRollbackTrans();
}
```

---

## 3. 전체 구조 요약

```
CAdoDB
 ├─ 연결 관리: Connect / GetDBCon / GetDBClass / ConClose
 ├─ 트랜잭션: ConBeginTrans / ConCommitTrans / ConRollbackTrans / ConCancel
 ├─ 쿼리/프로시저 실행: Open / Execute / StoredProcedureExecute
 ├─ Command 파라미터: CreateReturnParamAppend / CreateArgParamAppend / GetReturnValue
 ├─ Recordset 커서: IsEOF / Next / Prev / First / Last / RSClose
 ├─ 결과 메타/핸들: GetRecCount / GetFieldCount / GetCmdPointer / GetRecPointer
 ├─ 필드 값 조회: GetFieldByIndex × 7 / GetFieldByName × 7 / GetRs × 5
 ├─ 서버 메타정보: GetHostInfo / GetDBMSName / GetServerVersion / GetCharacterSetName (DBMS별 분기)
 └─ 내부 상태 확인(private): ISConnect / ISRSCon / ISOpen / ISCommand
```
