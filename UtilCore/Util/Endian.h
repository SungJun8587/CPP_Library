
//***************************************************************************
// Endian.h : interface for the Endian Functions.
//
//***************************************************************************

#ifndef __ENDIAN_H__
#define __ENDIAN_H__

#ifdef WIN32
	typedef signed __int8 int8_t;
	typedef signed __int16 int16_t;
	typedef signed __int32 int32_t;
	typedef signed __int64 int64_t;
	typedef unsigned __int8 uint8_t;
	typedef unsigned __int16 uint16_t;
	typedef unsigned __int32 uint32_t;
	typedef unsigned __int64 uint64_t;
#else // WIN32
	#include <stdint.h>
#endif // WIN32

#define HAVE_LITTLE_ENDIAN

//***************************************************************************
//
uint8_t HighByteFromBigEndian(const uint16_t& wData);
uint16_t HighWordFromBigEndian(const uint32_t& dwData);
uint8_t HighByteFromLittleEndian(const uint16_t& wData);
uint16_t HighWordFromLittleEndian(const uint32_t& dwData);

uint8_t LowByteFromBigEndian(const uint16_t& wData);
uint16_t LowWordFromBigEndian(const uint32_t& dwData);
uint8_t LowByteFromLittleEndian(const uint16_t& wData);
uint16_t LowWordFromLittleEndian(const uint32_t& dwData);

uint16_t BigEndianWord(const uint8_t& HighByte, const uint8_t& LowByte);
uint32_t BigEndianDoubleWord(const uint16_t& HighWord, const uint16_t& LowWord);
uint16_t LittleEndianWord(const uint8_t& HighByte, const uint8_t& LowByte);
uint32_t LittleEndianDoubleWord(const uint16_t& HighWord, const uint16_t& LowWord);

uint16_t BigEndianToHostEndian(const uint16_t wData);
uint32_t BigEndianToHostEndian(const uint32_t dwData);
uint16_t LittleEndianToHostEndian(const uint16_t wData);
uint32_t LittleEndianToHostEndian(const uint32_t dwData);

uint16_t HostEndianToBigEndian(const uint16_t wData);
uint32_t HostEndianToBigEndian(const uint32_t dwData);
uint16_t HostEndianToLittleEndian(const uint16_t wData);
uint32_t HostEndianToLittleEndian(const uint32_t dwData);

inline uint8_t HighByteFromHostEndian(const uint16_t& wData)
{
	return (wData >> 8);
}

inline uint16_t HighWordFromHostEndian(const uint32_t& dwData)
{
	return (dwData >> 16);
}

inline uint32_t HostEndianDoubleWord(const uint16_t& HighWord, const uint16_t& LowWord)
{
	return (HighWord << 16) | LowWord;
}

inline uint16_t HostEndianWord(const uint8_t& HighByte, const uint8_t& LowByte)
{
	return (HighByte << 8) | LowByte;
}

inline uint8_t LowByteFromHostEndian(const uint16_t& wData)
{
	return (wData & 0xFF);
}

inline uint16_t LowWordFromHostEndian(const uint32_t& dwData)
{
	return (dwData & 0xFFFF);
}

inline uint16_t ByteSwap2(const uint16_t InData)
{
	return (InData >> 8) | (InData << 8);
}

inline uint32_t ByteSwap4(const uint32_t InData)
{
	return ((InData >> 24) & 0x000000ff) |
		((InData >> 8) & 0x0000ff00) |
		((InData << 8) & 0x00ff0000) |
		((InData << 24) & 0xff000000);
}

inline uint64_t ByteSwap8(const uint64_t InData)
{
	return ((InData >> 56) & 0x00000000000000ff) |
		((InData >> 40) & 0x000000000000ff00) |
		((InData >> 24) & 0x0000000000ff0000) |
		((InData >> 8) & 0x00000000ff000000) |
		((InData << 8) & 0x000000ff00000000) |
		((InData << 24) & 0x0000ff0000000000) |
		((InData << 40) & 0x00ff000000000000) |
		((InData << 56) & 0xff00000000000000);
}

#endif // ndef __ENDIAN_H__

