
#ifndef UC_COLORSPACE_H
#define UC_COLORSPACE_H

#include "ImageTypes.h"
#include <cstdint>

//***************************************************************************
// @brief ImageBuffer(RGBA8)에 대한 색공간 변환들을 모아둔 정적 유틸리티 클래스
// @details Grayscale/Sepia 변환은 버퍼를 in-place로 수정하고, HSV/YCbCr
//          변환은 픽셀 단위 왕복 변환 함수로 제공한다.
//***************************************************************************
class ColorSpace
{
public:
	static void ToGrayscaleInPlace(ImageBuffer& img);
	static void ToSepiaInPlace(ImageBuffer& img);

	static void RgbToHsv(uint8_t r, uint8_t g, uint8_t b, double& h, double& s, double& v);
	static void HsvToRgb(double h, double s, double v, uint8_t& r, uint8_t& g, uint8_t& b);

	static void RgbToYCbCr(uint8_t r, uint8_t g, uint8_t b, uint8_t& y, uint8_t& cb, uint8_t& cr);
	static void YCbCrToRgb(uint8_t y, uint8_t cb, uint8_t cr, uint8_t& r, uint8_t& g, uint8_t& b);

	static void InvertInPlace(ImageBuffer& img);

private:
	static uint8_t Clamp(double v);
};

#endif // ndef UC_COLORSPACE_H
