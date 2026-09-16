
#include "pch.h"
#include "JpegCodec.h"

//***************************************************************************
// @brief 데이터가 JPEG SOI 마커(0xFFD8)로 시작하는지 확인
// @param data 확인할 데이터 시작 주소
// @param size 데이터 길이(바이트)
// @return JPEG로 판단되면 true
//***************************************************************************
bool JpegCodec::CanDecode(const uint8_t* data, size_t size) const
{
	return size >= 2 && data[0] == 0xFF && data[1] == 0xD8;
}

//***************************************************************************
// @brief JPEG 바이트 시퀀스를 디코드하여 ImageBuffer로 변환
// @param data JPEG 파일 데이터 시작 주소
// @param size JPEG 파일 데이터 길이(바이트)
// @return 디코드된 RGBA8 ImageBuffer
//***************************************************************************
ImageBuffer JpegCodec::Decode(const uint8_t* data, size_t size) const
{
	Decoder dec;
	return dec.Run(data, size);
}

//***************************************************************************
// @brief JPEG 인코딩은 지원하지 않으므로 항상 예외를 던짐
// @return (예외를 던지므로 반환하지 않음)
//***************************************************************************
std::vector<uint8_t> JpegCodec::Encode(const ImageBuffer&) const
{
	throw ImageException("JPEG: encoding not implemented (decode-only). PNG/BMP로 저장하세요.");
}

//***************************************************************************
// @brief DHT의 코드 길이별 개수와 심볼 목록으로부터 정규 허프만 룩업 테이블을 구성
// @param counts 코드 길이(1~16)별 심볼 개수 배열
// @param symbols 코드 길이 순으로 나열된 심볼 목록
//***************************************************************************
void JpegCodec::HuffTable::Build(const uint8_t counts[16], const std::vector<uint8_t>& symbols)
{
	lut.clear();
	int code = 0, k = 0;
	for( int len = 1; len <= 16; ++len )
	{
		for( int i = 0; i < counts[len - 1]; ++i )
		{
			lut[(static_cast<uint32_t>(len) << 16) | static_cast<uint32_t>(code)] = symbols[k++];
			++code;
		}
		code <<= 1;
	}
}

//***************************************************************************
// @brief 엔트로피 코딩 구간에서 0xFF 0x00 스터핑을 제거하며 비트 하나를 읽음
// @return 읽은 비트 값(0 또는 1). 마커에 도달하면 0을 반환
//***************************************************************************
int JpegCodec::BitStream::ReadBit()
{
	if( bitsLeft_ == 0 )
	{
		if( pos_ >= size_ ) return 0;
		uint8_t b = data_[pos_++];
		if( b == 0xFF )
		{
			if( pos_ < size_ && data_[pos_] == 0x00 )
			{
				++pos_;
			}
			else
			{
				--pos_;
				cur_ = 0;
				bitsLeft_ = 8;
				return 0;
			}
		}
		cur_ = b;
		bitsLeft_ = 8;
	}
	--bitsLeft_;
	return (cur_ >> bitsLeft_) & 1;
}

//***************************************************************************
// @brief 여러 비트를 MSB-first 순서로 읽어 하나의 정수 값으로 조합
// @param n 읽을 비트 개수
// @return 조합된 정수 값
//***************************************************************************
int JpegCodec::BitStream::ReadBits(int n)
{
	int v = 0;
	for( int i = 0; i < n; ++i ) v = (v << 1) | ReadBit();
	return v;
}

//***************************************************************************
// @brief 빅엔디안 2바이트 정수를 읽음(JPEG 마커 세그먼트는 빅엔디안)
// @param p 읽을 위치
// @return 읽은 16비트 정수
//***************************************************************************
uint16_t JpegCodec::Decoder::ReadU16(const uint8_t* p) { return (p[0] << 8) | p[1]; }

