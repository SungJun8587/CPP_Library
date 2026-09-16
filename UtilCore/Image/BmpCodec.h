
#ifndef UC_BMPCODEC_H
#define UC_BMPCODEC_H

#include "ICodec.h"

//***************************************************************************
// @brief 비압축 24/32bit BMP 파일을 디코드/인코드하는 코덱
// @details BMP는 압축 없이 픽셀을 그대로 담는 가장 단순한 포맷이라
//          파싱이 직관적이며, 별도의 압축 해제 로직이 필요 없다.
//***************************************************************************
class BmpCodec : public ICodec
{
public:
	//***************************************************************************
	// @brief 이 코덱이 처리하는 이미지 포맷을 반환
	// @return ImageFormat::BMP
	//***************************************************************************
	ImageFormat Format() const override { return ImageFormat::BMP; }

	bool CanDecode(const uint8_t* data, size_t size) const override;
	ImageBuffer Decode(const uint8_t* data, size_t size) const override;
	std::vector<uint8_t> Encode(const ImageBuffer& image) const override;

private:
	static uint16_t ReadU16(const uint8_t* p);
	static uint32_t ReadU32(const uint8_t* p);
	static void WriteU16(uint8_t* p, uint16_t v);
	static void WriteU32(uint8_t* p, uint32_t v);
};

#endif // ndef UC_BMPCODEC_H
