
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
uint8_t HighByteFromBigEndian(const uint16_t& wData)
{
#ifdef HAVE_BIG_ENDIAN
	return HighByteFromHostEndian(wData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return LowByteFromHostEndian(wData);
#endif
}

//***************************************************************************
//
uint16_t HighWordFromBigEndian(const uint32_t& dwData)
{
#ifdef HAVE_BIG_ENDIAN
	return HighWordFromHostEndian(dwData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return LowWordFromHostEndian(dwData);
#endif
}

//***************************************************************************
//
uint8_t HighByteFromLittleEndian(const uint16_t& wData)
{
#ifdef HAVE_BIG_ENDIAN
	return LowByteFromHostEndian(wData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return HighByteFromHostEndian(wData);
#endif
}

//***************************************************************************
//
uint16_t HighWordFromLittleEndian(const uint32_t& dwData)
{
#ifdef HAVE_BIG_ENDIAN
	return LowWordFromHostEndian(dwData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return HighWordFromHostEndian(dwData);
#endif
}

//***************************************************************************
//
uint8_t LowByteFromBigEndian(const uint16_t& wData)
{
#ifdef HAVE_BIG_ENDIAN
	return LowByteFromHostEndian(wData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return HighByteFromHostEndian(wData);
#endif
}

//***************************************************************************
//
uint16_t LowWordFromBigEndian(const uint32_t& dwData)
{
#ifdef HAVE_BIG_ENDIAN
	return LowWordFromHostEndian(dwData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return HighWordFromHostEndian(dwData);
#endif
}

//***************************************************************************
//
uint8_t LowByteFromLittleEndian(const uint16_t& wData)
{
#ifdef HAVE_BIG_ENDIAN
	return HighByteFromHostEndian(wData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return LowByteFromHostEndian(wData);
#endif
}

//***************************************************************************
//
uint16_t LowWordFromLittleEndian(const uint32_t& dwData)
{
#ifdef HAVE_BIG_ENDIAN
	return HighWordFromHostEndian(dwData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return LowWordFromHostEndian(dwData);
#endif
}

//***************************************************************************
//
uint16_t BigEndianWord(const uint8_t& HighByte, const uint8_t& LowByte)
{
#ifdef HAVE_BIG_ENDIAN
	return HostEndianWord(HighByte, LowByte);
#elif defined(HAVE_LITTLE_ENDIAN)
	return (LowByte << 8) | HighByte;
#endif
}

//***************************************************************************
//
uint32_t BigEndianDoubleWord(const uint16_t& HighWord, const uint16_t& LowWord)
{
#ifdef HAVE_BIG_ENDIAN
	return HostEndianDoubleWord(HighWord, LowWord);
#elif defined(HAVE_LITTLE_ENDIAN)
	return (LowWord << 16) | HighWord;
#endif
}

//***************************************************************************
//
uint16_t LittleEndianWord(const uint8_t& HighByte, const uint8_t& LowByte)
{
#ifdef HAVE_BIG_ENDIAN
	return (LowByte << 8) | HighByte;
#elif defined(HAVE_LITTLE_ENDIAN)
	return HostEndianWord(HighByte, LowByte);
#endif
}

//***************************************************************************
//
uint32_t LittleEndianDoubleWord(const uint16_t& HighWord, const uint16_t& LowWord)
{
#ifdef HAVE_BIG_ENDIAN
	return (LowWord << 16) | HighWord;
#elif defined(HAVE_LITTLE_ENDIAN)
	return HostEndianDoubleWord(HighWord, LowWord);
#endif
}

//***************************************************************************
//
uint16_t BigEndianToHostEndian(const uint16_t wData)
{
#ifdef HAVE_LITTLE_ENDIAN
	return ByteSwap2(wData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return wData;
#endif 
}

//***************************************************************************
//
uint32_t BigEndianToHostEndian(const uint32_t dwData)
{
#ifdef HAVE_LITTLE_ENDIAN
	return ByteSwap4(dwData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return dwData;
#endif 
}

//***************************************************************************
//
uint16_t LittleEndianToHostEndian(const uint16_t wData)
{
#ifdef HAVE_BIG_ENDIAN
	return ByteSwap2(wData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return wData;
#endif 
}

//***************************************************************************
//
uint32_t LittleEndianToHostEndian(const uint32_t dwData)
{
#ifdef HAVE_BIG_ENDIAN
	return ByteSwap4(dwData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return dwData;
#endif 
}

//***************************************************************************
//
uint16_t HostEndianToBigEndian(const uint16_t wData)
{
#ifdef HAVE_LITTLE_ENDIAN
	return ByteSwap2(wData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return wData;
#endif 
}

//***************************************************************************
//
uint32_t HostEndianToBigEndian(const uint32_t dwData)
{
#ifdef HAVE_LITTLE_ENDIAN
	return ByteSwap4(dwData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return dwData;
#endif 
}

//***************************************************************************
//
uint16_t HostEndianToLittleEndian(const uint16_t wData)
{
#ifdef HAVE_BIG_ENDIAN
	return ByteSwap2(wData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return wData;
#endif 
}

//***************************************************************************
//
uint32_t HostEndianToLittleEndian(const uint32_t dwData)
{
#ifdef HAVE_BIG_ENDIAN
	return ByteSwap4(dwData);
#elif defined(HAVE_LITTLE_ENDIAN)
	return dwData;
#endif 
}









