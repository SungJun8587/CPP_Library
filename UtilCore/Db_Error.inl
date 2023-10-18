class CDBError
{
public:
	TCHAR* operator()(const SQLSMALLINT nHandleType, const SQLHENV& hStatement, TCHAR* ptszMessage, TCHAR* ptszSQLState = NULL) const
	{
		SQLTCHAR		tszSQLStateText[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		SQLTCHAR		tszMessageText[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		SQLINTEGER		nNativeError = 0;
		SQLSMALLINT		nTextLengh = 0;
		SQLRETURN		errorRet = 0;

#ifdef _UNICODE
		errorRet = ::SQLGetDiagRecW(nHandleType, hStatement, 1, tszSQLStateText, OUT & nNativeError, tszMessageText, SQL_MAX_MESSAGE_LENGTH, OUT & nTextLengh);
#else
		errorRet = ::SQLGetDiagRec(nHandleType, hStatement, 1, tszSQLStateText, OUT & nNativeError, tszMessageText, SQL_MAX_MESSAGE_LENGTH, OUT & nTextLengh);
#endif

		if( ptszSQLState != NULL )
			_sntprintf_s(ptszSQLState, SQL_MAX_MESSAGE_LENGTH, _TRUNCATE, _T("%s"), tszSQLStateText);

		_sntprintf_s(ptszMessage, SQL_MAX_MESSAGE_LENGTH, _TRUNCATE, _T("STATE[%s], ERROR[%ld], MSG[%s]"), tszSQLStateText, nNativeError, tszMessageText);

		return &ptszMessage[0];
	}
};
