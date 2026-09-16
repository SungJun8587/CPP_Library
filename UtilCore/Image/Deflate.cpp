
#include "pch.h"
#include "Deflate.h"

//***************************************************************************
// @brief 비트 하나를 LSB-first 순서로 기록
// @param bit 기록할 비트 값(0 또는 1)
//***************************************************************************
void BitWriter::WriteBit(uint32_t bit)
{
	curByte_ |= (bit & 1) << bitPos_;
	if( ++bitPos_ == 8 ) { out_.push_back(curByte_); curByte_ = 0; bitPos_ = 0; }
}

//***************************************************************************
// @brief 여러 비트를 LSB-first 순서로 기록
// @param value 기록할 값
// @param count 기록할 비트 개수
//***************************************************************************
void BitWriter::WriteBits(uint32_t value, int count)
{
	for( int i = 0; i < count; ++i ) WriteBit((value >> i) & 1);
}

//***************************************************************************
// @brief 고정 허프만 코드처럼 MSB-first 순서로 정의된 값을 비트 단위로 기록
// @param value 기록할 값(MSB-first로 해석)
// @param count 기록할 비트 개수
//***************************************************************************
void BitWriter::WriteBitsMSBFirst(uint32_t value, int count)
{
	for( int i = count - 1; i >= 0; --i ) WriteBit((value >> i) & 1);
}

//***************************************************************************
// @brief 마지막으로 채우던 바이트가 남아 있으면 출력 버퍼에 밀어넣음
//***************************************************************************
void BitWriter::Flush() { if( bitPos_ > 0 ) { out_.push_back(curByte_); curByte_ = 0; bitPos_ = 0; } }

//***************************************************************************
// @brief 지금까지 기록된 바이트 버퍼를 소유권과 함께 반환
// @return 완성된 바이트 버퍼
//***************************************************************************
std::vector<uint8_t> BitWriter::TakeBuffer() { Flush(); return std::move(out_); }

//***************************************************************************
// @brief 데이터를 DEFLATE로 압축한 뒤 zlib 헤더/트레일러(Adler32)를 붙여 반환
// @param data 압축할 원본 데이터 시작 주소
// @param size 원본 데이터 길이(바이트)
// @return zlib 래퍼가 포함된 완전한 압축 스트림
//***************************************************************************
std::vector<uint8_t> Deflate::CompressZlib(const uint8_t* data, size_t size)
{
	std::vector<uint8_t> result;
	result.push_back(0x78);
	result.push_back(0x01);

	std::vector<uint8_t> compressed = CompressRaw(data, size);
	result.insert(result.end(), compressed.begin(), compressed.end());

	uint32_t adler = Adler32::Compute(data, size);
	result.push_back(static_cast<uint8_t>((adler >> 24) & 0xFF));
	result.push_back(static_cast<uint8_t>((adler >> 16) & 0xFF));
	result.push_back(static_cast<uint8_t>((adler >> 8) & 0xFF));
	result.push_back(static_cast<uint8_t>(adler & 0xFF));
	return result;
}

//***************************************************************************
// @brief 고정 허프만 리터럴/길이 심볼의 코드 비트 길이를 반환
// @param sym 리터럴/길이 심볼(0~285)
// @return 해당 심볼의 코드 길이(비트)
//***************************************************************************
int Deflate::LiteralCodeLength(int sym)
{
	if( sym <= 143 ) return 8;
	if( sym <= 255 ) return 9;
	if( sym <= 279 ) return 7;
	return 8;
}

//***************************************************************************
// @brief 고정 허프만 리터럴/길이 심볼의 코드 값을 반환
// @param sym 리터럴/길이 심볼(0~285)
// @return 해당 심볼의 고정 허프만 코드 값(MSB-first 해석)
//***************************************************************************
uint32_t Deflate::LiteralCode(int sym)
{
	if( sym <= 143 )  return 0x30 + sym;
	if( sym <= 255 )  return 0x190 + (sym - 144);
	if( sym <= 279 )  return 0x0 + (sym - 256);
	return 0xC0 + (sym - 280);
}

//***************************************************************************
// @brief 리터럴/길이 심볼 하나를 고정 허프만 코드로 비트스트림에 기록
// @param bw 비트를 기록할 BitWriter
// @param sym 기록할 리터럴/길이 심볼
//***************************************************************************
void Deflate::WriteSymbol(BitWriter& bw, int sym)
{
	bw.WriteBitsMSBFirst(LiteralCode(sym), LiteralCodeLength(sym));
}