//***************************************************************************
// @brief 지정한 위치부터 다음 마커(RST 계열 제외, 스터핑 바이트 제외)를 탐색
// @param data 탐색할 데이터 시작 주소
// @param size 데이터 전체 길이
// @param from 탐색을 시작할 위치
// @return 발견한 마커의 오프셋(없으면 size)
//***************************************************************************
size_t JpegCodec::Decoder::FindNextMarker(const uint8_t* data, size_t size, size_t from)
{
	size_t i = from;
	while( i + 1 < size )
	{
		if( data[i] == 0xFF )
		{
			uint8_t m = data[i + 1];
			if( m != 0x00 && !(m >= 0xD0 && m <= 0xD7) ) return i;
		}
		++i;
	}
	return size;
}

//***************************************************************************
// @brief JPEG 마커들을 순회하며 파싱하고 각 스캔을 디코드하여 최종 이미지를 구성
// @param data JPEG 파일 데이터 시작 주소
// @param size JPEG 파일 데이터 길이(바이트)
// @return 디코드된 RGBA8 ImageBuffer
//***************************************************************************
ImageBuffer JpegCodec::Decoder::Run(const uint8_t* data, size_t size)
{
	if( size < 4 || data[0] != 0xFF || data[1] != 0xD8 )
		throw ImageException("JPEG: invalid SOI");
	size_t pos = 2;

	while( pos + 4 <= size )
	{
		if( data[pos] != 0xFF ) { ++pos; continue; }
		uint8_t marker = data[pos + 1];
		pos += 2;
		if( marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7) ) continue;
		if( marker == 0xD9 ) break;

		uint16_t segLen = ReadU16(data + pos);
		const uint8_t* seg = data + pos + 2;
		size_t segDataLen = segLen - 2;

		switch( marker )
		{
		case 0xDB: ParseDQT(seg, segDataLen); break;
		case 0xC0: ParseSOF0(seg, segDataLen); break;
		case 0xC2: throw ImageException("JPEG: progressive JPEG not supported");
		case 0xC4: ParseDHT(seg, segDataLen); break;
		case 0xDD: restartInterval_ = ReadU16(seg); break;
		case 0xDA: {
			size_t scanHeaderEnd = pos + segLen;
			size_t entropyStart = scanHeaderEnd;
			ParseSOS(seg, segDataLen);
			size_t entropyEnd = FindNextMarker(data, size, entropyStart);
			DecodeScan(data + entropyStart, entropyEnd - entropyStart);
			pos = entropyEnd;
			continue;
		}
		default: break;
		}
		pos += segDataLen;
	}

	if( width_ == 0 || height_ == 0 ) throw ImageException("JPEG: missing SOF");
	return ComposeImage();
}

//***************************************************************************
// @brief DQT 마커를 파싱하여 양자화 테이블을 지그재그 역순으로 저장
// @param seg DQT 세그먼트 데이터 시작 주소(길이 필드 제외)
// @param len DQT 세그먼트 데이터 길이(바이트)
//***************************************************************************
void JpegCodec::Decoder::ParseDQT(const uint8_t* seg, size_t len)
{
	size_t p = 0;
	while( p < len )
	{
		uint8_t pq_tq = seg[p++];
		int precision = pq_tq >> 4;
		int id = pq_tq & 0xF;
		for( int i = 0; i < 64; ++i )
		{
			quantTables_[id][kZigZag[i]] = precision ? ReadU16(seg + p + i * 2) : seg[p + i];
		}
		p += precision ? 128 : 64;
		quantSet_[id] = true;
	}
}

