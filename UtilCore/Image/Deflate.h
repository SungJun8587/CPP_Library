
#ifndef UC_DEFLATE_H
#define UC_DEFLATE_H

#include <cstdint>
#include <vector>
#include "ImageTypes.h"

//***************************************************************************
// @brief DEFLATE 비트스트림에 LSB-first 순서로 비트를 기록하는 라이터
//***************************************************************************
class BitWriter
{
public:
	void WriteBit(uint32_t bit);
	void WriteBits(uint32_t value, int count);
	void WriteBitsMSBFirst(uint32_t value, int count);
	void Flush();
	std::vector<uint8_t> TakeBuffer();

private:
	std::vector<uint8_t> out_;  // 완성된 바이트를 누적하는 출력 버퍼
	uint8_t curByte_ = 0;       // 현재 채우고 있는 바이트
	int bitPos_ = 0;            // curByte_ 내에서 다음에 기록할 비트 위치
};

//***************************************************************************
// @brief LZ77 매칭 + 고정 허프만 코드로 DEFLATE/zlib 스트림을 생성하는
//        압축 유틸리티 클래스
// @details 동적 허프만 테이블 생성은 생략하여 zlib 대비 압축률은 낮지만,
//          표준을 따르는 유효한 스트림을 생성하며 외부 의존성이 없다.
//***************************************************************************
class Deflate
{
public:
	static std::vector<uint8_t> CompressZlib(const uint8_t* data, size_t size);

private:
	static constexpr int kMinMatch = 3;        // LZ77 매치로 인정할 최소 길이
	static constexpr int kMaxMatch = 258;       // DEFLATE 규격상 매치 최대 길이
	static constexpr int kWindowSize = 32768;   // LZ77 탐색 윈도우 크기(32K)

	static int LiteralCodeLength(int sym);
	static uint32_t LiteralCode(int sym);
	static void WriteSymbol(BitWriter& bw, int sym);
	static void FindLengthCode(int length, int& code, int& extraBits, int& extraVal);
	static void FindDistCode(int dist, int& code, int& extraBits, int& extraVal);
	static std::vector<uint8_t> CompressRaw(const uint8_t* data, size_t size);
	static uint32_t HashKey(const uint8_t* data, size_t pos);
	static int MatchLength(const uint8_t* data, size_t size, size_t a, size_t b);
};

#endif // ndef UC_DEFLATE_H
