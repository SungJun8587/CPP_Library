
#include "pch.h"
#include "ImageTypes.h"

//***************************************************************************
// @brief 픽셀 포맷 하나가 차지하는 바이트 수를 반환
// @param fmt 픽셀 포맷
// @return 채널당 바이트 수(RGBA8=4, RGB8=3, GRAY8=1)
//***************************************************************************
int BytesPerPixel(PixelFormat fmt)
{
	switch( fmt )
	{
	case PixelFormat::RGBA8: return 4;
	case PixelFormat::RGB8:  return 3;
	case PixelFormat::GRAY8: return 1;
	}
	return 0;
}

//***************************************************************************
// @brief 주어진 크기로 RGBA8 픽셀 버퍼를 0으로 초기화하여 생성
// @param width 가로 크기(픽셀)
// @param height 세로 크기(픽셀)
//***************************************************************************
ImageBuffer::ImageBuffer(uint32_t width, uint32_t height)
	: width_(width), height_(height), pixels_(static_cast<size_t>(width)* height * 4, 0)
{
}

//***************************************************************************
// @brief (x, y) 픽셀의 RGBA 시작 주소를 반환
// @param x 가로 좌표
// @param y 세로 좌표
// @return 해당 픽셀의 쓰기 가능한 시작 주소
//***************************************************************************
uint8_t* ImageBuffer::At(uint32_t x, uint32_t y)
{
	return pixels_.data() + (static_cast<size_t>(y) * width_ + x) * 4;
}

//***************************************************************************
// @brief (x, y) 픽셀의 RGBA 시작 주소를 반환
// @param x 가로 좌표
// @param y 세로 좌표
// @return 해당 픽셀의 읽기 전용 시작 주소
//***************************************************************************
const uint8_t* ImageBuffer::At(uint32_t x, uint32_t y) const
{
	return pixels_.data() + (static_cast<size_t>(y) * width_ + x) * 4;
}

//***************************************************************************
// @brief (x, y) 픽셀의 RGBA 값을 설정
// @param x 가로 좌표
// @param y 세로 좌표
// @param r Red 채널 값
// @param g Green 채널 값
// @param b Blue 채널 값
// @param a Alpha 채널 값(기본값 255)
//***************************************************************************
void ImageBuffer::SetPixel(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	uint8_t* p = At(x, y);
	p[0] = r; p[1] = g; p[2] = b; p[3] = a;
}

//***************************************************************************
// @brief 버퍼를 주어진 크기로 재할당하고 0으로 초기화
// @param width 새 가로 크기(픽셀)
// @param height 새 세로 크기(픽셀)
//***************************************************************************
void ImageBuffer::Resize(uint32_t width, uint32_t height)
{
	width_ = width;
	height_ = height;
	pixels_.assign(static_cast<size_t>(width) * height * 4, 0);
}

//***************************************************************************
// @brief 현재 버퍼를 깊은 복사하여 새 ImageBuffer로 반환
// @return 픽셀 데이터를 복사한 새 ImageBuffer
//***************************************************************************
ImageBuffer ImageBuffer::Clone() const
{
	ImageBuffer copy;
	copy.width_ = width_;
	copy.height_ = height_;
	copy.pixels_ = pixels_;
	return copy;
}