//***************************************************************************
// @brief SOF0(baseline) 마커를 파싱하여 이미지 크기와 컴포넌트 정보를 읽음
// @param seg SOF0 세그먼트 데이터 시작 주소(길이 필드 제외)
//***************************************************************************
void JpegCodec::Decoder::ParseSOF0(const uint8_t* seg, size_t)
{
	height_ = ReadU16(seg + 1);
	width_ = ReadU16(seg + 3);
	int numComps = seg[5];
	comps_.resize(numComps);
	size_t p = 6;
	for( int i = 0; i < numComps; ++i )
	{
		comps_[i].id = seg[p];
		comps_[i].hSamp = seg[p + 1] >> 4;
		comps_[i].vSamp = seg[p + 1] & 0xF;
		comps_[i].quantTableId = seg[p + 2];
		p += 3;
		maxH_ = std::max(maxH_, comps_[i].hSamp);
		maxV_ = std::max(maxV_, comps_[i].vSamp);
	}
}

//***************************************************************************
// @brief DHT 마커를 파싱하여 DC/AC 허프만 테이블을 구성
// @param seg DHT 세그먼트 데이터 시작 주소(길이 필드 제외)
// @param len DHT 세그먼트 데이터 길이(바이트)
//***************************************************************************
void JpegCodec::Decoder::ParseDHT(const uint8_t* seg, size_t len)
{
	size_t p = 0;
	while( p < len )
	{
		uint8_t tc_th = seg[p++];
		int tableClass = tc_th >> 4;
		int id = tc_th & 0xF;
		uint8_t counts[16];
		int total = 0;
		for( int i = 0; i < 16; ++i ) { counts[i] = seg[p + i]; total += counts[i]; }
		p += 16;
		std::vector<uint8_t> symbols(seg + p, seg + p + total);
		p += total;
		if( tableClass == 0 ) dcTables_[id].Build(counts, symbols);
		else acTables_[id].Build(counts, symbols);
	}
}

//***************************************************************************
// @brief SOS 마커를 파싱하여 이번 스캔에 참여하는 컴포넌트와 테이블 선택을 기록
// @param seg SOS 세그먼트 데이터 시작 주소(길이 필드 제외)
//***************************************************************************
void JpegCodec::Decoder::ParseSOS(const uint8_t* seg, size_t)
{
	int n = seg[0];
	scanCompOrder_.clear();
	size_t p = 1;
	for( int i = 0; i < n; ++i )
	{
		int compId = seg[p];
		int tableSel = seg[p + 1];
		p += 2;
		for( size_t c = 0; c < comps_.size(); ++c )
		{
			if( comps_[c].id == compId )
			{
				comps_[c].dcTableId = tableSel >> 4;
				comps_[c].acTableId = tableSel & 0xF;
				scanCompOrder_.push_back(static_cast<int>(c));
			}
		}
	}
}

//***************************************************************************
// @brief JPEG의 부호 있는 카테고리 값(Table F.1)을 복원
// @param value 허프만 이후 읽은 원시 비트 값
// @param numBits 카테고리(비트 수)
// @return 복원된 부호 있는 정수 값
//***************************************************************************
int JpegCodec::Decoder::Extend(int value, int numBits)
{
	if( numBits == 0 ) return 0;
	int vt = 1 << (numBits - 1);
	return (value < vt) ? (value - (1 << numBits) + 1) : value;
}

//***************************************************************************
// @brief 비트스트림에서 허프만 코드를 한 비트씩 읽어 심볼로 복호
// @param bs 비트를 읽어올 BitStream
// @param table 사용할 허프만 테이블
// @return 복호된 심볼 값
//***************************************************************************
int JpegCodec::Decoder::DecodeHuffSymbol(BitStream& bs, const HuffTable& table)
{
	int code = 0;
	for( int len = 1; len <= 16; ++len )
	{
		code = (code << 1) | bs.ReadBit();
		auto it = table.lut.find((static_cast<uint32_t>(len) << 16) | static_cast<uint32_t>(code));
		if( it != table.lut.end() ) return it->second;
	}
	throw ImageException("JPEG: invalid huffman code in entropy stream");
}

