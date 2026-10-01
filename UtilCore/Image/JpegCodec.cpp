
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
// @brief 기본 품질(85)로 ImageBuffer를 baseline JPEG 바이트 시퀀스로 인코드
// @param image 인코드할 원본 이미지
// @return 완성된 JPEG 파일 바이트 시퀀스
//***************************************************************************
std::vector<uint8_t> JpegCodec::Encode(const ImageBuffer& image) const
{
	return EncodeQuality(image, 85);
}

//***************************************************************************
// @brief 지정한 품질로 ImageBuffer를 baseline JPEG 바이트 시퀀스로 인코드
// @param image 인코드할 원본 이미지
// @param quality 압축 품질(1~100, 클수록 고화질/저압축)
// @return 완성된 JPEG 파일 바이트 시퀀스
//***************************************************************************
std::vector<uint8_t> JpegCodec::EncodeQuality(const ImageBuffer& image, int quality) const
{
	Encoder enc;
	return enc.Run(image, quality);
}

//***************************************************************************
// @brief DHT의 코드 길이별 개수와 심볼 목록으로부터 정규 허프만 룩업 테이블을 구성
// @param counts 코드 길이(1~16)별 심볼 개수 배열
// @param symbols 코드 길이 순으로 나열된 심볼 목록
//***************************************************************************
void JpegCodec::HuffTable::Build(const uint8_t counts[16], const std::vector<uint8_t>& symbols)
{
	values_ = symbols;
	int code = 0, k = 0;
	for( int len = 1; len <= 16; ++len )
	{
		if( counts[len - 1] == 0 )
		{
			maxCode[len] = -1;
		}
		else
		{
			valPtr[len] = k;
			minCode[len] = code;
			code += counts[len - 1];
			maxCode[len] = code - 1;
			k += counts[len - 1];
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

		if( pos + 2 > size ) throw ImageException("JPEG: truncated marker segment length");
		uint16_t segLen = ReadU16(data + pos);
		if( segLen < 2 ) throw ImageException("JPEG: invalid marker segment length");
		if( pos + segLen > size ) throw ImageException("JPEG: marker segment exceeds buffer");

		const uint8_t* seg = data + pos + 2;
		size_t segDataLen = static_cast<size_t>(segLen) - 2;

		switch( marker )
		{
		case 0xDB: ParseDQT(seg, segDataLen); break;
		case 0xC0: ParseSOF0(seg, segDataLen); break;
		case 0xC2: throw ImageException("JPEG: progressive JPEG not supported");
		case 0xC4: ParseDHT(seg, segDataLen); break;
		case 0xDD:
			if( segDataLen < 2 ) throw ImageException("JPEG: truncated DRI segment");
			restartInterval_ = ReadU16(seg);
			break;
		case 0xDA: {
			size_t scanHeaderEnd = pos + segLen;
			size_t entropyStart = scanHeaderEnd;
			ParseSOS(seg, segDataLen);
			size_t entropyEnd = FindNextMarker(data, size, entropyStart);
			DecodeScan(data + entropyStart, entropyEnd - entropyStart);
			hasScanData_ = true;
			pos = entropyEnd;
			continue;
		}
		default: break;
		}
		pos += segLen; // segLen은 길이 필드 자신(2바이트)을 포함하므로 segDataLen이 아닌 segLen만큼 전진해야 한다
	}

	if( width_ == 0 || height_ == 0 ) throw ImageException("JPEG: missing SOF");
	if( !hasScanData_ ) throw ImageException("JPEG: missing scan data (no SOS)");
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
		if( p + 1 > len ) throw ImageException("JPEG: truncated DQT segment");
		uint8_t pq_tq = seg[p++];
		int precision = pq_tq >> 4;
		int id = pq_tq & 0xF;
		if( id >= 4 ) throw ImageException("JPEG: quantization table id out of range");
		size_t tableBytes = precision ? 128 : 64;
		if( p + tableBytes > len ) throw ImageException("JPEG: truncated DQT table data");
		for( int i = 0; i < 64; ++i )
		{
			quantTables_[id][kZigZag[i]] = precision ? ReadU16(seg + p + i * 2) : seg[p + i];
		}
		p += tableBytes;
		quantSet_[id] = true;
	}
}

