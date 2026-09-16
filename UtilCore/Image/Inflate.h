
#ifndef UC_INFLATE_H
#define UC_INFLATE_H

#include <algorithm>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <utility>
#include "ImageTypes.h"

//***************************************************************************
// @brief DEFLATE 비트스트림에서 LSB-first 순서로 비트를 읽어들이는 리더
// @details DEFLATE 명세는 바이트 내에서 최하위 비트(LSB)부터 값을 구성하므로
//          일반적인 MSB-first 비트리더와 순서가 반대이다.
//***************************************************************************
class BitReader
{
public:
	BitReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

	uint32_t ReadBit();
	uint32_t ReadBits(int count);

	//***************************************************************************
	// @brief 다음 stored 블록을 읽기 위해 현재 바이트의 남은 비트를 버리고
	//        바이트 경계로 정렬
	//***************************************************************************
	void AlignToByte() { bitPos_ = 0; }

	uint8_t ReadRawByte();

	//***************************************************************************
	// @brief 현재까지 소비한 바이트 오프셋을 반환
	// @return 다음에 읽을 바이트의 인덱스
	//***************************************************************************
	size_t BytePos() const { return bytePos_; }

private:
	const uint8_t* data_;    // 입력 스트림 시작 주소
	size_t size_;            // 입력 스트림 전체 길이(바이트)
	size_t bytePos_ = 0;     // 다음에 읽을 바이트 인덱스
	uint8_t curByte_ = 0;    // 현재 처리 중인 바이트
	int bitPos_ = 0;         // curByte_ 내에서 다음에 읽을 비트 위치
};

//***************************************************************************
// @brief 정규(canonical) 허프만 코드 길이 배열로부터 심볼을 복원하는 복호기
// @details RFC 1951 3.2.2절의 정규 허프만 코드 생성 규칙을 그대로 따른다.
//***************************************************************************
class HuffmanDecoder
{
public:
	void Build(const std::vector<int>& codeLengths);
	int Decode(BitReader& br) const;

private:
	//***************************************************************************
	// @brief (코드 길이, 코드 값) 쌍을 해시하기 위한 해시 함수 객체
	//***************************************************************************
	struct KeyHash
	{
		//***************************************************************************
		// @brief (길이, 코드) 쌍에 대한 해시값을 계산
		// @param k (코드 길이, 코드 값) 쌍
		// @return 계산된 해시값
		//***************************************************************************
		size_t operator()(const std::pair<int, int>& k) const
		{
			return (static_cast<size_t>(k.first) << 20) ^ static_cast<size_t>(k.second);
		}
	};
	std::unordered_map<std::pair<int, int>, int, KeyHash> symbolOfCode_; // (길이,코드) -> 심볼
	int maxLen_ = 0; // 이 테이블에 등장하는 최대 코드 길이
};

//***************************************************************************
// @brief zlib/DEFLATE 스트림을 압축 해제하는 정적 유틸리티 클래스
// @details stored, fixed-huffman, dynamic-huffman 세 가지 블록 타입을 모두
//          지원하여 외부에서 생성된 표준 PNG도 읽을 수 있다.
//***************************************************************************
class Inflate
{
public:
	static std::vector<uint8_t> Decompress(const uint8_t* zlibData, size_t size);

private:
	static void DecodeStoredBlock(BitReader& br, std::vector<uint8_t>& out);
	static void BuildFixedTables(HuffmanDecoder& litDec, HuffmanDecoder& distDec);
	static void BuildDynamicTables(BitReader& br, HuffmanDecoder& litDec, HuffmanDecoder& distDec);
	static void DecodeCompressedBlock(BitReader& br, const HuffmanDecoder& litDec,
		const HuffmanDecoder& distDec, std::vector<uint8_t>& out);
};

#endif // ndef UC_INFLATE_H
