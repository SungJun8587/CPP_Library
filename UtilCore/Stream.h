
//***************************************************************************
// Stream.h : interface for the CStream class.
//
//***************************************************************************

#ifndef __STREAM_H__
#define __STREAM_H__

//***************************************************************************
//
class CStream
{
public:
	CStream(void);
	virtual ~CStream(void);

	BOOL SetBuffer(BYTE* pbBuffer);

	BOOL ReadInt32(INT* SrcData);
	BOOL ReadDWORD(DWORD* SrcData);
	BOOL ReadDWORD_PTR(DWORD_PTR* SrcData);
	BOOL ReadByte(BYTE* SrcData);
	BOOL ReadBytes(BYTE* SrcData, DWORD dwLength);
	BOOL ReadFloat(FLOAT* SrcData);
	BOOL ReadInt64(INT64* SrcData);
	BOOL ReadUSHORT(USHORT* SrcData);
	BOOL ReadSHORT(SHORT* SrcData);
	BOOL ReadBOOL(BOOL* SrcData);

	BOOL WriteInt32(const INT& SrcData);
	BOOL WriteDWORD(const DWORD& SrcData);
	BOOL WriteDWORD_PTR(const DWORD_PTR& SrcData);
	BOOL WriteByte(const BYTE& SrcData);
	BOOL WriteBytes(BYTE* SrcData, const DWORD& dwLength);
	BOOL WriteFloat(const FLOAT& SrcData);
	BOOL WriteInt64(const INT64& SrcData);
	BOOL WriteUSHORT(const USHORT& SrcData);
	BOOL WriteSHORT(const SHORT& SrcData);
	BOOL WriteBOOL(const BOOL& SrcData);

	DWORD GetLength();

private:
	BYTE* m_pbBuffer;
	int	m_nLength;
};

//***************************************************************************
//
class CStreamSP
{
public:
	CStreamSP()
	{
		m_pStream = new CStream();
	};
	~CStreamSP()
	{
		if( m_pStream )
		{
			delete m_pStream;
			m_pStream = NULL;
		}
	};

	CStream* operator->(VOID)
	{
		return m_pStream;
	}
	operator CStream*(VOID)
	{
		return m_pStream;
	}

private:
	CStream* m_pStream;
};

#endif // ndef __STREAM_H__

