
#include "pch.h"
#include "ImageProcessor.h"

//***************************************************************************
// @brief 파일을 로드하여 내부 이미지를 교체
// @param path 로드할 이미지 파일 경로
// @return 체이닝을 위한 자기 자신에 대한 참조
//***************************************************************************
ImageProcessor& ImageProcessor::Load(const std::string& path)
{
	image_ = ImageIO::Load(path);
	return *this;
}

//***************************************************************************
// @brief [추가] 메모리 바이트로부터 로드하여 내부 이미지를 교체
// @param data 이미지 파일 바이트 그대로
// @return 체이닝을 위한 자기 자신에 대한 참조
//***************************************************************************
ImageProcessor& ImageProcessor::LoadFromMemory(const std::vector<uint8_t>& data)
{
	image_ = ImageIO::LoadFromMemory(data);
	return *this;
}

//***************************************************************************
// @brief 파일 확장자로부터 포맷을 추론하여 현재 이미지를 저장
// @param path 저장할 파일 경로
// @return 체이닝을 위한 자기 자신에 대한 상수 참조
//***************************************************************************
const ImageProcessor& ImageProcessor::Save(const std::string& path) const
{
	ImageFormat fmt = ImageIO::FormatFromExtension(path);
	if( fmt == ImageFormat::Unknown ) throw ImageException("ImageProcessor: unknown output extension for " + path);
	ImageIO::Save(path, image_, fmt);
	return *this;
}

//***************************************************************************
// @brief 지정한 포맷으로 현재 이미지를 저장
// @param path 저장할 파일 경로
// @param format 사용할 인코딩 포맷
// @return 체이닝을 위한 자기 자신에 대한 상수 참조
//***************************************************************************
const ImageProcessor& ImageProcessor::Save(const std::string& path, ImageFormat format) const
{
	ImageIO::Save(path, image_, format);
	return *this;
}

//***************************************************************************
// @brief [추가] 파일에 쓰는 대신 인코딩된 바이트를 그대로 돌려받는다
// @param format 사용할 인코딩 포맷
// @return 인코딩된 파일 바이트
//***************************************************************************
std::vector<uint8_t> ImageProcessor::SaveToMemory(ImageFormat format) const
{
	return ImageIO::SaveToMemory(image_, format);
}

//***************************************************************************
// @brief (srcX, srcY) 실수 좌표 주변 4개 픽셀을 양선형 보간하여 샘플링
// @param srcX 샘플링할 원본 이미지의 가로 실수 좌표
// @param srcY 샘플링할 원본 이미지의 세로 실수 좌표
// @param out 보간된 RGBA 값을 채울 4바이트 출력 버퍼
//***************************************************************************
void ImageProcessor::BilinearSample(double srcX, double srcY, uint8_t* out) const
{
	uint32_t x0 = static_cast<uint32_t>(srcX);
	uint32_t y0 = static_cast<uint32_t>(srcY);
	uint32_t x1 = std::min(x0 + 1, image_.Width() - 1);
	uint32_t y1 = std::min(y0 + 1, image_.Height() - 1);
	double fx = srcX - x0, fy = srcY - y0;

	const uint8_t* p00 = image_.At(x0, y0);
	const uint8_t* p10 = image_.At(x1, y0);
	const uint8_t* p01 = image_.At(x0, y1);
	const uint8_t* p11 = image_.At(x1, y1);

	for( int c = 0; c < 4; ++c )
	{
		double top = p00[c] * (1 - fx) + p10[c] * fx;
		double bot = p01[c] * (1 - fx) + p11[c] * fx;
		out[c] = static_cast<uint8_t>(std::round(top * (1 - fy) + bot * fy));
	}
}

//***************************************************************************
// @brief 이미지를 지정한 크기로 리샘플링
// @param newWidth 새 가로 크기(픽셀)
// @param newHeight 새 세로 크기(픽셀)
// @param method 리샘플링 방식(기본값 Bilinear)
// @return 체이닝을 위한 자기 자신에 대한 참조
//***************************************************************************
ImageProcessor& ImageProcessor::Resize(uint32_t newWidth, uint32_t newHeight, ResizeMethod method)
{
	ImageBuffer dst(newWidth, newHeight);
	double sx = static_cast<double>(image_.Width()) / newWidth;
	double sy = static_cast<double>(image_.Height()) / newHeight;

	for( uint32_t y = 0; y < newHeight; ++y )
	{
		for( uint32_t x = 0; x < newWidth; ++x )
		{
			if( method == ResizeMethod::Nearest )
			{
				uint32_t srcX = std::min<uint32_t>(static_cast<uint32_t>(x * sx), image_.Width() - 1);
				uint32_t srcY = std::min<uint32_t>(static_cast<uint32_t>(y * sy), image_.Height() - 1);
				std::memcpy(dst.At(x, y), image_.At(srcX, srcY), 4);
			}
			else
			{
				BilinearSample(x * sx, y * sy, dst.At(x, y));
			}
		}
	}
	image_ = std::move(dst);
	return *this;
}

//***************************************************************************
// @brief 이미지를 지정한 사각 영역으로 잘라냄
// @param x 잘라낼 영역의 좌상단 가로 좌표
// @param y 잘라낼 영역의 좌상단 세로 좌표
// @param w 잘라낼 영역의 가로 크기
// @param h 잘라낼 영역의 세로 크기
// @return 체이닝을 위한 자기 자신에 대한 참조
//***************************************************************************
ImageProcessor& ImageProcessor::Crop(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	if( x + w > image_.Width() || y + h > image_.Height() )
		throw ImageException("ImageProcessor::Crop: region out of bounds");
	ImageBuffer dst(w, h);
	for( uint32_t row = 0; row < h; ++row )
	{
		std::memcpy(dst.At(0, row), image_.At(x, y + row), static_cast<size_t>(w) * 4);
	}
	image_ = std::move(dst);
	return *this;
}

