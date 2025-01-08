
//***************************************************************************
// Stream.cpp : implementation of the CStream class.
//
//***************************************************************************

#include "pch.h"
#include "Stream.h"

//***************************************************************************
// Construction/Destruction 
//***************************************************************************

CStream::CStream()
{
	m_pbBuffer = NULL;
	m_nLength = 0;
}

CStream::~CStream()
{
}

//***************************************************************************
//
BOOL CStream::SetBuffer(BYTE* pbBuffer)
{
	if( !pbBuffer ) return FALSE;

	m_pbBuffer = pbBuffer;
	m_nLength = 0;

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::ReadInt32(INT* SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(SrcData, m_pbBuffer + m_nLength, sizeof(INT));

	m_nLength += sizeof(INT);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::ReadDWORD(DWORD* SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(SrcData, m_pbBuffer + m_nLength, sizeof(DWORD));

	m_nLength += sizeof(DWORD);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::ReadDWORD_PTR(DWORD_PTR* SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(SrcData, m_pbBuffer + m_nLength, sizeof(DWORD_PTR));

	m_nLength += sizeof(DWORD_PTR);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::ReadByte(BYTE* SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(SrcData, m_pbBuffer + m_nLength, sizeof(BYTE));

	m_nLength += sizeof(BYTE);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::ReadBytes(BYTE* SrcData, DWORD dwLength)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(SrcData, m_pbBuffer + m_nLength, dwLength);

	m_nLength += dwLength;

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::ReadFloat(FLOAT* SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(SrcData, m_pbBuffer + m_nLength, sizeof(FLOAT));

	m_nLength += sizeof(FLOAT);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::ReadInt64(INT64* SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(SrcData, m_pbBuffer + m_nLength, sizeof(INT64));

	m_nLength += sizeof(INT64);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::ReadSHORT(SHORT* SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(SrcData, m_pbBuffer + m_nLength, sizeof(SHORT));

	m_nLength += sizeof(SHORT);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::ReadUSHORT(USHORT* SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(SrcData, m_pbBuffer + m_nLength, sizeof(USHORT));

	m_nLength += sizeof(USHORT);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::ReadBOOL(BOOL* SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(SrcData, m_pbBuffer + m_nLength, sizeof(BOOL));

	m_nLength += sizeof(BOOL);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::WriteInt32(const INT& SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(m_pbBuffer + m_nLength, &SrcData, sizeof(INT));

	m_nLength += sizeof(INT);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::WriteDWORD(const DWORD& SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(m_pbBuffer + m_nLength, &SrcData, sizeof(DWORD));

	m_nLength += sizeof(DWORD);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::WriteDWORD_PTR(const DWORD_PTR& SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(m_pbBuffer + m_nLength, &SrcData, sizeof(DWORD_PTR));

	m_nLength += sizeof(DWORD_PTR);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::WriteByte(const BYTE& SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(m_pbBuffer + m_nLength, &SrcData, sizeof(BYTE));

	m_nLength += sizeof(BYTE);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::WriteBytes(BYTE* SrcData, const DWORD& dwLength)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(m_pbBuffer + m_nLength, SrcData, dwLength);

	m_nLength += dwLength;

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::WriteFloat(const FLOAT& SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(m_pbBuffer + m_nLength, &SrcData, sizeof(FLOAT));

	m_nLength += sizeof(FLOAT);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::WriteInt64(const INT64& SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(m_pbBuffer + m_nLength, &SrcData, sizeof(INT64));

	m_nLength += sizeof(INT64);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::WriteSHORT(const SHORT& SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(m_pbBuffer + m_nLength, &SrcData, sizeof(SHORT));

	m_nLength += sizeof(SHORT);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::WriteUSHORT(const USHORT& SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(m_pbBuffer + m_nLength, &SrcData, sizeof(USHORT));

	m_nLength += sizeof(USHORT);

	return TRUE;
}

//***************************************************************************
//
BOOL CStream::WriteBOOL(const BOOL& SrcData)
{
	// ���� �о� SrcData ������ ����
	CopyMemory(m_pbBuffer + m_nLength, &SrcData, sizeof(BOOL));

	m_nLength += sizeof(BOOL);

	return TRUE;
}

//***************************************************************************
//
DWORD CStream::GetLength()
{
	return m_nLength;
}