
#include "pch.h"
#include "PngCodec.h"

//***************************************************************************
// @brief 빅엔디안 4바이트 정수를 읽음(PNG 청크 길이/필드는 빅엔디안)
// @param p 읽을 위치
// @return 읽은 32비트 정수
//***************************************************************************
uint32_t PngCodec::ReadU32(const uint8_t* p)
{
	return (static_cast<uint32_t>(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

//***************************************************************************
// @brief 빅엔디안 4바이트 정수를 기록
// @param p 기록할 위치
// @param v 기록할 값
//***************************************************************************
void PngCodec::WriteU32(uint8_t* p, uint32_t v)
{
	p[0] = (v >> 24) & 0xFF; p[1] = (v >> 16) & 0xFF; p[2] = (v >> 8) & 0xFF; p[3] = v & 0xFF;
}

//***************************************************************************
// @brief 길이/타입/데이터/CRC32로 구성된 PNG 청크 하나를 출력 버퍼에 기록
// @param out 청크를 누적할 출력 버퍼
// @param type 4바이트 청크 타입 문자열(예: "IHDR")
// @param data 청크 데이터 시작 주소(없으면 nullptr)
// @param len 청크 데이터 길이(바이트)
//***************************************************************************
void PngCodec::WriteChunk(std::vector<uint8_t>& out, const char* type,
	const uint8_t* data, size_t len)
{
	uint8_t lenBuf[4];
	WriteU32(lenBuf, static_cast<uint32_t>(len));
	out.insert(out.end(), lenBuf, lenBuf + 4);

	size_t typeStart = out.size();
	out.insert(out.end(), type, type + 4);
	if( data && len > 0 ) out.insert(out.end(), data, data + len);

	uint32_t crc = Crc32::Compute(&out[typeStart], 4 + len);
	uint8_t crcBuf[4];
	WriteU32(crcBuf, crc);
	out.insert(out.end(), crcBuf, crcBuf + 4);
}

//***************************************************************************
// @brief 데이터가 PNG 시그니처(8바이트)로 시작하는지 확인
// @param data 확인할 데이터 시작 주소
// @param size 데이터 길이(바이트)
// @return PNG로 판단되면 true
//***************************************************************************
bool PngCodec::CanDecode(const uint8_t* data, size_t size) const
{
	static const uint8_t kSig[8] = { 0x89,'P','N','G','\r','\n',0x1A,'\n' };
	return size >= 8 && std::memcmp(data, kSig, 8) == 0;
}

//***************************************************************************
// @brief PNG color type에 대응하는 채널 개수를 반환
// @param colorType IHDR의 컬러 타입 값
// @return 채널 개수(Gray=1, RGB=3, GrayAlpha=2, RGBA=4)
//***************************************************************************
int PngCodec::ChannelsForColorType(uint8_t colorType)
{
	switch( colorType )
	{
	case 0: return 1;
	case 2: return 3;
	case 4: return 2;
	case 6: return 4;
	default: throw ImageException("PNG: unsupported color type (palette)");
	}
}

//***************************************************************************
// @brief Paeth 예측 필터의 예측값을 계산
// @param a 왼쪽 픽셀 값
// @param b 위쪽 픽셀 값
// @param c 왼쪽 위 픽셀 값
// @return 세 후보 중 예측 오차가 가장 작은 값
//***************************************************************************
uint8_t PngCodec::PaethPredictor(int a, int b, int c)
{
	int p = a + b - c;
	int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
	if( pa <= pb && pa <= pc ) return static_cast<uint8_t>(a);
	if( pb <= pc ) return static_cast<uint8_t>(b);
	return static_cast<uint8_t>(c);
}

//***************************************************************************
// @brief IDAT 압축 해제 결과(필터 타입 바이트 포함)를 스캔라인별로 역필터링
// @param raw 필터 타입 바이트가 포함된 압축 해제 결과
// @param out 역필터링된 순수 픽셀 데이터를 채울 출력 버퍼
// @param width 이미지 가로 크기(픽셀)
// @param height 이미지 세로 크기(픽셀)
// @param channels 픽셀당 채널 개수
//***************************************************************************
void PngCodec::Unfilter(const std::vector<uint8_t>& raw, std::vector<uint8_t>& out,
	uint32_t width, uint32_t height, int channels)
{
	uint32_t stride = width * channels;
	std::vector<uint8_t> prevRow(stride, 0);

	size_t srcPos = 0;
	for( uint32_t y = 0; y < height; ++y )
	{
		uint8_t filterType = raw[srcPos++];
		uint8_t* curRow = &out[static_cast<size_t>(y) * stride];
		const uint8_t* src = &raw[srcPos];

		for( uint32_t x = 0; x < stride; ++x )
		{
			uint8_t a = (x >= static_cast<uint32_t>(channels)) ? curRow[x - channels] : 0;
			uint8_t b = prevRow[x];
			uint8_t c = (x >= static_cast<uint32_t>(channels)) ? prevRow[x - channels] : 0;
			uint8_t raw_x = src[x];

			switch( filterType )
			{
			case 0: curRow[x] = raw_x; break;
			case 1: curRow[x] = raw_x + a; break;
			case 2: curRow[x] = raw_x + b; break;
			case 3: curRow[x] = raw_x + static_cast<uint8_t>((a + b) / 2); break;
			case 4: curRow[x] = raw_x + PaethPredictor(a, b, c); break;
			default: throw ImageException("PNG: invalid filter type");
			}
		}
		std::memcpy(prevRow.data(), curRow, stride);
		srcPos += stride;
	}
}

//***************************************************************************
// @brief 스캔라인에 대해 5종 필터(None/Sub/Up/Average/Paeth)를 모두 시도하여
//        절대값 합이 최소인 필터를 선택(libpng의 "최소 절대값 합" 휴리스틱)
// @param curRow 현재 스캔라인의 원본(비필터) 픽셀 데이터
// @param prevRow 이전 스캔라인의 원본(비필터) 픽셀 데이터(첫 행은 0으로 채운 행)
// @param stride 한 스캔라인의 바이트 길이(width * channels)
// @param channels 픽셀당 채널 개수
// @param outFiltered 선택된 필터로 필터링된 stride 바이트를 채울 출력 버퍼
// @return 선택된 필터 타입(0=None, 1=Sub, 2=Up, 3=Average, 4=Paeth)
//***************************************************************************
int PngCodec::FilterRow(const uint8_t* curRow, const uint8_t* prevRow, uint32_t stride,
	int channels, uint8_t* outFiltered)
{
	static thread_local std::vector<uint8_t> candidates[5];
	for( auto& c : candidates ) c.resize(stride);

	for( uint32_t x = 0; x < stride; ++x )
	{
		uint8_t a = (x >= static_cast<uint32_t>(channels)) ? curRow[x - channels] : 0;
		uint8_t b = prevRow[x];
		uint8_t c = (x >= static_cast<uint32_t>(channels)) ? prevRow[x - channels] : 0;
		uint8_t val = curRow[x];

		candidates[0][x] = val;
		candidates[1][x] = static_cast<uint8_t>(val - a);
		candidates[2][x] = static_cast<uint8_t>(val - b);
		candidates[3][x] = static_cast<uint8_t>(val - (a + b) / 2);
		candidates[4][x] = static_cast<uint8_t>(val - PaethPredictor(a, b, c));
	}

	int bestType = 0;
	long bestSum = -1;
	for( int t = 0; t < 5; ++t )
	{
		long sum = 0;
		for( uint32_t x = 0; x < stride; ++x )
		{
			int8_t signedVal = static_cast<int8_t>(candidates[t][x]);
			sum += std::abs(static_cast<int>(signedVal));
		}
		if( bestSum < 0 || sum < bestSum ) { bestSum = sum; bestType = t; }
	}

	std::memcpy(outFiltered, candidates[bestType].data(), stride);
	return bestType;
}

//***************************************************************************
// @brief PNG 바이트 시퀀스를 디코드하여 ImageBuffer로 변환
// @param data PNG 파일 데이터 시작 주소
// @param size PNG 파일 데이터 길이(바이트)
// @return 디코드된 RGBA8 ImageBuffer
//***************************************************************************
ImageBuffer PngCodec::Decode(const uint8_t* data, size_t size) const
{
	if( !CanDecode(data, size) ) throw ImageException("PNG: invalid signature");

	size_t pos = 8;
	uint32_t width = 0, height = 0;
	uint8_t bitDepth = 0, colorType = 0, interlace = 0;
	std::vector<uint8_t> idat;

	while( pos + 8 <= size )
	{
		uint32_t len = ReadU32(data + pos);
		const char* type = reinterpret_cast<const char*>(data + pos + 4);
		const uint8_t* chunkData = data + pos + 8;
		if( pos + 12 + len > size ) throw ImageException("PNG: truncated chunk");

		if( std::memcmp(type, "IHDR", 4) == 0 )
		{
			width = ReadU32(chunkData);
			height = ReadU32(chunkData + 4);
			bitDepth = chunkData[8];
			colorType = chunkData[9];
			interlace = chunkData[12];
		}
		else if( std::memcmp(type, "IDAT", 4) == 0 )
		{
			idat.insert(idat.end(), chunkData, chunkData + len);
		}
		else if( std::memcmp(type, "IEND", 4) == 0 )
		{
			break;
		}
		pos += 12 + len;
	}

	if( width == 0 || height == 0 ) throw ImageException("PNG: missing IHDR");
	if( bitDepth != 8 ) throw ImageException("PNG: only 8-bit depth supported");
	if( interlace != 0 ) throw ImageException("PNG: interlaced PNG not supported");

	int channels = ChannelsForColorType(colorType);
	std::vector<uint8_t> raw = Inflate::Decompress(idat.data(), idat.size());

	uint32_t stride = width * channels;
	if( raw.size() < static_cast<size_t>(stride + 1) * height )
		throw ImageException("PNG: decompressed data too short");

	std::vector<uint8_t> unfiltered(static_cast<size_t>(stride) * height);
	Unfilter(raw, unfiltered, width, height, channels);

	ImageBuffer image(width, height);
	for( uint32_t y = 0; y < height; ++y )
	{
		const uint8_t* row = &unfiltered[static_cast<size_t>(y) * stride];
		for( uint32_t x = 0; x < width; ++x )
		{
			const uint8_t* px = row + x * channels;
			uint8_t r, g, b, a = 255;
			switch( colorType )
			{
			case 0: r = g = b = px[0]; break;
			case 4: r = g = b = px[0]; a = px[1]; break;
			case 2: r = px[0]; g = px[1]; b = px[2]; break;
			case 6: r = px[0]; g = px[1]; b = px[2]; a = px[3]; break;
			default: throw ImageException("PNG: unsupported color type (palette)");
			}
			image.SetPixel(x, y, r, g, b, a);
		}
	}
	return image;
}

//***************************************************************************
// @brief ImageBuffer를 적응형 필터링 + zlib 압축을 적용한 RGBA8 PNG로 인코드
// @param image 인코드할 원본 이미지
// @return 완성된 PNG 파일 바이트 시퀀스
//***************************************************************************
std::vector<uint8_t> PngCodec::Encode(const ImageBuffer& image) const
{
	uint32_t width = image.Width(), height = image.Height();
	const int channels = 4;
	uint32_t stride = width * channels;

	std::vector<uint8_t> raw(static_cast<size_t>(stride + 1) * height);
	std::vector<uint8_t> zeroRow(stride, 0);
	const uint8_t* prevRow = zeroRow.data();

	for( uint32_t y = 0; y < height; ++y )
	{
		const uint8_t* curRow = image.At(0, y);
		uint8_t* dst = &raw[static_cast<size_t>(y) * (stride + 1)];
		dst[0] = static_cast<uint8_t>(FilterRow(curRow, prevRow, stride, channels, dst + 1));
		prevRow = curRow;
	}

	std::vector<uint8_t> compressed = Deflate::CompressZlib(raw.data(), raw.size());

	std::vector<uint8_t> out;
	static const uint8_t kSig[8] = { 0x89,'P','N','G','\r','\n',0x1A,'\n' };
	out.insert(out.end(), kSig, kSig + 8);

	uint8_t ihdr[13];
	WriteU32(ihdr, width);
	WriteU32(ihdr + 4, height);
	ihdr[8] = 8;
	ihdr[9] = 6;
	ihdr[10] = 0;
	ihdr[11] = 0;
	ihdr[12] = 0;
	WriteChunk(out, "IHDR", ihdr, sizeof(ihdr));
	WriteChunk(out, "IDAT", compressed.data(), compressed.size());
	WriteChunk(out, "IEND", nullptr, 0);
	return out;
}