
#include "pch.h"
#include "ImageFilters.h"

//***************************************************************************
// @brief 값을 0~255 범위로 클램프하며 정수로 반올림
// @param v 클램프할 실수 값
// @return 클램프된 8비트 값
//***************************************************************************
uint8_t ImageFilters::Clamp(double v)
{
	if( v < 0 ) return 0;
	if( v > 255 ) return 255;
	return static_cast<uint8_t>(std::round(v));
}

//***************************************************************************
// @brief 지정한 반경/표준편차로 정규화된 1차원 가우시안 커널을 생성
// @param radius 커널 반경(커널 크기 = radius*2+1)
// @param sigma 가우시안 표준편차
// @return 합이 1이 되도록 정규화된 커널 계수 배열
//***************************************************************************
std::vector<double> ImageFilters::MakeGaussianKernel(int radius, double sigma)
{
	int size = radius * 2 + 1;
	std::vector<double> kernel(size);
	double sum = 0.0;
	for( int i = -radius; i <= radius; ++i )
	{
		double v = std::exp(-(i * i) / (2.0 * sigma * sigma));
		kernel[i + radius] = v;
		sum += v;
	}
	for( double& v : kernel ) v /= sum;
	return kernel;
}

//***************************************************************************
// @brief 좌표 값을 [lo, hi] 범위로 클램프(경계 픽셀 처리용)
// @param v 클램프할 좌표 값
// @param lo 최소값
// @param hi 최대값
// @return 클램프된 좌표 값
//***************************************************************************
int ImageFilters::ClampCoord(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

//***************************************************************************
// @brief 1차원 커널을 가로 또는 세로 방향으로 분리 적용하는 컨볼루션(경계 클램프)
// @param src 원본 이미지
// @param kernel 적용할 1차원 커널 계수
// @param horizontal true면 가로 방향, false면 세로 방향으로 적용
// @return 컨볼루션이 적용된 새 이미지(알파는 원본 유지)
//***************************************************************************
ImageBuffer ImageFilters::ConvolveSeparable(const ImageBuffer& src, const std::vector<double>& kernel, bool horizontal)
{
	int radius = static_cast<int>(kernel.size() / 2);
	ImageBuffer dst(src.Width(), src.Height());
	int w = static_cast<int>(src.Width()), h = static_cast<int>(src.Height());

	for( int y = 0; y < h; ++y )
	{
		for( int x = 0; x < w; ++x )
		{
			double sum[3] = { 0, 0, 0 };
			for( int k = -radius; k <= radius; ++k )
			{
				int sx = horizontal ? ClampCoord(x + k, 0, w - 1) : x;
				int sy = horizontal ? y : ClampCoord(y + k, 0, h - 1);
				const uint8_t* p = src.At(sx, sy);
				double weight = kernel[k + radius];
				sum[0] += p[0] * weight;
				sum[1] += p[1] * weight;
				sum[2] += p[2] * weight;
			}
			uint8_t* d = dst.At(x, y);
			d[0] = Clamp(sum[0]); d[1] = Clamp(sum[1]); d[2] = Clamp(sum[2]);
			d[3] = src.At(x, y)[3];
		}
	}
	return dst;
}

//***************************************************************************
// @brief 3x3 커널을 경계 클램프 방식으로 이미지 전체에 컨볼루션 적용
// @param src 원본 이미지
// @param kernel 적용할 3x3 커널 계수(행 우선, 9개)
// @return 컨볼루션이 적용된 새 이미지(알파는 원본 유지)
//***************************************************************************
ImageBuffer ImageFilters::Convolve3x3(const ImageBuffer& src, const double kernel[9])
{
	ImageBuffer dst(src.Width(), src.Height());
	int w = static_cast<int>(src.Width()), h = static_cast<int>(src.Height());

	for( int y = 0; y < h; ++y )
	{
		for( int x = 0; x < w; ++x )
		{
			double sum[3] = { 0, 0, 0 };
			int k = 0;
			for( int dy = -1; dy <= 1; ++dy )
			{
				for( int dx = -1; dx <= 1; ++dx )
				{
					int sx = ClampCoord(x + dx, 0, w - 1);
					int sy = ClampCoord(y + dy, 0, h - 1);
					const uint8_t* p = src.At(sx, sy);
					double weight = kernel[k++];
					sum[0] += p[0] * weight;
					sum[1] += p[1] * weight;
					sum[2] += p[2] * weight;
				}
			}
			uint8_t* d = dst.At(x, y);
			d[0] = Clamp(sum[0]); d[1] = Clamp(sum[1]); d[2] = Clamp(sum[2]);
			d[3] = src.At(x, y)[3];
		}
	}
	return dst;
}

//***************************************************************************
// @brief 분리 가능한 가우시안 커널로 이미지를 블러 처리
// @param src 원본 이미지
// @param radius 가우시안 커널 반경
// @param sigma 가우시안 표준편차
// @return 블러 처리된 새 이미지
//***************************************************************************
ImageBuffer ImageFilters::GaussianBlur(const ImageBuffer& src, int radius, double sigma)
{
	std::vector<double> kernel = MakeGaussianKernel(radius, sigma);
	ImageBuffer horiz = ConvolveSeparable(src, kernel, true);
	return ConvolveSeparable(horiz, kernel, false);
}

//***************************************************************************
// @brief 3x3 샤픈 커널을 적용하여 이미지를 선명하게 처리
// @param src 원본 이미지
// @return 샤픈 처리된 새 이미지
//***************************************************************************
ImageBuffer ImageFilters::Sharpen(const ImageBuffer& src)
{
	static const double kernel[9] = {
		 0, -1,  0,
		-1,  5, -1,
		 0, -1,  0
	};
	return Convolve3x3(src, kernel);
}

//***************************************************************************
// @brief 3x3 라플라시안 커널을 적용하여 이미지의 엣지를 검출
// @param src 원본 이미지
// @return 엣지 검출 결과 이미지
//***************************************************************************
ImageBuffer ImageFilters::EdgeDetect(const ImageBuffer& src)
{
	static const double kernel[9] = {
		-1, -1, -1,
		-1,  8, -1,
		-1, -1, -1
	};
	return Convolve3x3(src, kernel);
}

//***************************************************************************
// @brief 이미지의 밝기와 대비를 조정(in-place, 알파는 유지)
// @param img 조정할 이미지 버퍼
// @param brightness 밝기 가산값(-255~255)
// @param contrast 대비 배율(1.0=변화 없음)
//***************************************************************************
void ImageFilters::AdjustBrightnessContrastInPlace(ImageBuffer& img, int brightness, double contrast)
{
	for( size_t i = 0; i < img.DataSize(); i += 4 )
	{
		for( int c = 0; c < 3; ++c )
		{
			double v = (img.Data()[i + c] - 128.0) * contrast + 128.0 + brightness;
			img.Data()[i + c] = Clamp(v);
		}
	}
}