class CDBError
{
public:
	TCHAR* operator()(const SQLSMALLINT nHandleType, const SQLHENV& hStatement, TCHAR* ptszMessage, TCHAR* ptszSQLState = NULL) const
	{
		SQLTCHAR		szSQLStateText[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		SQLTCHAR		szMessageText[SQL_MAX_MESSAGE_LENGTH] = { 0, };
		SQLINTEGER		nNativeError = 0;
		SQLSMALLINT		nTextLengh = 0;

		if( SQL_SUCCESS != ::SQLGetDiagRec(nHandleType, hStatement, 1, szSQLStateText, &nNativeError, szMessageText, SQL_MAX_MESSAGE_LENGTH, &nTextLengh) )
			return nullptr;

		if( ptszSQLState != NULL )
			_sntprintf_s(ptszSQLState, SQL_MAX_MESSAGE_LENGTH, _TRUNCATE, _T("%s"), szSQLStateText);

		_sntprintf_s(ptszMessage, SQL_MAX_MESSAGE_LENGTH, _TRUNCATE, _T("STATE[%s], ERROR[%ld], MSG[%s]"), szSQLStateText, nNativeError, szMessageText);

		return &ptszMessage[0];
	}
};