//***************************************************************************
// @brief 하나의 8x8 블록(DC 1개 + AC 63개 계수)을 허프만 복호하여 채움
// @param bs 비트를 읽어올 BitStream
// @param comp 이 블록이 속한 컴포넌트(DC 예측값 갱신)
// @param block 지그재그가 아닌 자연 순서로 채워질 64개 계수 출력 버퍼
//***************************************************************************
void JpegCodec::Decoder::DecodeBlock(BitStream& bs, Component& comp, int block[64])
{
	std::memset(block, 0, sizeof(int) * 64);
	const HuffTable& dcTab = dcTables_[comp.dcTableId];
	const HuffTable& acTab = acTables_[comp.acTableId];

	int s = DecodeHuffSymbol(bs, dcTab);
	int diff = s ? Extend(bs.ReadBits(s), s) : 0;
	comp.dcPred += diff;
	block[0] = comp.dcPred;

	int k = 1;
	while( k < 64 )
	{
		int rs = DecodeHuffSymbol(bs, acTab);
		int run = rs >> 4, size = rs & 0xF;
		if( size == 0 )
		{
			if( run == 15 ) { k += 16; continue; }
			break;
		}
		k += run;
		if( k >= 64 ) break;
		block[kZigZag[k]] = Extend(bs.ReadBits(size), size);
		++k;
	}
}

//***************************************************************************
// @brief 역양자화된 8x8 DCT 계수 블록에 2D IDCT를 적용하여 픽셀로 복원
// @param in 역양자화된 64개 DCT 계수(자연 순서)
// @param out 복원된 64개 픽셀 값(0..255, 레벨시프트 완료) 출력 버퍼
//***************************************************************************
void JpegCodec::Decoder::IDCT8x8(const float in[64], uint8_t out[64])
{
	static float cosTable[8][8];
	static bool init = false;
	if( !init )
	{
		for( int x = 0; x < 8; ++x )
			for( int u = 0; u < 8; ++u )
				cosTable[x][u] = std::cos((2 * x + 1) * u * 3.14159265358979f / 16.0f);
		init = true;
	}
	float tmp[64];
	for( int y = 0; y < 8; ++y )
	{
		for( int x = 0; x < 8; ++x )
		{
			float sum = 0.0f;
			for( int u = 0; u < 8; ++u )
			{
				float cu = (u == 0) ? 0.70710678f : 1.0f;
				sum += cu * in[y * 8 + u] * cosTable[x][u];
			}
			tmp[y * 8 + x] = sum * 0.5f;
		}
	}
	for( int x = 0; x < 8; ++x )
	{
		for( int y = 0; y < 8; ++y )
		{
			float sum = 0.0f;
			for( int v = 0; v < 8; ++v )
			{
				float cv = (v == 0) ? 0.70710678f : 1.0f;
				sum += cv * tmp[v * 8 + x] * cosTable[y][v];
			}
			float val = sum * 0.5f + 128.0f;
			out[y * 8 + x] = static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, std::round(val))));
		}
	}
}