//***************************************************************************
// @brief 매치 길이에 대응하는 DEFLATE 길이 코드와 추가 비트를 계산
// @param length 매치 길이(3~258)
// @param code 계산된 길이 코드(257~285) 출력
// @param extraBits 추가로 기록해야 할 비트 수 출력
// @param extraVal 추가 비트로 기록할 값 출력
//***************************************************************************
void Deflate::FindLengthCode(int length, int& code, int& extraBits, int& extraVal)
{
	static const int base[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,
								   67,83,99,115,131,163,195,227,258 };
	static const int extra[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
	for( int i = 28; i >= 0; --i )
	{
		if( length >= base[i] )
		{
			code = 257 + i;
			extraBits = extra[i];
			extraVal = length - base[i];
			return;
		}
	}
	code = 257; extraBits = 0; extraVal = 0;
}

//***************************************************************************
// @brief 매치 거리에 대응하는 DEFLATE 거리 코드와 추가 비트를 계산
// @param dist 매치 거리(1~32768)
// @param code 계산된 거리 코드(0~29) 출력
// @param extraBits 추가로 기록해야 할 비트 수 출력
// @param extraVal 추가 비트로 기록할 값 출력
//***************************************************************************
void Deflate::FindDistCode(int dist, int& code, int& extraBits, int& extraVal)
{
	static const int base[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,
								   1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
	static const int extra[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };
	for( int i = 29; i >= 0; --i )
	{
		if( dist >= base[i] )
		{
			code = i;
			extraBits = extra[i];
			extraVal = dist - base[i];
			return;
		}
	}
	code = 0; extraBits = 0; extraVal = 0;
}

//***************************************************************************
// @brief LZ77 매칭용 해시 체인에 사용할 3바이트 시퀀스 해시 키를 계산
// @param data 원본 데이터 시작 주소
// @param pos 해시를 계산할 위치
// @return 계산된 해시 키
//***************************************************************************
uint32_t Deflate::HashKey(const uint8_t* data, size_t pos)
{
	return (static_cast<uint32_t>(data[pos]) << 16) |
		(static_cast<uint32_t>(data[pos + 1]) << 8) |
		static_cast<uint32_t>(data[pos + 2]);
}

//***************************************************************************
// @brief 두 위치에서 시작하는 바이트 시퀀스가 일치하는 길이를 계산
// @param data 원본 데이터 시작 주소
// @param size 원본 데이터 전체 길이
// @param a 비교할 첫 번째 위치(이전 등장 위치)
// @param b 비교할 두 번째 위치(현재 위치)
// @return 일치하는 바이트 길이(최대 kMaxMatch)
//***************************************************************************
int Deflate::MatchLength(const uint8_t* data, size_t size, size_t a, size_t b)
{
	int len = 0;
	while( b + len < size && len < kMaxMatch && data[a + len] == data[b + len] ) ++len;
	return len;
}

//***************************************************************************
// @brief 해시 체인 기반 LZ77 매칭과 고정 허프만 코드로 단일 DEFLATE 블록을 생성
// @param data 압축할 원본 데이터 시작 주소
// @param size 원본 데이터 길이(바이트)
// @return 압축된 DEFLATE 원시 바이트 시퀀스(zlib 래퍼 미포함)
//***************************************************************************
std::vector<uint8_t> Deflate::CompressRaw(const uint8_t* data, size_t size)
{
	BitWriter bw;
	bw.WriteBit(1);
	bw.WriteBits(1, 2);

	std::unordered_map<uint32_t, std::vector<size_t>> chains;

	size_t i = 0;
	while( i < size )
	{
		int bestLen = 0;
		size_t bestPos = 0;

		if( i + kMinMatch <= size )
		{
			uint32_t key = HashKey(data, i);
			auto it = chains.find(key);
			if( it != chains.end() )
			{
				const auto& positions = it->second;
				int checked = 0;
				for( auto rit = positions.rbegin(); rit != positions.rend() && checked < 32; ++rit, ++checked )
				{
					size_t cand = *rit;
					if( i - cand > kWindowSize ) break;
					int len = MatchLength(data, size, cand, i);
					if( len > bestLen ) { bestLen = len; bestPos = cand; }
					if( bestLen >= kMaxMatch ) break;
				}
			}
		}

		if( bestLen >= kMinMatch )
		{
			int lenCode, lenExtraBits, lenExtraVal;
			FindLengthCode(bestLen, lenCode, lenExtraBits, lenExtraVal);
			WriteSymbol(bw, lenCode);
			bw.WriteBits(static_cast<uint32_t>(lenExtraVal), lenExtraBits);

			int dist = static_cast<int>(i - bestPos);
			int distCode, distExtraBits, distExtraVal;
			FindDistCode(dist, distCode, distExtraBits, distExtraVal);
			bw.WriteBitsMSBFirst(static_cast<uint32_t>(distCode), 5);
			bw.WriteBits(static_cast<uint32_t>(distExtraVal), distExtraBits);

			size_t end = i + bestLen;
			for( ; i < end && i + kMinMatch <= size; ++i )
			{
				chains[HashKey(data, i)].push_back(i);
			}
			i = end;
		}
		else
		{
			if( i + kMinMatch <= size ) chains[HashKey(data, i)].push_back(i);
			WriteSymbol(bw, data[i]);
			++i;
		}
	}

	WriteSymbol(bw, 256);
	return bw.TakeBuffer();
}