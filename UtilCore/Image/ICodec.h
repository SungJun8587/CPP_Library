
#ifndef UC_ICODEC_H
#define UC_ICODEC_H

#include <vector>
#include <cstdint>
#include "ImageTypes.h"

//***************************************************************************
// @brief 이미지 파일 포맷 코덱이 구현해야 하는 공통 인터페이스
// @details CanDecode()로 시그니처를 판별하고, Decode()/Encode()로
//          ImageBuffer <-> 파일 바이트 시퀀스 간 변환을 수행한다.
//***************************************************************************
class ICodec
{
public:
	//***************************************************************************
	// @brief 기본 가상 소멸자
	//***************************************************************************
	virtual ~ICodec() = default;

	virtual ImageFormat Format() const = 0;
	virtual bool CanDecode(const uint8_t* data, size_t size) const = 0;
	virtual ImageBuffer Decode(const uint8_t* data, size_t size) const = 0;
	virtual std::vector<uint8_t> Encode(const ImageBuffer& image) const = 0;
};

#endif // ndef UC_ICODEC_H
