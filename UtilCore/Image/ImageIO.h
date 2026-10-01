
#ifndef UC_IMAGEIO_H
#define UC_IMAGEIO_H

#include "ICodec.h"
#include "BmpCodec.h"
#include "PngCodec.h"
#include "JpegCodec.h"

#include <memory>
#include <string>
#include <fstream>
#include <cctype>

//***************************************************************************
// @brief 파일 시그니처/확장자로 포맷을 자동 감지해 등록된 코덱에 위임하는
//        로드/저장 유틸리티 클래스
// @details 파일 입출력에 표준 <fstream>만 사용하므로 Windows/Linux/macOS에서
//          동일하게 동작한다.
//***************************************************************************
class ImageIO
{
public:
	static ImageBuffer Load(const std::string& path);
	static void Save(const std::string& path, const ImageBuffer& image, ImageFormat format);
	static ImageFormat FormatFromExtension(const std::string& path);

	// 파일이 아닌 메모리 버퍼 기반 인코드/디코드(이미 메모리에 있는 바이트를
	// 다루거나, 디스크를 거치지 않고 네트워크/DB 등으로 바로 보낼 때 사용).
	static std::vector<uint8_t> SaveToMemory(const ImageBuffer& image, ImageFormat format);
	static ImageBuffer LoadFromMemory(const uint8_t* data, size_t size);

private:
	static std::vector<std::unique_ptr<ICodec>>& Codecs();
	static const ICodec* FindCodec(ImageFormat format);
	static std::vector<uint8_t> ReadFile(const std::string& path);
	static void WriteFile(const std::string& path, const std::vector<uint8_t>& data);
};


#endif // ndef UC_IMAGEIO_H