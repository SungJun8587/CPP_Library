
#include "pch.h"
#include "Inflate.h"

//***************************************************************************
// @brief 비트 하나를 LSB-first 순서로 읽어들임
// @return 읽은 비트 값(0 또는 1)
//***************************************************************************
uint32_t BitReader::ReadBit()
{
	if( bitPos_ == 0 )
	{
		if( bytePos_ >= size_ ) throw ImageException("Inflate: unexpected end of stream");
		curByte_ = data_[bytePos_++];
	}
	uint32_t bit = (curByte_ >> bitPos_) & 1;
	bitPos_ = (bitPos_ + 1) & 7;
	return bit;
}

//***************************************************************************
// @brief 여러 비트를 LSB-first 순서로 읽어 하나의 정수 값으로 조합
// @param count 읽을 비트 개수
// @return 조합된 정수 값
//***************************************************************************
uint32_t BitReader::ReadBits(int count)
{
	uint32_t value = 0;
	for( int i = 0; i < count; ++i ) value |= (ReadBit() << i);
	return value;
}

//***************************************************************************
// @brief 바이트 경계로 정렬된 상태에서 원시 바이트 하나를 읽음
// @return 읽은 바이트 값
//***************************************************************************
uint8_t BitReader::ReadRawByte()
{
	if( bytePos_ >= size_ ) throw ImageException("Inflate: unexpected end of stream");
	return data_[bytePos_++];
}

//***************************************************************************
// @brief 코드 길이 배열로부터 정규 허프만 코드 테이블을 구성
// @param codeLengths 심볼별 코드 길이 배열(0이면 미사용 심볼)
//***************************************************************************
void HuffmanDecoder::Build(const std::vector<int>& codeLengths)
{
	int maxLen = 0;
	for( int len : codeLengths ) maxLen = std::max(maxLen, len);
	std::vector<int> blCount(maxLen + 1, 0);
	for( int len : codeLengths ) if( len > 0 ) blCount[len]++;

	std::vector<int> nextCode(maxLen + 1, 0);
	int code = 0;
	for( int bits = 1; bits <= maxLen; ++bits )
	{
		code = (code + blCount[bits - 1]) << 1;
		nextCode[bits] = code;
	}

	symbolOfCode_.clear();
	maxLen_ = maxLen;
	for( size_t sym = 0; sym < codeLengths.size(); ++sym )
	{
		int len = codeLengths[sym];
		if( len == 0 ) continue;
		int c = nextCode[len]++;
		symbolOfCode_[{len, c}] = static_cast<int>(sym);
	}
}

//***************************************************************************
// @brief 비트스트림에서 허프만 코드를 한 비트씩 읽어 심볼로 복호
// @param br 비트를 읽어올 BitReader
// @return 복호된 심볼 값
//***************************************************************************
int HuffmanDecoder::Decode(BitReader& br) const
{
	int code = 0;
	for( int len = 1; len <= maxLen_; ++len )
	{
		code = (code << 1) | br.ReadBit();
		auto it = symbolOfCode_.find({ len, code });
		if( it != symbolOfCode_.end() ) return it->second;
	}
	throw ImageException("Inflate: invalid huffman code");
}

//***************************************************************************
// @brief zlib 헤더를 건너뛴 뒤 DEFLATE 스트림 전체를 압축 해제
// @param zlibData zlib 래퍼가 포함된 압축 데이터 시작 주소
// @param size 압축 데이터 전체 길이(바이트)
// @return 압축 해제된 원본 바이트 시퀀스
//***************************************************************************
std::vector<uint8_t> Inflate::Decompress(const uint8_t* zlibData, size_t size)
{
	if( size < 2 ) throw ImageException("Inflate: input too small");
	const uint8_t* deflateData = zlibData + 2;
	size_t deflateSize = size - 2;

	std::vector<uint8_t> out;
	BitReader br(deflateData, deflateSize);

	bool finalBlock = false;
	while( !finalBlock )
	{
		finalBlock = br.ReadBit() != 0;
		uint32_t blockType = br.ReadBits(2);

		if( blockType == 0 )
		{
			DecodeStoredBlock(br, out);
		}
		else if( blockType == 1 )
		{
			HuffmanDecoder litDec, distDec;
			BuildFixedTables(litDec, distDec);
			DecodeCompressedBlock(br, litDec, distDec, out);
		}
		else if( blockType == 2 )
		{
			HuffmanDecoder litDec, distDec;
			BuildDynamicTables(br, litDec, distDec);
			DecodeCompressedBlock(br, litDec, distDec, out);
		}
		else
		{
			throw ImageException("Inflate: reserved block type");
		}
	}
	return out;
}

