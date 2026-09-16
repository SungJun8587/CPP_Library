
#ifndef UC_IMAGEFILTERS_H
#define UC_IMAGEFILTERS_H

#include "ImageTypes.h"
#include <vector>

//***************************************************************************
// @brief 커널 컨볼루션 기반 이미지 필터들을 모아둔 정적 유틸리티 클래스
// @details 경계 픽셀은 클램프(edge clamp) 방식으로 처리하며, 알파 채널은
//          모든 필터에서 원본 값을 그대로 유지한다.
//***************************************************************************
class ImageFilters
{
public:
	static ImageBuffer GaussianBlur(const ImageBuffer& src, int radius = 2, double sigma = 1.4);
	static ImageBuffer Sharpen(const ImageBuffer& src);
	static ImageBuffer EdgeDetect(const ImageBuffer& src);
	static void AdjustBrightnessContrastInPlace(ImageBuffer& img, int brightness, double contrast);

private:
	static uint8_t Clamp(double v);
	static std::vector<double> MakeGaussianKernel(int radius, double sigma);
	static int ClampCoord(int v, int lo, int hi);
	static ImageBuffer ConvolveSeparable(const ImageBuffer& src, const std::vector<double>& kernel, bool horizontal);
	static ImageBuffer Convolve3x3(const ImageBuffer& src, const double kernel[9]);
};

#endif // ndef UC_IMAGEFILTERS_H
