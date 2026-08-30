
//***************************************************************************
// Endian.h : interface for the Endian Functions.
//
//***************************************************************************

#ifndef UC_ENDIAN_H
#define UC_ENDIAN_H

#ifndef WIN32
	#include <stdint.h>
#endif

#define HAVE_LITTLE_ENDIAN

//***************************************************************************
//
uint8 HighByteFromBigEndian(const uint16& wData);
uint16 HighWordFromBigEndian(const uint32& dwData);
uint8 HighByteFromLittleEndian(const uint16& wData);
uint16 HighWordFromLittleEndian(const uint32& dwData);

uint8 LowByteFromBigEndian(const uint16& wData);
uint16 LowWordFromBigEndian(const uint32& dwData);
uint8 LowByteFromLittleEndian(const uint16& wData);
uint16 LowWordFromLittleEndian(const uint32& dwData);

uint16 BigEndianWord(const uint8& HighByte, const uint8& LowByte);
uint32 BigEndianDoubleWord(const uint16& HighWord, const uint16& LowWord);
uint16 LittleEndianWord(const uint8& HighByte, const uint8& LowByte);
uint32 LittleEndianDoubleWord(const uint16& HighWord, const uint16& LowWord);

uint16 BigEndianToHostEndian(const uint16 wData);
uint32 BigEndianToHostEndian(const uint32 dwData);
uint16 LittleEndianToHostEndian(const uint16 wData);
uint32 LittleEndianToHostEndian(const uint32 dwData);

uint16 HostEndianToBigEndian(const uint16 wData);
uint32 HostEndianToBigEndian(const uint32 dwData);
uint16 HostEndianToLittleEndian(const uint16 wData);
uint32 HostEndianToLittleEndian(const uint32 dwData);

inline uint8 HighByteFromHostEndian(const uint16& wData)
{
	return (wData >> 8);
}

inline uint16 HighWordFromHostEndian(const uint32& dwData)
{
	return (dwData >> 16);
}

inline uint32 HostEndianDoubleWord(const uint16& HighWord, const uint16& LowWord)
{
	return (HighWord << 16) | LowWord;
}

inline uint16 HostEndianWord(const uint8& HighByte, const uint8& LowByte)
{
	return (HighByte << 8) | LowByte;
}

inline uint8 LowByteFromHostEndian(const uint16& wData)
{
	return (wData & 0xFF);
}

inline uint16 LowWordFromHostEndian(const uint32& dwData)
{
	return (dwData & 0xFFFF);
}

inline uint16 ByteSwap2(const uint16 InData)
{
	return (InData >> 8) | (InData << 8);
}

inline uint32 ByteSwap4(const uint32 InData)
{
	return ((InData >> 24) & 0x000000ff) |
		((InData >> 8) & 0x0000ff00) |
		((InData << 8) & 0x00ff0000) |
		((InData << 24) & 0xff000000);
}

inline uint64 ByteSwap8(const uint64 InData)
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

#endif // ndef UC_ENDIAN_H

