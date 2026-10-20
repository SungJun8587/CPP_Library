
#include "pch.h"
#include "GifAnimation.h"

//***************************************************************************
// @brief 파일에서 GIF를 읽어 모든 프레임을 디코드
// @param path GIF 파일 경로
// @param destW 0보다 크면 모든 프레임을 이 가로 크기로 리사이즈
// @param destH 0보다 크면 모든 프레임을 이 세로 크기로 리사이즈
// @return 한 프레임 이상 로드되면 true. 실패하면 비어 있는 상태가 된다.
//***************************************************************************
bool GifAnimation::LoadFile(const std::filesystem::path& path, uint32_t destW, uint32_t destH)
{
	Clear();

	std::ifstream fs(path, std::ios::binary | std::ios::ate);
	if( !fs ) return false;

	const std::streamoff len = fs.tellg();
	if( len <= 0 ) return false;

	std::vector<uint8_t> bytes(static_cast<size_t>(len));
	fs.seekg(0);
	if( !fs.read(reinterpret_cast<char*>(bytes.data()), len) ) return false;

	return LoadMemory(bytes.data(), bytes.size(), destW, destH);
}

//***************************************************************************
// @brief 메모리의 GIF를 모든 프레임으로 디코드
// @param data GIF 데이터 시작 주소
// @param size GIF 데이터 길이(바이트)
// @param destW 0보다 크면 모든 프레임을 이 가로 크기로 리사이즈
// @param destH 0보다 크면 모든 프레임을 이 세로 크기로 리사이즈
// @return 한 프레임 이상 로드되면 true. 실패하면 비어 있는 상태가 된다.
//***************************************************************************
bool GifAnimation::LoadMemory(const uint8_t* data, size_t size, uint32_t destW, uint32_t destH)
{
	Clear();
	if( data == nullptr || size == 0 ) return false;

	try
	{
		GifCodec codec;
		std::vector<GifCodec::Frame> frames = codec.DecodeAllFrames(data, size);

		if( destW > 0 && destH > 0 )
		{
			for( GifCodec::Frame& fr : frames )
			{
				if( fr.image.Width() == destW && fr.image.Height() == destH ) continue;

				ImageProcessor proc(std::move(fr.image));
				proc.Resize(destW, destH, ResizeMethod::Bilinear);
				fr.image = std::move(proc.Buffer());
			}
		}

		m_frames = std::move(frames);
	}
	catch( const ImageException& )
	{
		m_frames.clear();
		return false;
	}

	return !m_frames.empty();
}

//***************************************************************************
// @brief 현재 프레임을 보여줄 시간(ms)
// @return 현재 프레임의 지연시간. 비어 있으면 0
//***************************************************************************
int GifAnimation::CurrentDelayMs() const
{
	return m_frames.empty() ? 0 : m_frames[m_nCurrent].DelayMs();
}

//***************************************************************************
// @brief 다음 프레임으로 이동(마지막 프레임 다음은 처음으로 순환)
//***************************************************************************
void GifAnimation::Advance()
{
	if( !m_frames.empty() ) m_nCurrent = (m_nCurrent + 1) % m_frames.size();
}