//***************************************************************************
// @brief SOF0(baseline) 마커를 파싱하여 이미지 크기와 컴포넌트 정보를 읽음
// @param seg SOF0 세그먼트 데이터 시작 주소(길이 필드 제외)
//***************************************************************************
void JpegCodec::Decoder::ParseSOF0(const uint8_t* seg, size_t len)
{
	if( len < 6 ) throw ImageException("JPEG: truncated SOF0 segment");
	height_ = ReadU16(seg + 1);
	width_ = ReadU16(seg + 3);
	// 가로*세로가 비정상적으로 큰 값(손상/조작된 헤더)이면 거대 할당 시도로 인한
	// 메모리 고갈(OOM)을 막기 위해 여기서 미리 거부한다.
	if( static_cast<uint64_t>(width_) * static_cast<uint64_t>(height_) > 100'000'000ULL )
		throw ImageException("JPEG: image dimensions too large");

	int numComps = seg[5];
	if( len < static_cast<size_t>(6) + static_cast<size_t>(numComps) * 3 )
		throw ImageException("JPEG: truncated SOF0 component data");
	if( numComps != 1 && numComps != 3 )
		throw ImageException("JPEG: unsupported component count (only 1 or 3 supported)");

	comps_.resize(numComps);
	size_t p = 6;
	for( int i = 0; i < numComps; ++i )
	{
		comps_[i].id = seg[p];
		comps_[i].hSamp = seg[p + 1] >> 4;
		comps_[i].vSamp = seg[p + 1] & 0xF;
		comps_[i].quantTableId = seg[p + 2];
		if( comps_[i].quantTableId >= 4 ) throw ImageException("JPEG: quantization table id out of range");
		if( comps_[i].hSamp == 0 || comps_[i].vSamp == 0 )
			throw ImageException("JPEG: invalid sampling factor");
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
		if( p + 17 > len ) throw ImageException("JPEG: truncated DHT segment header");
		uint8_t tc_th = seg[p];
		int tableClass = tc_th >> 4;
		int id = tc_th & 0xF;
		if( id >= 4 ) throw ImageException("JPEG: huffman table id out of range");
		uint8_t counts[16];
		int total = 0;
		for( int i = 0; i < 16; ++i ) { counts[i] = seg[p + 1 + i]; total += counts[i]; }
		p += 17;
		if( p + static_cast<size_t>(total) > len ) throw ImageException("JPEG: truncated DHT symbol data");
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
void JpegCodec::Decoder::ParseSOS(const uint8_t* seg, size_t len)
{
	if( len < 1 ) throw ImageException("JPEG: truncated SOS segment");
	int n = seg[0];
	if( len < static_cast<size_t>(1) + static_cast<size_t>(n) * 2 )
		throw ImageException("JPEG: truncated SOS component data");
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
				if( comps_[c].dcTableId >= 4 || comps_[c].acTableId >= 4 )
					throw ImageException("JPEG: huffman table id out of range");
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
		if( table.maxCode[len] >= 0 && code <= table.maxCode[len] )
		{
			return table.values_[table.valPtr[len] + (code - table.minCode[len])];
		}
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
	// DC 카테고리는 표준상 0~11(8bit precision) 범위. 손상/조작된 허프만 테이블이
	// 임의의 큰 심볼값을 돌려줄 수 있으므로, ReadBits/Extend의 쉬프트 연산이
	// 정의되지 않은 동작(UB)에 빠지지 않도록 여기서 범위를 검증한다.
	if( s > 16 ) throw ImageException("JPEG: invalid DC coefficient category");
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
	// 함수 지역 static의 초기화는 C++11부터 스레드 안전함이 표준으로 보장된다
	// (최초 진입 스레드가 초기화를 마칠 때까지 다른 스레드는 대기). 과거에는
	// 별도의 bool 플래그로 수동 더블체크를 했는데, 이는 스레드 안전하지 않은
	// 데이터 레이스였다(ThreadSanitizer로 실제 재현/확인됨). 람다 1회 호출로
	// 초기화되는 지역 static으로 바꿔 컴파일러가 생성하는 가드를 활용한다.
	static const auto cosTable = [] {
		std::array<std::array<float, 8>, 8> table{};
		for( int x = 0; x < 8; ++x )
			for( int u = 0; u < 8; ++u )
				table[x][u] = std::cos((2 * x + 1) * u * 3.14159265358979f / 16.0f);
		return table;
		}();
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

// ---------------------------------------------------------------------------
// 인코더에서만 쓰는 표준(Annex K) 양자화/허프만 테이블. 헤더에 노출할 필요가
// 없는 구현 세부사항이므로 이 파일 스코프의 익명 네임스페이스에 둔다.
// ---------------------------------------------------------------------------
namespace {

	// JPEG 표준(Annex K, Table K.1) 권장 휘도 양자화 테이블(품질 50, natural/raster 순서)
	const uint8_t kStdLumaQuant[64] = {
		16, 11, 10, 16, 24, 40, 51, 61,
		12, 12, 14, 19, 26, 58, 60, 55,
		14, 13, 16, 24, 40, 57, 69, 56,
		14, 17, 22, 29, 51, 87, 80, 62,
		18, 22, 37, 56, 68,109,103, 77,
		24, 35, 55, 64, 81,104,113, 92,
		49, 64, 78, 87,103,121,120,101,
		72, 92, 95, 98,112,100,103, 99
	};

	// JPEG 표준(Annex K, Table K.2) 권장 색차 양자화 테이블(품질 50, natural/raster 순서)
	const uint8_t kStdChromaQuant[64] = {
		17, 18, 24, 47, 99, 99, 99, 99,
		18, 21, 26, 66, 99, 99, 99, 99,
		24, 26, 56, 99, 99, 99, 99, 99,
		47, 66, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99
	};

	// JPEG 표준(Annex K.3, Table K.3) 권장 DC 휘도 허프만 코드 길이별 심볼 개수
	const uint8_t kStdDcLumaCounts[16] = { 0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0 };
	const uint8_t kStdDcLumaValues[12] = { 0,1,2,3,4,5,6,7,8,9,10,11 };

	// JPEG 표준(Annex K.3, Table K.4) 권장 DC 색차 허프만 코드 길이별 심볼 개수
	const uint8_t kStdDcChromaCounts[16] = { 0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0 };
	const uint8_t kStdDcChromaValues[12] = { 0,1,2,3,4,5,6,7,8,9,10,11 };

	// JPEG 표준(Annex K.3, Table K.5) 권장 AC 휘도 허프만 코드 길이별 심볼 개수/값(162개)
	const uint8_t kStdAcLumaCounts[16] = { 0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,125 };
	const uint8_t kStdAcLumaValues[162] = {
		0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,
		0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
		0x22,0x71,0x14,0x32,0x81,0x91,0xA1,0x08,
		0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,
		0x24,0x33,0x62,0x72,0x82,0x09,0x0A,0x16,
		0x17,0x18,0x19,0x1A,0x25,0x26,0x27,0x28,
		0x29,0x2A,0x34,0x35,0x36,0x37,0x38,0x39,
		0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
		0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59,
		0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
		0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,
		0x7A,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
		0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,
		0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,
		0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,
		0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,
		0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,
		0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xE1,0xE2,
		0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,
		0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,
		0xF9,0xFA
	};

	// JPEG 표준(Annex K.3, Table K.6) 권장 AC 색차 허프만 코드 길이별 심볼 개수/값(162개)
	const uint8_t kStdAcChromaCounts[16] = { 0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,119 };
	const uint8_t kStdAcChromaValues[162] = {
		0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,
		0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
		0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,
		0xA1,0xB1,0xC1,0x09,0x23,0x33,0x52,0xF0,
		0x15,0x62,0x72,0xD1,0x0A,0x16,0x24,0x34,
		0xE1,0x25,0xF1,0x17,0x18,0x19,0x1A,0x26,
		0x27,0x28,0x29,0x2A,0x35,0x36,0x37,0x38,
		0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,
		0x49,0x4A,0x53,0x54,0x55,0x56,0x57,0x58,
		0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,
		0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,
		0x79,0x7A,0x82,0x83,0x84,0x85,0x86,0x87,
		0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,
		0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,
		0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,
		0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,
		0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,
		0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,
		0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,
		0xEA,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,
		0xF9,0xFA
	};

} // 익명 네임스페이스

//***************************************************************************
// @brief DHT에 쓸 심볼별 (코드, 길이)를 코드 길이별 개수/심볼 목록으로부터 구성
// @param counts 코드 길이(1~16)별 심볼 개수 배열
// @param symbols 코드 길이 순으로 나열된 심볼 목록
//***************************************************************************
void JpegCodec::EncHuffTable::Build(const uint8_t counts[16], const std::vector<uint8_t>& symbols)
{
	int code = 0, k = 0;
	for( int len = 1; len <= 16; ++len )
	{
		for( int i = 0; i < counts[len - 1]; ++i )
		{
			uint8_t sym = symbols[k++];
			length[sym] = static_cast<uint8_t>(len);
			this->code[sym] = static_cast<uint16_t>(code);
			++code;
		}
		code <<= 1;
	}
}

//***************************************************************************
// @brief 비트 하나 이상을 MSB-first 순서로 기록하고, 0xFF 바이트는 즉시 스터핑
// @param value 기록할 값(하위 length 비트만 사용)
// @param length 기록할 비트 개수
//***************************************************************************
void JpegCodec::Encoder::BitWriter::PutBits(uint32_t value, int length)
{
	if( length <= 0 ) return;
	acc_ = (acc_ << length) | (value & ((1u << length) - 1));
	nbits_ += length;
	while( nbits_ >= 8 )
	{
		nbits_ -= 8;
		uint8_t byte = static_cast<uint8_t>((acc_ >> nbits_) & 0xFF);
		out_.push_back(byte);
		if( byte == 0xFF ) out_.push_back(0x00); // 마커와 구분하기 위한 스터핑
	}
	if( nbits_ > 0 ) acc_ &= (1ULL << nbits_) - 1;
	else acc_ = 0;
}

//***************************************************************************
// @brief 남은 비트를 1로 패딩하여 마지막 바이트를 완성해 출력 버퍼로 밀어넣음
//***************************************************************************
void JpegCodec::Encoder::BitWriter::Flush()
{
	if( nbits_ > 0 )
	{
		int pad = 8 - nbits_;
		uint8_t byte = static_cast<uint8_t>(((acc_ << pad) | ((1u << pad) - 1)) & 0xFF);
		out_.push_back(byte);
		if( byte == 0xFF ) out_.push_back(0x00);
		nbits_ = 0;
		acc_ = 0;
	}
}

//***************************************************************************
// @brief 지금까지 기록된 바이트 버퍼를 소유권과 함께 반환
// @return 완성된 엔트로피 코딩 바이트 버퍼
//***************************************************************************
std::vector<uint8_t> JpegCodec::Encoder::BitWriter::TakeBuffer() { Flush(); return std::move(out_); }

//***************************************************************************
// @brief 표준 기본(품질 50) 양자화 테이블을 품질값에 맞게 스케일링
// @param base 품질 50 기준 표준 양자화 테이블(natural/raster 순서, 64개)
// @param quality 압축 품질(1~100)
// @param out 스케일링된 양자화 테이블(natural/raster 순서) 출력
//***************************************************************************
void JpegCodec::Encoder::BuildQuantTable(const uint8_t base[64], int quality, uint16_t out[64])
{
	quality = std::max(1, std::min(100, quality));
	int scale = (quality < 50) ? (5000 / quality) : (200 - quality * 2);
	for( int i = 0; i < 64; ++i )
	{
		int v = (static_cast<int>(base[i]) * scale + 50) / 100;
		out[i] = static_cast<uint16_t>(std::max(1, std::min(255, v)));
	}
}

//***************************************************************************
// @brief 마커와 길이 필드, 페이로드로 구성된 세그먼트 하나를 출력 버퍼에 기록
// @param out 세그먼트를 누적할 출력 버퍼
// @param marker 마커 값(예: 0xFFDB)
// @param data 세그먼트 페이로드(길이 필드 제외), 없으면 nullptr
// @param len 페이로드 길이(바이트)
//***************************************************************************
void JpegCodec::Encoder::WriteSegment(std::vector<uint8_t>& out, uint16_t marker, const uint8_t* data, size_t len)
{
	out.push_back(static_cast<uint8_t>(marker >> 8));
	out.push_back(static_cast<uint8_t>(marker & 0xFF));
	uint16_t segLen = static_cast<uint16_t>(len + 2);
	out.push_back(static_cast<uint8_t>(segLen >> 8));
	out.push_back(static_cast<uint8_t>(segLen & 0xFF));
	if( data && len > 0 ) out.insert(out.end(), data, data + len);
}

//***************************************************************************
// @brief 레벨시프트된 8x8 공간 영역 블록에 순방향(2D) DCT를 적용
// @param in 레벨시프트(−128) 완료된 64개 입력 샘플(자연 순서)
// @param out 계산된 64개 DCT 계수(자연 순서) 출력
//***************************************************************************
void JpegCodec::Encoder::FDCT8x8(const float in[64], float out[64])
{
	// IDCT8x8과 동일한 이유로 스레드 안전한 지역 static 초기화 패턴을 사용한다
	// (수동 bool 플래그 더블체크는 데이터 레이스 - ThreadSanitizer로 확인됨).
	static const auto cosTable = [] {
		std::array<std::array<float, 8>, 8> table{};
		for( int n = 0; n < 8; ++n )
			for( int k = 0; k < 8; ++k )
				table[n][k] = std::cos((2 * n + 1) * k * 3.14159265358979f / 16.0f);
		return table;
		}();

	float tmp[64];
	for( int y = 0; y < 8; ++y )
	{
		for( int u = 0; u < 8; ++u )
		{
			float sum = 0.0f;
			for( int x = 0; x < 8; ++x ) sum += in[y * 8 + x] * cosTable[x][u];
			float cu = (u == 0) ? 0.70710678f : 1.0f;
			tmp[y * 8 + u] = 0.5f * cu * sum;
		}
	}
	for( int u = 0; u < 8; ++u )
	{
		for( int v = 0; v < 8; ++v )
		{
			float sum = 0.0f;
			for( int y = 0; y < 8; ++y ) sum += tmp[y * 8 + u] * cosTable[y][v];
			float cv = (v == 0) ? 0.70710678f : 1.0f;
			out[v * 8 + u] = 0.5f * cv * sum;
		}
	}
}

//***************************************************************************
// @brief 값의 크기를 표현하는 데 필요한 비트 수(JPEG 카테고리)를 계산
// @param value 카테고리를 계산할 값(부호 있음, 0 가능)
// @return 카테고리(0~11)
//***************************************************************************
int JpegCodec::Encoder::CalcCategory(int value)
{
	int v = value < 0 ? -value : value;
	int category = 0;
	while( v > 0 ) { ++category; v >>= 1; }
	return category;
}

//***************************************************************************
// @brief 허프만 테이블에서 심볼에 대응하는 코드를 찾아 비트스트림에 기록
// @param bw 비트를 기록할 BitWriter
// @param table 사용할 인코드용 허프만 테이블
// @param symbol 기록할 심볼(0~255)
//***************************************************************************
void JpegCodec::Encoder::EmitHuffman(BitWriter& bw, const EncHuffTable& table, int symbol)
{
	bw.PutBits(table.code[symbol], table.length[symbol]);
}

//***************************************************************************
// @brief DC 차분/AC 계수의 카테고리 추가 비트(부호 있는 크기값)를 기록
// @param bw 비트를 기록할 BitWriter
// @param value 기록할 값(부호 있음)
// @param category 값의 카테고리(비트 수)
//***************************************************************************
void JpegCodec::Encoder::EmitCategoryBits(BitWriter& bw, int value, int category)
{
	if( category == 0 ) return;
	// 음수는 (value + 2^category - 1)로 인코딩하는 JPEG의 부호 있는 크기값 규칙(Decoder::Extend의 역연산)
	int bits = (value >= 0) ? value : (value + (1 << category) - 1);
	bw.PutBits(static_cast<uint32_t>(bits), category);
}

//***************************************************************************
// @brief 양자화까지 끝난 8x8 블록 하나를 DC 차분 + AC 런랭스 방식으로 허프만 인코드
// @param bw 비트를 기록할 BitWriter
// @param block 레벨시프트 완료된 64개 입력 샘플(자연 순서, FDCT 이전)
// @param quantTable 사용할 양자화 테이블(natural/raster 순서)
// @param dcTable 사용할 DC 허프만 테이블
// @param acTable 사용할 AC 허프만 테이블
// @param dcPred 이 컴포넌트의 DC 예측값(누적, in-out)
//***************************************************************************
void JpegCodec::Encoder::EncodeBlock(BitWriter& bw, const float block[64], const uint16_t quantTable[64],
	const EncHuffTable& dcTable, const EncHuffTable& acTable, int& dcPred)
{
	float dctOut[64];
	FDCT8x8(block, dctOut);

	int natural[64];
	for( int i = 0; i < 64; ++i )
	{
		natural[i] = static_cast<int>(std::round(dctOut[i] / static_cast<float>(quantTable[i])));
	}

	int zz[64];
	for( int i = 0; i < 64; ++i ) zz[i] = natural[kZigZag[i]];

	// DC: 이전 블록과의 차분을 카테고리 심볼 + 추가 비트로 인코드
	int diff = zz[0] - dcPred;
	dcPred = zz[0];
	int dcCategory = CalcCategory(diff);
	EmitHuffman(bw, dcTable, dcCategory);
	EmitCategoryBits(bw, diff, dcCategory);

	// AC: 지그재그 순서 1..63을 런랭스 + 카테고리 심볼로 인코드
	int zeroRun = 0;
	for( int k = 1; k < 64; ++k )
	{
		int v = zz[k];
		if( v == 0 )
		{
			++zeroRun;
			continue;
		}
		while( zeroRun >= 16 )
		{
			EmitHuffman(bw, acTable, 0xF0); // ZRL: 16개 연속 0
			zeroRun -= 16;
		}
		int category = CalcCategory(v);
		int rs = (zeroRun << 4) | category;
		EmitHuffman(bw, acTable, rs);
		EmitCategoryBits(bw, v, category);
		zeroRun = 0;
	}
	if( zeroRun > 0 )
	{
		EmitHuffman(bw, acTable, 0x00); // EOB: 블록 끝까지 0
	}
}

//***************************************************************************
// @brief ImageBuffer를 지정한 품질로 4:4:4 baseline JPEG 바이트 시퀀스로 인코드
// @param image 인코드할 원본 이미지
// @param quality 압축 품질(1~100, 클수록 고화질/저압축)
// @return 완성된 JPEG 파일 바이트 시퀀스
//***************************************************************************
std::vector<uint8_t> JpegCodec::Encoder::Run(const ImageBuffer& image, int quality) const
{
	if( image.Empty() ) throw ImageException("JPEG: cannot encode an empty image");

	const int width = static_cast<int>(image.Width());
	const int height = static_cast<int>(image.Height());
	const int paddedW = (width + 7) / 8 * 8;
	const int paddedH = (height + 7) / 8 * 8;

	// 1) RGBA8 -> Y/Cb/Cr 평면 변환(가장자리 픽셀 복제로 8의 배수까지 패딩)
	std::vector<uint8_t> planeY(static_cast<size_t>(paddedW) * paddedH);
	std::vector<uint8_t> planeCb(static_cast<size_t>(paddedW) * paddedH);
	std::vector<uint8_t> planeCr(static_cast<size_t>(paddedW) * paddedH);

	for( int py = 0; py < paddedH; ++py )
	{
		int sy = std::min(py, height - 1);
		for( int px = 0; px < paddedW; ++px )
		{
			int sx = std::min(px, width - 1);
			const uint8_t* p = image.At(static_cast<uint32_t>(sx), static_cast<uint32_t>(sy));
			uint8_t y, cb, cr;
			ColorSpace::RgbToYCbCr(p[0], p[1], p[2], y, cb, cr);
			size_t idx = static_cast<size_t>(py) * paddedW + px;
			planeY[idx] = y;
			planeCb[idx] = cb;
			planeCr[idx] = cr;
		}
	}

	// 2) 양자화 테이블 준비(품질에 맞게 스케일링)
	uint16_t quantLuma[64], quantChroma[64];
	BuildQuantTable(kStdLumaQuant, quality, quantLuma);
	BuildQuantTable(kStdChromaQuant, quality, quantChroma);

	// 3) 표준(Annex K) 허프만 테이블 구성
	EncHuffTable dcLuma, acLuma, dcChroma, acChroma;
	dcLuma.Build(kStdDcLumaCounts, std::vector<uint8_t>(kStdDcLumaValues, kStdDcLumaValues + 12));
	acLuma.Build(kStdAcLumaCounts, std::vector<uint8_t>(kStdAcLumaValues, kStdAcLumaValues + 162));
	dcChroma.Build(kStdDcChromaCounts, std::vector<uint8_t>(kStdDcChromaValues, kStdDcChromaValues + 12));
	acChroma.Build(kStdAcChromaCounts, std::vector<uint8_t>(kStdAcChromaValues, kStdAcChromaValues + 162));

	// 4) 헤더(SOI, APP0/JFIF, DQT, SOF0, DHT, SOS) 기록
	std::vector<uint8_t> out;
	out.push_back(0xFF); out.push_back(0xD8); // SOI

	const uint8_t app0[14] = {
		'J','F','I','F', 0,
		1, 1,             // version 1.1
		0,                // units: 0 = no units (aspect ratio only)
		0, 1, 0, 1,       // Xdensity=1, Ydensity=1
		0, 0              // thumbnail width/height = 0
	};
	WriteSegment(out, 0xFFE0, app0, sizeof(app0));

	uint8_t dqtLuma[65];
	dqtLuma[0] = 0x00; // Pq=0(8bit precision), Tq=0
	for( int i = 0; i < 64; ++i ) dqtLuma[1 + i] = static_cast<uint8_t>(quantLuma[kZigZag[i]]);
	WriteSegment(out, 0xFFDB, dqtLuma, sizeof(dqtLuma));

	uint8_t dqtChroma[65];
	dqtChroma[0] = 0x01; // Tq=1
	for( int i = 0; i < 64; ++i ) dqtChroma[1 + i] = static_cast<uint8_t>(quantChroma[kZigZag[i]]);
	WriteSegment(out, 0xFFDB, dqtChroma, sizeof(dqtChroma));

	uint8_t sof0[15]; // precision(1)+height(2)+width(2)+numComp(1)+3*comp(3*3) = 15
	sof0[0] = 8; // precision
	sof0[1] = static_cast<uint8_t>(height >> 8); sof0[2] = static_cast<uint8_t>(height & 0xFF);
	sof0[3] = static_cast<uint8_t>(width >> 8);  sof0[4] = static_cast<uint8_t>(width & 0xFF);
	sof0[5] = 3; // Y, Cb, Cr
	sof0[6] = 1; sof0[7] = 0x11; sof0[8] = 0;  // Y : id=1, 4:4:4(1x1), quant table 0
	sof0[9] = 2; sof0[10] = 0x11; sof0[11] = 1; // Cb: id=2, 4:4:4(1x1), quant table 1
	sof0[12] = 3; sof0[13] = 0x11; sof0[14] = 1; // Cr: id=3, 4:4:4(1x1), quant table 1
	WriteSegment(out, 0xFFC0, sof0, sizeof(sof0));

	auto writeDht = [&](uint8_t classAndId, const uint8_t counts[16], const uint8_t* values, int numValues) {
		std::vector<uint8_t> seg;
		seg.push_back(classAndId);
		seg.insert(seg.end(), counts, counts + 16);
		seg.insert(seg.end(), values, values + numValues);
		WriteSegment(out, 0xFFC4, seg.data(), seg.size());
		};
	writeDht(0x00, kStdDcLumaCounts, kStdDcLumaValues, 12);   // DC, id 0 (휘도)
	writeDht(0x10, kStdAcLumaCounts, kStdAcLumaValues, 162);  // AC, id 0 (휘도)
	writeDht(0x01, kStdDcChromaCounts, kStdDcChromaValues, 12);  // DC, id 1 (색차)
	writeDht(0x11, kStdAcChromaCounts, kStdAcChromaValues, 162); // AC, id 1 (색차)

	const uint8_t sos[10] = { // ns(1)+2*ns(6)+Ss,Se,AhAl(3) = 10
		3,             // 컴포넌트 수
		1, 0x00,       // Y  : DC테이블0, AC테이블0
		2, 0x11,       // Cb : DC테이블1, AC테이블1
		3, 0x11,       // Cr : DC테이블1, AC테이블1
		0, 63, 0       // Ss=0, Se=63, Ah/Al=0 (baseline 고정값)
	};
	WriteSegment(out, 0xFFDA, sos, sizeof(sos));

	// 5) 엔트로피 코딩된 스캔 데이터: 4:4:4이므로 MCU = 8x8 블록 1개(Y,Cb,Cr 순서로 반복)
	Encoder::BitWriter bw;
	int dcPredY = 0, dcPredCb = 0, dcPredCr = 0;
	float block[64];

	for( int by = 0; by < paddedH; by += 8 )
	{
		for( int bx = 0; bx < paddedW; bx += 8 )
		{
			for( int yy = 0; yy < 8; ++yy )
				for( int xx = 0; xx < 8; ++xx )
					block[yy * 8 + xx] = static_cast<float>(planeY[static_cast<size_t>(by + yy) * paddedW + (bx + xx)]) - 128.0f;
			EncodeBlock(bw, block, quantLuma, dcLuma, acLuma, dcPredY);

			for( int yy = 0; yy < 8; ++yy )
				for( int xx = 0; xx < 8; ++xx )
					block[yy * 8 + xx] = static_cast<float>(planeCb[static_cast<size_t>(by + yy) * paddedW + (bx + xx)]) - 128.0f;
			EncodeBlock(bw, block, quantChroma, dcChroma, acChroma, dcPredCb);

			for( int yy = 0; yy < 8; ++yy )
				for( int xx = 0; xx < 8; ++xx )
					block[yy * 8 + xx] = static_cast<float>(planeCr[static_cast<size_t>(by + yy) * paddedW + (bx + xx)]) - 128.0f;
			EncodeBlock(bw, block, quantChroma, dcChroma, acChroma, dcPredCr);
		}
	}

	std::vector<uint8_t> entropyData = bw.TakeBuffer();
	out.insert(out.end(), entropyData.begin(), entropyData.end());

	out.push_back(0xFF); out.push_back(0xD9); // EOI
	return out;
}
