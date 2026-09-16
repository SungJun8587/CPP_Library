
#include "pch.h"
#include "BmpCodec.h"

//***************************************************************************
// @brief 리틀엔디안 2바이트 정수를 읽음
// @param p 읽을 위치
// @return 읽은 16비트 정수
//***************************************************************************
uint16_t BmpCodec::ReadU16(const uint8_t* p) { return p[0] | (p[1] << 8); }

//***************************************************************************
// @brief 리틀엔디안 4바이트 정수를 읽음
// @param p 읽을 위치
// @return 읽은 32비트 정수
//***************************************************************************
uint32_t BmpCodec::ReadU32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }

//***************************************************************************
// @brief 리틀엔디안 2바이트 정수를 기록
// @param p 기록할 위치
// @param v 기록할 값
//***************************************************************************
void BmpCodec::WriteU16(uint8_t* p, uint16_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }

//***************************************************************************
// @brief 리틀엔디안 4바이트 정수를 기록
// @param p 기록할 위치
// @param v 기록할 값
//***************************************************************************
void BmpCodec::WriteU32(uint8_t* p, uint32_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF; }

//***************************************************************************
// @brief 데이터가 BMP 시그니처("BM")로 시작하는지 확인
// @param data 확인할 데이터 시작 주소
// @param size 데이터 길이(바이트)
// @return BMP로 판단되면 true
//***************************************************************************
bool BmpCodec::CanDecode(const uint8_t* data, size_t size) const
{
	return size >= 2 && data[0] == 'B' && data[1] == 'M';
}

//***************************************************************************
// @brief BMP 바이트 시퀀스를 디코드하여 ImageBuffer로 변환
// @param data BMP 파일 데이터 시작 주소
// @param size BMP 파일 데이터 길이(바이트)
// @return 디코드된 RGBA8 ImageBuffer
//***************************************************************************
ImageBuffer BmpCodec::Decode(const uint8_t* data, size_t size) const
{
	if( !CanDecode(data, size) ) throw ImageException("BMP: invalid signature");
	if( size < 54 ) throw ImageException("BMP: file too small");

	uint32_t pixelDataOffset = ReadU32(data + 10);
	uint32_t dibHeaderSize = ReadU32(data + 14);
	int32_t  width = static_cast<int32_t>(ReadU32(data + 18));
	int32_t  heightRaw = static_cast<int32_t>(ReadU32(data + 22));
	uint16_t bitsPerPixel = ReadU16(data + 28);
	uint32_t compression = ReadU32(data + 30);

	if( compression != 0 ) throw ImageException("BMP: compressed BMP not supported");
	if( bitsPerPixel != 24 && bitsPerPixel != 32 )
		throw ImageException("BMP: only 24/32 bpp supported");
	(void)dibHeaderSize;

	bool topDown = heightRaw < 0;
	int32_t height = topDown ? -heightRaw : heightRaw;
	int bytesPerPixel = bitsPerPixel / 8;
	uint32_t rowStride = ((static_cast<uint32_t>(width) * bytesPerPixel + 3) / 4) * 4;

	if( pixelDataOffset + static_cast<uint64_t>(rowStride) * height > size )
		throw ImageException("BMP: pixel data exceeds file size");

	ImageBuffer image(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
	for( int32_t y = 0; y < height; ++y )
	{
		int32_t srcRow = topDown ? y : (height - 1 - y);
		const uint8_t* row = data + pixelDataOffset + static_cast<size_t>(srcRow) * rowStride;
		for( int32_t x = 0; x < width; ++x )
		{
			const uint8_t* px = row + x * bytesPerPixel;
			uint8_t b = px[0], g = px[1], r = px[2];
			uint8_t a = (bytesPerPixel == 4) ? px[3] : 255;
			image.SetPixel(static_cast<uint32_t>(x), static_cast<uint32_t>(y), r, g, b, a);
		}
	}
	return image;
}

//***************************************************************************
// @brief ImageBuffer를 24bpp 비압축 BMP 바이트 시퀀스로 인코드
// @param image 인코드할 원본 이미지
// @return 완성된 BMP 파일 바이트 시퀀스
//***************************************************************************
std::vector<uint8_t> BmpCodec::Encode(const ImageBuffer& image) const
{
	uint32_t width = image.Width(), height = image.Height();
	uint32_t rowStride = ((width * 3 + 3) / 4) * 4;
	uint32_t pixelDataSize = rowStride * height;
	uint32_t fileSize = 54 + pixelDataSize;

	std::vector<uint8_t> out(fileSize, 0);
	out[0] = 'B'; out[1] = 'M';
	WriteU32(&out[2], fileSize);
	WriteU32(&out[10], 54);

	WriteU32(&out[14], 40);
	WriteU32(&out[18], width);
	WriteU32(&out[22], height);
	WriteU16(&out[26], 1);
	WriteU16(&out[28], 24);
	WriteU32(&out[30], 0);
	WriteU32(&out[34], pixelDataSize);

	for( uint32_t y = 0; y < height; ++y )
	{
		uint32_t srcRow = height - 1 - y;
		uint8_t* row = &out[54] + static_cast<size_t>(y) * rowStride;
		for( uint32_t x = 0; x < width; ++x )
		{
			const uint8_t* px = image.At(x, srcRow);
			uint8_t* dst = row + x * 3;
			dst[0] = px[2];
			dst[1] = px[1];
			dst[2] = px[0];
		}
	}
	return out;
}