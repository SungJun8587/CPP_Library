
//***************************************************************************
// DBBind.h : interface for the Database Bind.
//
//***************************************************************************

#ifndef __DBBIND_H__
#define __DBBIND_H__

#pragma once

template<int32 C>
struct FullBits { enum { value = (1 << (C - 1)) | FullBits<C - 1>::value }; };

template<>
struct FullBits<1> { enum { value = 1 }; };

template<>
struct FullBits<0> { enum { value = 0 }; };

template<int32 ParamCount, int32 ColumnCount>
class CDBBind
{
public:
	CDBBind(CBaseODBC& dbConn, _tstring query)
		: _dbConn(dbConn), _query(query)
	{
		::memset(_paramIndex, 0, sizeof(_paramIndex));
		::memset(_columnIndex, 0, sizeof(_columnIndex));
		_paramFlag = 0;
		_columnFlag = 0;
		_dbConn.ClearStmt();
	}

	bool Validate()
	{
		return _paramFlag == FullBits<ParamCount>::value && _columnFlag == FullBits<ColumnCount>::value;
	}

	bool ExecDirect()
	{
		ASSERT_CRASH(Validate());
		return _dbConn.ExecDirect(_query.c_str());
	}

	bool Prepare()
	{
		return _dbConn.PrepareQuery(_query.c_str());
	}

	bool Execute()
	{
		ASSERT_CRASH(Validate());
		return _dbConn.Execute();
	}

	bool Fetch()
	{
		return _dbConn.Fetch();
	}

	int64 RowCount()
	{
		return _dbConn.RowCount();
	}

	_tstring GetSQLString()
	{
		return _query;
	}

public:
	template<typename T>
	void BindParam(int32 idx, T& value)
	{
		_dbConn.BindParamInput(idx + 1, value, _paramIndex[idx]);
		_paramFlag |= (1LL << idx);
	}

	void BindParam(int32 idx, const TCHAR* value)
	{
		_dbConn.BindParamInput(idx + 1, value, _paramIndex[idx]);
		_paramFlag |= (1LL << idx);
	}

	template<typename T, int32 N>
	void BindParam(int32 idx, T(&value)[N])
	{
		_dbConn.BindParamInput(idx + 1, (const BYTE*)value, size32(T) * N, _paramIndex[idx]);
		_paramFlag |= (1LL << idx);
	}

	template<typename T>
	void BindParam(int32 idx, T* value, int32 N)
	{
		_dbConn.BindParamInput(idx + 1, (const BYTE*)value, size32(T) * N, _paramIndex[idx]);
		_paramFlag |= (1LL << idx);
	}

	template<typename T>
	void BindCol(int32 idx, T& value)
	{
		_dbConn.BindCol(idx + 1, value, _columnIndex[idx]);
		_columnFlag |= (1LL << idx);
	}

	template<int32 N>
	void BindCol(int32 idx, TCHAR(&value)[N])
	{
		int32 iLength = N - 1;

		_dbConn.BindCol(idx + 1, value, iLength, _columnIndex[idx]);
		_columnFlag |= (1LL << idx);
	}

	void BindCol(int32 idx, TCHAR* value, int32 len)
	{
		int32 iLength = len - 1;

		_dbConn.BindCol(idx + 1, value, iLength, _columnIndex[idx]);
		_columnFlag |= (1LL << idx);
	}

	template<typename T, int32 N>
	void BindCol(int32 idx, T(&value)[N])
	{
		_dbConn.BindCol(idx + 1, value, size32(T) * N, _columnIndex[idx]);
		_columnFlag |= (1LL << idx);
	}

protected:
	CBaseODBC&		_dbConn;
	_tstring		_query;
	SQLLEN			_paramIndex[ParamCount > 0 ? ParamCount : 1];
	SQLLEN			_columnIndex[ColumnCount > 0 ? ColumnCount : 1];
	uint64			_paramFlag;
	uint64			_columnFlag;
};

#endif // ndef __DBBIND_H__