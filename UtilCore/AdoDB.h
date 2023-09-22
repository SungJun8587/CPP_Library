
//***************************************************************************
// AdoDB.h : interface for the CAdoDB class.
//
//***************************************************************************

#ifndef __ADODB_H__
#define __ADODB_H__

#define  ADO_COM_CREATE_ERROR		-3
#define  ADO_OPEN_ERROR				-2
#define  ADO_COINITIALIZE_ERROR     -1
#define  ADO_OPEN_COMMAND_CREATE	1

#pragma warning(push)
#pragma warning(disable:4146)
#import "c:\program files\common files\system\ado\msado15.dll" rename ("EOF","adoEOF") no_namespace
#pragma warning(pop) 

class CAdoDB
{
public:
	CAdoDB();
	virtual				~CAdoDB();

	int					DBConnect(LPCTSTR lptszConnstring, const int nTimeOut);
	void				ConCancel();
	void				ConRollbackTrans();
	void				ConCommitTrans();
	long				ConBeginTrans();
	void				ConClose();
	BOOL				GetDBCon();

	BOOL				Open(LPCTSTR lptszSourceBuf, const long lOption = -1);
	BOOL				Execute(LPCTSTR lptszSourceBuf, const long lOption = -1);
	BOOL				StoredProcedureExecute(LPCTSTR lptszStoredName, const long lOption);

	long				GetReturnValue();
	void				CreateReturnParamAppend();
	void				CreateArgParamAppend(_bstr_t bstrName, enum DataTypeEnum enumType, long lSize, _variant_t vt, BOOL bInOutCheck);

	void				RSClose();
	BOOL				IsEOF();
	BOOL				Next();
	BOOL				Prev();
	BOOL				First();
	BOOL				Last();
	void				GetRs(_variant_t x, _bstr_t& ret);
	void				GetRs(_variant_t x, _variant_t& ret);
	void				GetRs(_variant_t x, float& ret);
	void				GetRs(_variant_t x, long& ret);
	void				GetRs(_variant_t x, double& ret);

	_CommandPtr			GetCmdPointer();
	_RecordsetPtr		GetRecPointer();
	int					GetRecCount();
	int					GetFieldCount();
	void				GetFieldByIndex(_variant_t x, LPTSTR lptszValue, int nValueLen);
	void				GetFieldByName(LPCTSTR lptszFieldName, LPTSTR lptszValue, int nValueLen);
	void				GetFieldByName(LPCTSTR lptszFieldName, long& lFieldValue);

private:
	BOOL				ISRSCon();
	BOOL				ISOpen();
	BOOL				ISCommand();
	BOOL				ISConnect();

private:
	_RecordsetPtr		m_pRs;
	_CommandPtr			m_pCmd;
	_ConnectionPtr		m_pCon;
};

#endif // ndef __ADODB_H__