//***************************************************************************
// @brief 하나의 스캔 전체(모든 MCU)를 엔트로피 디코드하여 각 컴포넌트 평면을 채움
// @param entropyData 이번 스캔의 엔트로피 코딩 데이터 시작 주소
// @param entropyLen 엔트로피 코딩 데이터 길이(바이트)
//***************************************************************************
void JpegCodec::Decoder::DecodeScan(const uint8_t* entropyData, size_t entropyLen)
{
	int mcuW = 8 * maxH_, mcuH = 8 * maxV_;
	int mcusPerRow = (width_ + mcuW - 1) / mcuW;
	int mcusPerCol = (height_ + mcuH - 1) / mcuH;

	for( auto& c : comps_ )
	{
		c.width = mcusPerRow * c.hSamp * 8;
		c.height = mcusPerCol * c.vSamp * 8;
		c.plane.assign(static_cast<size_t>(c.width) * c.height, 0);
		c.dcPred = 0;
	}

	BitStream bs(entropyData, entropyLen);
	int mcuCount = 0;
	int block[64];
	float deq[64];

	for( int my = 0; my < mcusPerCol; ++my )
	{
		for( int mx = 0; mx < mcusPerRow; ++mx )
		{
			for( int ci : scanCompOrder_ )
			{
				Component& c = comps_[ci];
				for( int by = 0; by < c.vSamp; ++by )
				{
					for( int bx = 0; bx < c.hSamp; ++bx )
					{
						DecodeBlock(bs, c, block);
						const auto& q = quantTables_[c.quantTableId];
						for( int i = 0; i < 64; ++i ) deq[i] = static_cast<float>(block[i] * q[i]);
						uint8_t pixels[64];
						IDCT8x8(deq, pixels);

						int originX = (mx * c.hSamp + bx) * 8;
						int originY = (my * c.vSamp + by) * 8;
						for( int yy = 0; yy < 8; ++yy )
						{
							uint8_t* dst = &c.plane[static_cast<size_t>(originY + yy) * c.width + originX];
							std::memcpy(dst, &pixels[yy * 8], 8);
						}
					}
				}
			}
			++mcuCount;
			if( restartInterval_ && mcuCount % restartInterval_ == 0 && !(my == mcusPerCol - 1 && mx == mcusPerRow - 1) )
			{
				bs.ResetToByteBoundary();
				for( auto& c : comps_ ) c.dcPred = 0;
				size_t p = bs.Pos();
				while( p + 1 < entropyLen && !(entropyData[p] == 0xFF && entropyData[p + 1] >= 0xD0 && entropyData[p + 1] <= 0xD7) ) ++p;
				if( p + 1 < entropyLen ) p += 2;
				bs.SetPos(p);
			}
		}
	}
}

//***************************************************************************
// @brief 디코드된 컴포넌트 평면들을 업샘플링/색공간 변환하여 최종 RGBA8 이미지로 조합
// @return 완성된 RGBA8 ImageBuffer
//***************************************************************************
ImageBuffer JpegCodec::Decoder::ComposeImage()
{
	ImageBuffer image(width_, height_);
	bool isGray = (comps_.size() == 1);

	for( int y = 0; y < height_; ++y )
	{
		for( int x = 0; x < width_; ++x )
		{
			if( isGray )
			{
				uint8_t Y = SamplePlane(comps_[0], x, y);
				image.SetPixel(x, y, Y, Y, Y, 255);
			}
			else
			{
				uint8_t Y = SamplePlane(comps_[0], x, y);
				uint8_t Cb = SamplePlane(comps_[1], x, y);
				uint8_t Cr = SamplePlane(comps_[2], x, y);
				int r = static_cast<int>(Y + 1.402f * (Cr - 128));
				int g = static_cast<int>(Y - 0.344136f * (Cb - 128) - 0.714136f * (Cr - 128));
				int b = static_cast<int>(Y + 1.772f * (Cb - 128));
				image.SetPixel(x, y, Clamp(r), Clamp(g), Clamp(b), 255);
			}
		}
	}
	return image;
}

//***************************************************************************
// @brief 크로마 서브샘플링을 고려해 컴포넌트 평면의 (x, y) 위치를 최근접 이웃으로 샘플링
// @param c 샘플링할 컴포넌트
// @param x 전체 이미지 기준 가로 좌표
// @param y 전체 이미지 기준 세로 좌표
// @return 샘플링된 값(0..255)
//***************************************************************************
uint8_t JpegCodec::Decoder::SamplePlane(const Component& c, int x, int y) const
{
	int sx = x * c.hSamp / maxH_;
	int sy = y * c.vSamp / maxV_;
	return c.plane[static_cast<size_t>(sy) * c.width + sx];
}

//***************************************************************************
// @brief 정수 값을 0~255 범위로 클램프
// @param v 클램프할 값
// @return 클램프된 8비트 값
//***************************************************************************
uint8_t JpegCodec::Decoder::Clamp(int v) { return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); }
