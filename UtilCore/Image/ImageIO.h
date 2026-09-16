
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

	//***************************************************************************
	// @brief [추가] 파일이 아니라 메모리에 이미 올라와 있는 바이트로부터
	//        직접 디코드한다(시그니처로 포맷 자동 감지, Load()와 동일한
	//        로직 — 디스크 I/O만 없을 뿐). 임시 파일을 거치지 않고 곧바로
	//        업로드된 바이트를 처리하려는 호출부(예: 파일 서버의 리사이즈)를
	//        위해 추가했다.
	// @param data 이미지 파일 바이트 그대로(디스크에 그대로 썼을 때와 동일한
	//        내용) — 확장자 정보가 없으므로 포맷은 시그니처로만 판별한다.
	// @return 디코드된 ImageBuffer
	// @throws ImageException 어떤 등록된 코덱도 이 데이터를 못 읽으면.
	//***************************************************************************
	static ImageBuffer LoadFromMemory(const std::vector<uint8_t>& data);

	//***************************************************************************
	// @brief [추가] 파일에 쓰는 대신 인코딩된 바이트를 그대로 돌려준다
	//        (Save()와 동일한 인코딩 로직, 디스크 쓰기만 생략).
	// @param image 인코딩할 이미지
	// @param format 사용할 인코딩 포맷
	// @return 인코딩된 파일 바이트(그대로 디스크에 쓰면 유효한 이미지 파일이 됨)
	// @throws ImageException format에 맞는 코덱이 없으면.
	//***************************************************************************
	static std::vector<uint8_t> SaveToMemory(const ImageBuffer& image, ImageFormat format);

private:
	static std::vector<std::unique_ptr<ICodec>>& Codecs();
	static const ICodec* FindCodec(ImageFormat format);
	static std::vector<uint8_t> ReadFile(const std::string& path);
	static void WriteFile(const std::string& path, const std::vector<uint8_t>& data);
};

#endif // ndef UC_IMAGEIO_H