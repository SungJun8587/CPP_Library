
#ifndef UC_CHECKSUM_H
#define UC_CHECKSUM_H

#include <cstdint>
#include <cstddef>

//***************************************************************************
// @brief PNG 청크 검증에 사용하는 CRC32 체크섬 계산 클래스
// @details IEEE 802.3(zlib과 동일한) 다항식을 사용하는 표준 CRC32 구현이다.
//***************************************************************************
class Crc32
{
public:
	static uint32_t Compute(const uint8_t* data, size_t len);

private:
	static const uint32_t* Table();
};

//***************************************************************************
// @brief zlib 스트림 트레일러 검증에 사용하는 Adler32 체크섬 계산 클래스
//***************************************************************************
class Adler32
{
public:
	static uint32_t Compute(const uint8_t* data, size_t len);
};

#endif // ndef UC_CHECKSUM_H
