
#ifndef UC_GIFANIMATION_H
#define UC_GIFANIMATION_H

#include "GifCodec.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

//***************************************************************************
// @brief GIF 애니메이션 재생 상태(프레임 목록 + 현재 프레임)를 보관하는 공용 클래스
// @details 디코드(GifCodec)와 화면 출력/타이머(UI)는 다루지 않는다. 호출 측은
//          Current()를 그리고, CurrentDelayMs() 후에 Advance()를 호출하면 된다.
//          정적 GIF(프레임 1개)도 같은 방식으로 다룬다.
//***************************************************************************
class GifAnimation
{
public:
	bool LoadFile(const std::filesystem::path& path, uint32_t destW = 0, uint32_t destH = 0);
	bool LoadMemory(const uint8_t* data, size_t size, uint32_t destW = 0, uint32_t destH = 0);

	//***************************************************************************
	// @brief 모든 프레임을 해제하고 비어 있는 상태로 되돌림
	//***************************************************************************
	void Clear() { m_frames.clear(); m_nCurrent = 0; }

	// 로드된 프레임이 없으면 true
	bool Empty() const { return m_frames.empty(); }

	// 프레임이 2개 이상이라 타이머 구동이 필요하면 true
	bool IsAnimated() const { return m_frames.size() > 1; }

	// 전체 프레임 개수
	size_t FrameCount() const { return m_frames.size(); }

	// 현재 프레임 위치(0부터)
	size_t CurrentIndex() const { return m_nCurrent; }

	//***************************************************************************
	// @brief 현재 프레임의 합성된 이미지
	// @details Empty()가 true일 때는 호출하면 안 된다.
	//***************************************************************************
	const ImageBuffer& Current() const { return m_frames[m_nCurrent].image; }

	//***************************************************************************
	// @brief i번째 프레임(합성 완료 이미지와 지연시간)
	// @param i 프레임 위치(0 <= i < FrameCount())
	//***************************************************************************
	const GifCodec::Frame& FrameAt(size_t i) const { return m_frames[i]; }

	int CurrentDelayMs() const;
	void Advance();

	//***************************************************************************
	// @brief 첫 프레임으로 되돌림
	//***************************************************************************
	void Rewind() { m_nCurrent = 0; }

private:
	std::vector<GifCodec::Frame> m_frames; // 합성 완료된 프레임 목록
	size_t                       m_nCurrent = 0; // 현재 프레임 위치
};

#endif // ndef UC_GIFANIMATION_H