
#include "pch.h"
#include "ColorSpace.h"

//***************************************************************************
// @brief 값을 0~255 범위로 클램프하며 정수로 반올림
// @param v 클램프할 실수 값
// @return 클램프된 8비트 값
//***************************************************************************
uint8_t ColorSpace::Clamp(double v)
{
	if( v < 0 ) return 0;
	if( v > 255 ) return 255;
	return static_cast<uint8_t>(std::round(v));
}

//***************************************************************************
// @brief 이미지를 ITU-R BT.601 휘도 가중치로 그레이스케일로 변환(in-place)
// @param img 변환할 이미지 버퍼
//***************************************************************************
void ColorSpace::ToGrayscaleInPlace(ImageBuffer& img)
{
	for( uint32_t y = 0; y < img.Height(); ++y )
	{
		for( uint32_t x = 0; x < img.Width(); ++x )
		{
			uint8_t* p = img.At(x, y);
			uint8_t gray = static_cast<uint8_t>(0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2]);
			p[0] = p[1] = p[2] = gray;
		}
	}
}

//***************************************************************************
// @brief 이미지를 세피아 톤으로 변환(in-place)
// @param img 변환할 이미지 버퍼
//***************************************************************************
void ColorSpace::ToSepiaInPlace(ImageBuffer& img)
{
	for( uint32_t y = 0; y < img.Height(); ++y )
	{
		for( uint32_t x = 0; x < img.Width(); ++x )
		{
			uint8_t* p = img.At(x, y);
			double r = p[0], g = p[1], b = p[2];
			p[0] = Clamp(0.393 * r + 0.769 * g + 0.189 * b);
			p[1] = Clamp(0.349 * r + 0.686 * g + 0.168 * b);
			p[2] = Clamp(0.272 * r + 0.534 * g + 0.131 * b);
		}
	}
}

//***************************************************************************
// @brief RGB 픽셀을 HSV로 변환
// @param r Red 채널 값
// @param g Green 채널 값
// @param b Blue 채널 값
// @param h 계산된 Hue(0~360) 출력
// @param s 계산된 Saturation(0~1) 출력
// @param v 계산된 Value(0~1) 출력
//***************************************************************************
void ColorSpace::RgbToHsv(uint8_t r, uint8_t g, uint8_t b, double& h, double& s, double& v)
{
	double rf = r / 255.0, gf = g / 255.0, bf = b / 255.0;
	double maxC = std::max({ rf, gf, bf });
	double minC = std::min({ rf, gf, bf });
	double delta = maxC - minC;

	v = maxC;
	s = (maxC == 0.0) ? 0.0 : delta / maxC;

	if( delta == 0.0 ) { h = 0.0; return; }
	if( maxC == rf )      h = 60.0 * std::fmod(((gf - bf) / delta), 6.0);
	else if( maxC == gf ) h = 60.0 * (((bf - rf) / delta) + 2.0);
	else                 h = 60.0 * (((rf - gf) / delta) + 4.0);
	if( h < 0 ) h += 360.0;
}

//***************************************************************************
// @brief HSV 값을 RGB 픽셀로 변환
// @param h Hue(0~360)
// @param s Saturation(0~1)
// @param v Value(0~1)
// @param r 계산된 Red 채널 값 출력
// @param g 계산된 Green 채널 값 출력
// @param b 계산된 Blue 채널 값 출력
//***************************************************************************
void ColorSpace::HsvToRgb(double h, double s, double v, uint8_t& r, uint8_t& g, uint8_t& b)
{
	double c = v * s;
	double x = c * (1 - std::fabs(std::fmod(h / 60.0, 2.0) - 1));
	double m = v - c;
	double rf, gf, bf;
	if( h < 60 ) { rf = c; gf = x; bf = 0; }
	else if( h < 120 ) { rf = x; gf = c; bf = 0; }
	else if( h < 180 ) { rf = 0; gf = c; bf = x; }
	else if( h < 240 ) { rf = 0; gf = x; bf = c; }
	else if( h < 300 ) { rf = x; gf = 0; bf = c; }
	else { rf = c; gf = 0; bf = x; }
	r = Clamp((rf + m) * 255.0);
	g = Clamp((gf + m) * 255.0);
	b = Clamp((bf + m) * 255.0);
}

//***************************************************************************
// @brief RGB 픽셀을 YCbCr로 변환(JPEG/BT.601 풀레인지 계수)
// @param r Red 채널 값
// @param g Green 채널 값
// @param b Blue 채널 값
// @param y 계산된 휘도(Y) 출력
// @param cb 계산된 색차(Cb) 출력
// @param cr 계산된 색차(Cr) 출력
//***************************************************************************
void ColorSpace::RgbToYCbCr(uint8_t r, uint8_t g, uint8_t b, uint8_t& y, uint8_t& cb, uint8_t& cr)
{
	y = Clamp(0.299 * r + 0.587 * g + 0.114 * b);
	cb = Clamp(128 - 0.168736 * r - 0.331264 * g + 0.5 * b);
	cr = Clamp(128 + 0.5 * r - 0.418688 * g - 0.081312 * b);
}

//***************************************************************************
// @brief YCbCr 값을 RGB 픽셀로 변환(JPEG/BT.601 풀레인지 계수)
// @param y 휘도(Y)
// @param cb 색차(Cb)
// @param cr 색차(Cr)
// @param r 계산된 Red 채널 값 출력
// @param g 계산된 Green 채널 값 출력
// @param b 계산된 Blue 채널 값 출력
//***************************************************************************
void ColorSpace::YCbCrToRgb(uint8_t y, uint8_t cb, uint8_t cr, uint8_t& r, uint8_t& g, uint8_t& b)
{
	r = Clamp(y + 1.402 * (cr - 128));
	g = Clamp(y - 0.344136 * (cb - 128) - 0.714136 * (cr - 128));
	b = Clamp(y + 1.772 * (cb - 128));
}

//***************************************************************************
// @brief 이미지의 RGB 채널을 반전(in-place, 알파는 유지)
// @param img 변환할 이미지 버퍼
//***************************************************************************
void ColorSpace::InvertInPlace(ImageBuffer& img)
{
	for( size_t i = 0; i < img.DataSize(); i += 4 )
	{
		img.Data()[i] = 255 - img.Data()[i];
		img.Data()[i + 1] = 255 - img.Data()[i + 1];
		img.Data()[i + 2] = 255 - img.Data()[i + 2];
	}
}