//***************************************************************************
// @brief 이미지를 지정한 각도만큼 시계 방향으로 회전
// @param angle 회전 각도(CW90/CW180/CW270)
// @return 체이닝을 위한 자기 자신에 대한 참조
//***************************************************************************
ImageProcessor& ImageProcessor::Rotate(RotateAngle angle)
{
	uint32_t w = image_.Width(), h = image_.Height();
	if( angle == RotateAngle::CW180 )
	{
		ImageBuffer dst(w, h);
		for( uint32_t y = 0; y < h; ++y )
			for( uint32_t x = 0; x < w; ++x )
				std::memcpy(dst.At(w - 1 - x, h - 1 - y), image_.At(x, y), 4);
		image_ = std::move(dst);
	}
	else
	{
		ImageBuffer dst(h, w);
		for( uint32_t y = 0; y < h; ++y )
		{
			for( uint32_t x = 0; x < w; ++x )
			{
				uint32_t dx, dy;
				if( angle == RotateAngle::CW90 ) { dx = h - 1 - y; dy = x; }
				else { dx = y;         dy = w - 1 - x; }
				std::memcpy(dst.At(dx, dy), image_.At(x, y), 4);
			}
		}
		image_ = std::move(dst);
	}
	return *this;
}

//***************************************************************************
// @brief 이미지를 좌우로 뒤집음(in-place)
// @return 체이닝을 위한 자기 자신에 대한 참조
//***************************************************************************
ImageProcessor& ImageProcessor::FlipHorizontal()
{
	uint32_t w = image_.Width(), h = image_.Height();
	for( uint32_t y = 0; y < h; ++y )
		for( uint32_t x = 0; x < w / 2; ++x )
		{
			uint8_t tmp[4];
			std::memcpy(tmp, image_.At(x, y), 4);
			std::memcpy(image_.At(x, y), image_.At(w - 1 - x, y), 4);
			std::memcpy(image_.At(w - 1 - x, y), tmp, 4);
		}
	return *this;
}

//***************************************************************************
// @brief 이미지를 상하로 뒤집음(in-place)
// @return 체이닝을 위한 자기 자신에 대한 참조
//***************************************************************************
ImageProcessor& ImageProcessor::FlipVertical()
{
	uint32_t w = image_.Width(), h = image_.Height();
	std::vector<uint8_t> tmpRow(static_cast<size_t>(w) * 4);
	for( uint32_t y = 0; y < h / 2; ++y )
	{
		uint8_t* rowTop = image_.At(0, y);
		uint8_t* rowBot = image_.At(0, h - 1 - y);
		std::memcpy(tmpRow.data(), rowTop, tmpRow.size());
		std::memcpy(rowTop, rowBot, tmpRow.size());
		std::memcpy(rowBot, tmpRow.data(), tmpRow.size());
	}
	return *this;
}

//***************************************************************************
// @brief 이미지를 그레이스케일로 변환
// @return 체이닝을 위한 자기 자신에 대한 참조
//***************************************************************************
ImageProcessor& ImageProcessor::ToGrayscale() { ColorSpace::ToGrayscaleInPlace(image_); return *this; }

//***************************************************************************
// @brief 이미지를 세피아 톤으로 변환
// @return 체이닝을 위한 자기 자신에 대한 참조
//***************************************************************************
ImageProcessor& ImageProcessor::ToSepia() { ColorSpace::ToSepiaInPlace(image_);     return *this; }

//***************************************************************************
// @brief 이미지의 RGB 채널을 반전
// @return 체이닝을 위한 자기 자신에 대한 참조
//***************************************************************************
ImageProcessor& ImageProcessor::Invert() { ColorSpace::InvertInPlace(image_);     return *this; }

//***************************************************************************
// @brief 이미지에 가우시안 블러를 적용
// @param radius 가우시안 커널 반경
// @param sigma 가우시안 표준편차
// @return 체이닝을 위한 자기 자신에 대한 참조
//***************************************************************************
ImageProcessor& ImageProcessor::GaussianBlur(int radius, double sigma)
{
	image_ = ImageFilters::GaussianBlur(image_, radius, sigma);
	return *this;
}

//***************************************************************************
// @brief 이미지에 샤픈 필터를 적용
// @return 체이닝을 위한 자기 자신에 대한 참조
//***************************************************************************
ImageProcessor& ImageProcessor::Sharpen() { image_ = ImageFilters::Sharpen(image_);    return *this; }

//***************************************************************************
// @brief 이미지에 엣지 검출 필터를 적용
// @return 체이닝을 위한 자기 자신에 대한 참조
//***************************************************************************
ImageProcessor& ImageProcessor::EdgeDetect() { image_ = ImageFilters::EdgeDetect(image_); return *this; }

//***************************************************************************
// @brief 이미지의 밝기와 대비를 조정
// @param brightness 밝기 가산값(-255~255)
// @param contrast 대비 배율(1.0=변화 없음)
// @return 체이닝을 위한 자기 자신에 대한 참조
//***************************************************************************
ImageProcessor& ImageProcessor::AdjustBrightnessContrast(int brightness, double contrast)
{
	ImageFilters::AdjustBrightnessContrastInPlace(image_, brightness, contrast);
	return *this;
}