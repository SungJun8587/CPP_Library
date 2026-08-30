
//***************************************************************************
// Endian.cpp : implementation of the Endian Functions.
//
//***************************************************************************

#include "pch.h"
#include "Endian.h"

#if (defined(HAVE_BIG_ENDIAN) && defined(HAVE_LITTLE_ENDIAN)) || (!defined(HAVE_BIG_ENDIAN) && !defined(HAVE_LITTLE_ENDIAN))
	#error define either HAVE_BIG_ENDIAN or HAVE_LITTLE_ENDIAN
#endif 

//***************************************************************************
//
uint8 HighByteFromBigEndian(const uint16& wData)
{
#ifdef HAVE_BIG_ENDIAN
	return HighByteFromHostEndian(wData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return LowByteFromHostEndian(wData);
#endif
}

//***************************************************************************
//
uint16 HighWordFromBigEndian(const uint32& dwData)
{
#ifdef HAVE_BIG_ENDIAN
	return HighWordFromHostEndian(dwData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return LowWordFromHostEndian(dwData);
#endif
}

//***************************************************************************
//
uint8 HighByteFromLittleEndian(const uint16& wData)
{
#ifdef HAVE_BIG_ENDIAN
	return LowByteFromHostEndian(wData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return HighByteFromHostEndian(wData);
#endif
}

//***************************************************************************
//
uint16 HighWordFromLittleEndian(const uint32& dwData)
{
#ifdef HAVE_BIG_ENDIAN
	return LowWordFromHostEndian(dwData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return HighWordFromHostEndian(dwData);
#endif
}

//***************************************************************************
//
uint8 LowByteFromBigEndian(const uint16& wData)
{
#ifdef HAVE_BIG_ENDIAN
	return LowByteFromHostEndian(wData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return HighByteFromHostEndian(wData);
#endif
}

//***************************************************************************
//
uint16 LowWordFromBigEndian(const uint32& dwData)
{
#ifdef HAVE_BIG_ENDIAN
	return LowWordFromHostEndian(dwData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return HighWordFromHostEndian(dwData);
#endif
}

//***************************************************************************
//
uint8 LowByteFromLittleEndian(const uint16& wData)
{
#ifdef HAVE_BIG_ENDIAN
	return HighByteFromHostEndian(wData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return LowByteFromHostEndian(wData);
#endif
}

//***************************************************************************
//
uint16 LowWordFromLittleEndian(const uint32& dwData)
{
#ifdef HAVE_BIG_ENDIAN
	return HighWordFromHostEndian(dwData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return LowWordFromHostEndian(dwData);
#endif
}

//***************************************************************************
//
uint16 BigEndianWord(const uint8& HighByte, const uint8& LowByte)
{
#ifdef HAVE_BIG_ENDIAN
	return HostEndianWord(HighByte, LowByte);
#elif defined(HAVE_LITTLE_ENDIAN)
	return (LowByte << 8) | HighByte;
#endif
}

//***************************************************************************
//
uint32 BigEndianDoubleWord(const uint16& HighWord, const uint16& LowWord)
{
#ifdef HAVE_BIG_ENDIAN
	return HostEndianDoubleWord(HighWord, LowWord);
#elif defined(HAVE_LITTLE_ENDIAN)
	return (LowWord << 16) | HighWord;
#endif
}

//***************************************************************************
//
uint16 LittleEndianWord(const uint8& HighByte, const uint8& LowByte)
{
#ifdef HAVE_BIG_ENDIAN
	return (LowByte << 8) | HighByte;
#elif defined(HAVE_LITTLE_ENDIAN)
	return HostEndianWord(HighByte, LowByte);
#endif
}

//***************************************************************************
//
uint32 LittleEndianDoubleWord(const uint16& HighWord, const uint16& LowWord)
{
#ifdef HAVE_BIG_ENDIAN
	return (LowWord << 16) | HighWord;
#elif defined(HAVE_LITTLE_ENDIAN)
	return HostEndianDoubleWord(HighWord, LowWord);
#endif
}

//***************************************************************************
//
uint16 BigEndianToHostEndian(const uint16 wData)
{
#ifdef HAVE_LITTLE_ENDIAN
	return ByteSwap2(wData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return wData;
#endif 
}

//***************************************************************************
//
uint32 BigEndianToHostEndian(const uint32 dwData)
{
#ifdef HAVE_LITTLE_ENDIAN
	return ByteSwap4(dwData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return dwData;
#endif 
}

//***************************************************************************
//
uint16 LittleEndianToHostEndian(const uint16 wData)
{
#ifdef HAVE_BIG_ENDIAN
	return ByteSwap2(wData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return wData;
#endif 
}

//***************************************************************************
//
uint32 LittleEndianToHostEndian(const uint32 dwData)
{
#ifdef HAVE_BIG_ENDIAN
	return ByteSwap4(dwData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return dwData;
#endif 
}

//***************************************************************************
//
uint16 HostEndianToBigEndian(const uint16 wData)
{
#ifdef HAVE_LITTLE_ENDIAN
	return ByteSwap2(wData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return wData;
#endif 
}

//***************************************************************************
//
uint32 HostEndianToBigEndian(const uint32 dwData)
{
#ifdef HAVE_LITTLE_ENDIAN
	return ByteSwap4(dwData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return dwData;
#endif 
}

//***************************************************************************
//
uint16 HostEndianToLittleEndian(const uint16 wData)
{
#ifdef HAVE_BIG_ENDIAN
	return ByteSwap2(wData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return wData;
#endif 
}

//***************************************************************************
//
uint32 HostEndianToLittleEndian(const uint32 dwData)
{
#ifdef HAVE_BIG_ENDIAN
	return ByteSwap4(dwData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return dwData;
#endif 
}









