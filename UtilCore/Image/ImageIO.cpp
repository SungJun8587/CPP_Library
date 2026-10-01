
#include "pch.h"
#include "ImageIO.h"

//***************************************************************************
// @brief 등록된 코덱 목록을 최초 호출 시 1회 생성하여 반환
// @return 등록된 ICodec 인스턴스 목록에 대한 참조
//***************************************************************************
std::vector<std::unique_ptr<ICodec>>& ImageIO::Codecs()
{
	static std::vector<std::unique_ptr<ICodec>> codecs = [] {
		std::vector<std::unique_ptr<ICodec>> v;
		v.push_back(std::make_unique<PngCodec>());
		v.push_back(std::make_unique<BmpCodec>());
		v.push_back(std::make_unique<GifCodec>());
		v.push_back(std::make_unique<JpegCodec>());
		return v;
		}();
	return codecs;
}

//***************************************************************************
// @brief 지정한 포맷을 처리하는 코덱을 찾음
// @param format 찾을 이미지 포맷
// @return 해당 코덱 포인터(없으면 nullptr)
//***************************************************************************
const ICodec* ImageIO::FindCodec(ImageFormat format)
{
	for( auto& c : Codecs() ) if( c->Format() == format ) return c.get();
	return nullptr;
}

//***************************************************************************
// @brief 파일 전체를 바이너리 모드로 읽어 바이트 버퍼로 반환
// @param path 읽을 파일 경로
// @return 파일 내용을 담은 바이트 버퍼
//***************************************************************************
std::vector<uint8_t> ImageIO::ReadFile(const std::string& path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if( !file ) throw ImageException("ImageIO: cannot open file for reading: " + path);
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	std::vector<uint8_t> buffer(static_cast<size_t>(size));
	if( size > 0 && !file.read(reinterpret_cast<char*>(buffer.data()), size) )
		throw ImageException("ImageIO: failed to read file: " + path);
	return buffer;
}

//***************************************************************************
// @brief 바이트 버퍼를 파일에 바이너리 모드로 기록
// @param path 기록할 파일 경로
// @param data 기록할 바이트 데이터
//***************************************************************************
void ImageIO::WriteFile(const std::string& path, const std::vector<uint8_t>& data)
{
	std::ofstream file(path, std::ios::binary);
	if( !file ) throw ImageException("ImageIO: cannot open file for writing: " + path);
	file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

//***************************************************************************
// @brief 파일을 읽어 시그니처로 포맷을 감지하고 알맞은 코덱으로 디코드
// @param path 로드할 이미지 파일 경로
// @return 디코드된 ImageBuffer
//***************************************************************************
ImageBuffer ImageIO::Load(const std::string& path)
{
	std::vector<uint8_t> data = ReadFile(path);
	try
	{
		return LoadFromMemory(data.data(), data.size());
	}
	catch( const ImageException& )
	{
		throw ImageException("ImageIO: unrecognized file format: " + path);
	}
}

//***************************************************************************
// @brief 메모리 상의 바이트 버퍼를 시그니처로 포맷을 감지해 알맞은 코덱으로 디코드
// @param data 디코드할 이미지 바이트 버퍼
// @param size 바이트 버퍼 길이
// @return 디코드된 ImageBuffer
//***************************************************************************
ImageBuffer ImageIO::LoadFromMemory(const uint8_t* data, size_t size)
{
	for( auto& codec : Codecs() )
	{
		if( codec->CanDecode(data, size) )
		{
			return codec->Decode(data, size);
		}
	}
	throw ImageException("ImageIO: unrecognized image data");
}

//***************************************************************************
// @brief 지정한 포맷의 코덱으로 이미지를 인코드하여 메모리 버퍼로 반환
// @param image 인코드할 이미지
// @param format 사용할 인코딩 포맷
// @return 인코드된 바이트 버퍼
//***************************************************************************
std::vector<uint8_t> ImageIO::SaveToMemory(const ImageBuffer& image, ImageFormat format)
{
	const ICodec* codec = FindCodec(format);
	if( !codec ) throw ImageException("ImageIO: no codec registered for requested format");
	return codec->Encode(image);
}

//***************************************************************************
// @brief 지정한 포맷의 코덱으로 이미지를 인코드하여 파일에 저장
// @param path 저장할 파일 경로
// @param image 저장할 이미지
// @param format 사용할 인코딩 포맷
//***************************************************************************
void ImageIO::Save(const std::string& path, const ImageBuffer& image, ImageFormat format)
{
	const ICodec* codec = FindCodec(format);
	if( !codec ) throw ImageException("ImageIO: no codec registered for requested format");
	std::vector<uint8_t> encoded = codec->Encode(image);
	WriteFile(path, encoded);
}

//***************************************************************************
// @brief 파일 경로의 확장자로부터 이미지 포맷을 추론
// @param path 확장자를 확인할 파일 경로
// @return 추론된 ImageFormat(알 수 없으면 ImageFormat::Unknown)
//***************************************************************************
ImageFormat ImageIO::FormatFromExtension(const std::string& path)
{
	std::string ext;
	size_t dot = path.find_last_of('.');
	if( dot != std::string::npos ) ext = path.substr(dot + 1);
	for( char& c : ext ) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

	if( ext == "bmp" ) return ImageFormat::BMP;
	if( ext == "png" ) return ImageFormat::PNG;
	if( ext == "jpg" || ext == "jpeg" ) return ImageFormat::JPEG;
	if( ext == "gif" ) return ImageFormat::GIF;
	return ImageFormat::Unknown;
}
