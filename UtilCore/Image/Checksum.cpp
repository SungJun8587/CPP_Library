
#include "pch.h"
#include "Checksum.h"

//***************************************************************************
// @brief CRC32 계산에 사용할 256엔트리 룩업 테이블을 생성/반환(최초 호출 시 1회 생성)
// @return CRC32 룩업 테이블의 시작 주소
//***************************************************************************
const uint32_t* Crc32::Table()
{
	// 지역 static의 초기화는 C++11부터 스레드 안전함이 보장된다(최초 진입
	// 스레드가 초기화를 마칠 때까지 다른 스레드는 대기). 과거의 별도 bool
	// 플래그 수동 더블체크 패턴은 스레드 안전하지 않은 데이터 레이스였다
	// (ThreadSanitizer로 재현/확인됨 — 여러 스레드가 동시에 Crc32::Compute를
	// 처음 호출하는 경우 발생). 람다 1회 호출 초기화로 교체해 컴파일러가
	// 생성하는 가드를 활용한다.
	static const auto table = [] {
		std::array<uint32_t, 256> t{};
		for( uint32_t n = 0; n < 256; ++n )
		{
			uint32_t c = n;
			for( int k = 0; k < 8; ++k )
			{
				c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
			}
			t[n] = c;
		}
		return t;
		}();
	return table.data();
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