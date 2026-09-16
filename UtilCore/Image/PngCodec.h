
#ifndef UC_PNGCODEC_H
#define UC_PNGCODEC_H

#include "ICodec.h"
#include "Inflate.h"
#include "Deflate.h"
#include "Checksum.h"

#include <vector>
#include <cstring>
#include <cstdlib>

//***************************************************************************
// @brief PNG 파일을 디코드/인코드하는 코덱
// @details 청크 파싱, zlib(DEFLATE) 압축해제/압축, 스캔라인 필터링(Sub/Up/
//          Average/Paeth)까지 외부 라이브러리 없이 직접 구현한다. 인코드 시
//          스캔라인마다 5종 필터를 모두 시도해 절대값 합이 최소인 필터를
//          선택하는 적응형 필터링(libpng와 동일한 휴리스틱)을 적용한다.
//***************************************************************************
class PngCodec : public ICodec
{
public:
	//***************************************************************************
	// @brief 이 코덱이 처리하는 이미지 포맷을 반환
	// @return ImageFormat::PNG
	//***************************************************************************
	ImageFormat Format() const override { return ImageFormat::PNG; }

	bool CanDecode(const uint8_t* data, size_t size) const override;
	ImageBuffer Decode(const uint8_t* data, size_t size) const override;
	std::vector<uint8_t> Encode(const ImageBuffer& image) const override;

private:
	static int ChannelsForColorType(uint8_t colorType);
	static uint8_t PaethPredictor(int a, int b, int c);
	static void Unfilter(const std::vector<uint8_t>& raw, std::vector<uint8_t>& out,
		uint32_t width, uint32_t height, int channels);
	static int FilterRow(const uint8_t* curRow, const uint8_t* prevRow, uint32_t stride,
		int channels, uint8_t* outFiltered);

	static uint32_t ReadU32(const uint8_t* p);
	static void WriteU32(uint8_t* p, uint32_t v);
	static void WriteChunk(std::vector<uint8_t>& out, const char* type,
		const uint8_t* data, size_t len);
};

#endif // ndef UC_PNGCODEC_H