//***************************************************************************
// @brief BTYPE=00(stored, 무압축) 블록을 읽어 그대로 출력에 복사
// @param br 비트를 읽어올 BitReader
// @param out 결과를 누적할 출력 버퍼
//***************************************************************************
void Inflate::DecodeStoredBlock(BitReader& br, std::vector<uint8_t>& out)
{
	br.AlignToByte();
	uint8_t lenLo = br.ReadRawByte();
	uint8_t lenHi = br.ReadRawByte();
	br.ReadRawByte();
	br.ReadRawByte();
	uint32_t len = lenLo | (lenHi << 8);
	for( uint32_t i = 0; i < len; ++i ) out.push_back(br.ReadRawByte());
}

//***************************************************************************
// @brief RFC 1951에 정의된 고정(fixed) 허프만 리터럴/거리 테이블을 구성
// @param litDec 구성될 리터럴/길이 허프만 복호기
// @param distDec 구성될 거리 허프만 복호기
//***************************************************************************
void Inflate::BuildFixedTables(HuffmanDecoder& litDec, HuffmanDecoder& distDec)
{
	std::vector<int> litLengths(288);
	for( int i = 0; i <= 143; ++i ) litLengths[i] = 8;
	for( int i = 144; i <= 255; ++i ) litLengths[i] = 9;
	for( int i = 256; i <= 279; ++i ) litLengths[i] = 7;
	for( int i = 280; i <= 287; ++i ) litLengths[i] = 8;
	litDec.Build(litLengths);

	std::vector<int> distLengths(30, 5);
	distDec.Build(distLengths);
}

//***************************************************************************
// @brief BTYPE=10(dynamic-huffman) 블록 헤더를 읽어 리터럴/거리 테이블을 구성
// @param br 비트를 읽어올 BitReader
// @param litDec 구성될 리터럴/길이 허프만 복호기
// @param distDec 구성될 거리 허프만 복호기
//***************************************************************************
void Inflate::BuildDynamicTables(BitReader& br, HuffmanDecoder& litDec, HuffmanDecoder& distDec)
{
	int hlit = br.ReadBits(5) + 257;
	int hdist = br.ReadBits(5) + 1;
	int hclen = br.ReadBits(4) + 4;

	static const int order[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
	std::vector<int> clLengths(19, 0);
	for( int i = 0; i < hclen; ++i ) clLengths[order[i]] = br.ReadBits(3);

	HuffmanDecoder clDec;
	clDec.Build(clLengths);

	std::vector<int> allLengths;
	allLengths.reserve(hlit + hdist);
	while( static_cast<int>(allLengths.size()) < hlit + hdist )
	{
		int sym = clDec.Decode(br);
		if( sym <= 15 )
		{
			allLengths.push_back(sym);
		}
		else if( sym == 16 )
		{
			int repeat = br.ReadBits(2) + 3;
			int prev = allLengths.empty() ? 0 : allLengths.back();
			for( int i = 0; i < repeat; ++i ) allLengths.push_back(prev);
		}
		else if( sym == 17 )
		{
			int repeat = br.ReadBits(3) + 3;
			for( int i = 0; i < repeat; ++i ) allLengths.push_back(0);
		}
		else
		{
			int repeat = br.ReadBits(7) + 11;
			for( int i = 0; i < repeat; ++i ) allLengths.push_back(0);
		}
	}

	std::vector<int> litLengths(allLengths.begin(), allLengths.begin() + hlit);
	std::vector<int> distLengths(allLengths.begin() + hlit, allLengths.end());
	litDec.Build(litLengths);
	distDec.Build(distLengths);
}

//***************************************************************************
// @brief 허프만 블록(BTYPE 01 또는 10)의 리터럴/매치 심볼 시퀀스를 복호하여
//        출력 버퍼에 채움
// @param br 비트를 읽어올 BitReader
// @param litDec 리터럴/길이 허프만 복호기
// @param distDec 거리 허프만 복호기
// @param out 결과를 누적할 출력 버퍼
//***************************************************************************
void Inflate::DecodeCompressedBlock(BitReader& br, const HuffmanDecoder& litDec,
	const HuffmanDecoder& distDec, std::vector<uint8_t>& out)
{
	static const int lengthBase[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,
										67,83,99,115,131,163,195,227,258 };
	static const int lengthExtra[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
	static const int distBase[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,
									  1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
	static const int distExtra[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };

	while( true )
	{
		int sym = litDec.Decode(br);
		if( sym < 256 )
		{
			out.push_back(static_cast<uint8_t>(sym));
		}
		else if( sym == 256 )
		{
			break;
		}
		else
		{
			int idx = sym - 257;
			if( idx >= 29 ) throw ImageException("Inflate: invalid length code");
			int length = lengthBase[idx] + static_cast<int>(br.ReadBits(lengthExtra[idx]));

			int distSym = distDec.Decode(br);
			if( distSym >= 30 ) throw ImageException("Inflate: invalid distance code");
			int distance = distBase[distSym] + static_cast<int>(br.ReadBits(distExtra[distSym]));

			if( static_cast<size_t>(distance) > out.size() )
				throw ImageException("Inflate: distance exceeds output size");

			size_t start = out.size() - distance;
			for( int i = 0; i < length; ++i ) out.push_back(out[start + i]);
		}
	}
}