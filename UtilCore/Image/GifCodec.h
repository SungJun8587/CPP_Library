
#ifndef UC_GIFCODEC_H
#define UC_GIFCODEC_H

#include "ICodec.h"

#include <cstring>
#include <algorithm>
#include <vector>

//***************************************************************************
// @brief GIF87a/89a 이미지를 디코드하는 코덱(인코드는 미구현)
// @details 헤더/블록 파싱, GIF 전용 가변폭 LZW 압축 해제, 인터레이스 해제,
//          Graphic Control Extension 기반 프레임 합성(디스포절 처리 포함)을
//          전부 직접 구현한다. ICodec::Decode()는 첫 프레임만 반환하며,
//          애니메이션 전체가 필요하면 DecodeAllFrames()를 사용한다.
//***************************************************************************
class GifCodec : public ICodec
{
public:
	//***************************************************************************
	// @brief 합성이 끝난 애니메이션 프레임 하나(화면 전체 크기의 최종 이미지)
	//***************************************************************************
	struct Frame
	{
		ImageBuffer image;   // 이 시점에 화면에 보여야 할 합성된 RGBA8 이미지
		int delayCs = 0;     // 다음 프레임까지의 지연시간(1/100초 단위, 0이면 미지정)
	};

	//***************************************************************************
	// @brief 이 코덱이 처리하는 이미지 포맷을 반환
	// @return ImageFormat::GIF
	//***************************************************************************
	ImageFormat Format() const override { return ImageFormat::GIF; }

	bool CanDecode(const uint8_t* data, size_t size) const override;
	ImageBuffer Decode(const uint8_t* data, size_t size) const override;
	std::vector<uint8_t> Encode(const ImageBuffer& image) const override;

	std::vector<Frame> DecodeAllFrames(const uint8_t* data, size_t size) const;

private:
	//***************************************************************************
	// @brief 팔레트 색상 하나(RGB, 알파 없음)
	//***************************************************************************
	struct RgbColor
	{
		uint8_t r = 0; // Red 채널 값
		uint8_t g = 0; // Green 채널 값
		uint8_t b = 0; // Blue 채널 값
	};

	//***************************************************************************
	// @brief 논리 화면 정보(GIF 헤더 + Logical Screen Descriptor 파싱 결과)
	//***************************************************************************
	struct LogicalScreen
	{
		int width = 0;                    // 논리 화면 가로 크기(픽셀)
		int height = 0;                   // 논리 화면 세로 크기(픽셀)
		std::vector<RgbColor> globalTable; // 전역 컬러 테이블(없으면 비어있음)
		int backgroundIndex = 0;          // 배경색의 전역 컬러 테이블 인덱스
	};

	//***************************************************************************
	// @brief 가장 최근 Graphic Control Extension에서 읽은, 다음 이미지에
	//        적용될 프레임 제어 정보
	//***************************************************************************
	struct GraphicControl
	{
		int disposalMethod = 0;      // 디스포절 방식(0~3)
		bool transparentFlag = false; // 투명색 사용 여부
		uint8_t transparentIndex = 0; // 투명 처리할 컬러 테이블 인덱스
		int delayCs = 0;              // 프레임 지연시간(1/100초)
	};

	static size_t ReadLogicalScreen(const uint8_t* data, size_t size, LogicalScreen& screen);
	static std::vector<RgbColor> ReadColorTable(const uint8_t* data, size_t size, size_t& pos, int sizeField);
	static std::vector<uint8_t> CollectSubBlocks(const uint8_t* data, size_t size, size_t& pos);
	static void SkipSubBlocks(const uint8_t* data, size_t size, size_t& pos);
	static std::vector<uint8_t> LzwDecode(const std::vector<uint8_t>& packed, int minCodeSize, size_t expectedPixels);
	static void Deinterlace(std::vector<uint8_t>& indices, int width, int height);
	static void CompositeFrame(ImageBuffer& canvas, const std::vector<uint8_t>& indices,
		int left, int top, int w, int h,
		const std::vector<RgbColor>& palette,
		bool transparentFlag, uint8_t transparentIndex);
	static void ClearRect(ImageBuffer& canvas, int left, int top, int w, int h);
	static ImageBuffer CaptureRect(const ImageBuffer& canvas, int left, int top, int w, int h);
	static void RestoreRect(ImageBuffer& canvas, int left, int top, int w, int h, const ImageBuffer& saved);
};

#endif // UC_GIFCODEC_H
