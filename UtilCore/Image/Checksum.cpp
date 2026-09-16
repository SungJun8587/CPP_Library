
#include "pch.h"
#include "Checksum.h"

//***************************************************************************
// @brief CRC32 계산에 사용할 256엔트리 룩업 테이블을 생성/반환(최초 호출 시 1회 생성)
// @return CRC32 룩업 테이블의 시작 주소
//***************************************************************************
const uint32_t* Crc32::Table()
{
	static uint32_t table[256];
	static bool initialized = false;
	if( !initialized )
	{
		for( uint32_t n = 0; n < 256; ++n )
		{
			uint32_t c = n;
			for( int k = 0; k < 8; ++k )
			{
				c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
			}
			table[n] = c;
		}
		initialized = true;
	}
	return table;
}

//***************************************************************************
// @brief 데이터 블록의 CRC32 체크섬을 계산
// @param data 체크섬을 계산할 데이터 시작 주소
// @param len 데이터 길이(바이트)
// @return 계산된 CRC32 값
//***************************************************************************
uint32_t Crc32::Compute(const uint8_t* data, size_t len)
{
	const uint32_t* table = Table();
	uint32_t crc = 0xFFFFFFFFu;
	for( size_t i = 0; i < len; ++i )
	{
		crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
	}
	return crc ^ 0xFFFFFFFFu;
}

//***************************************************************************
// @brief 데이터 블록의 Adler32 체크섬을 계산
// @param data 체크섬을 계산할 데이터 시작 주소
// @param len 데이터 길이(바이트)
// @return 계산된 Adler32 값
//***************************************************************************
uint32_t Adler32::Compute(const uint8_t* data, size_t len)
{
	const uint32_t MOD_ADLER = 65521;
	uint32_t a = 1, b = 0;
	for( size_t i = 0; i < len; ++i )
	{
		a = (a + data[i]) % MOD_ADLER;
		b = (b + a) % MOD_ADLER;
	}
	return (b << 16) | a;
}
