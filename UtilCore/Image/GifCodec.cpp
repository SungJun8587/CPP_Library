
#include "pch.h"
#include "GifCodec.h"

//***************************************************************************
// @brief 데이터가 GIF 시그니처("GIF87a" 또는 "GIF89a")로 시작하는지 확인
// @param data 확인할 데이터 시작 주소
// @param size 데이터 길이(바이트)
// @return GIF로 판단되면 true
//***************************************************************************
bool GifCodec::CanDecode(const uint8_t* data, size_t size) const
{
	if( size < 6 ) return false;
	return std::memcmp(data, "GIF87a", 6) == 0 || std::memcmp(data, "GIF89a", 6) == 0;
}

//***************************************************************************
// @brief 지정한 크기의 컬러 테이블을 읽음(전역/지역 공용)
// @param data GIF 파일 데이터 시작 주소
// @param size GIF 파일 데이터 길이
// @param pos 읽기 시작 위치(읽은 만큼 전진시켜 반환)
// @param sizeField Packed 필드의 컬러 테이블 크기 비트(0~7, 테이블 크기 = 2^(field+1))
// @return 읽은 RGB 팔레트
//***************************************************************************
std::vector<GifCodec::RgbColor> GifCodec::ReadColorTable(const uint8_t* data, size_t size, size_t& pos, int sizeField)
{
	int count = 1 << (sizeField + 1);
	std::vector<RgbColor> table(count);
	for( int i = 0; i < count; ++i )
	{
		if( pos + 3 > size ) throw ImageException("GIF: truncated color table");
		table[i].r = data[pos];
		table[i].g = data[pos + 1];
		table[i].b = data[pos + 2];
		pos += 3;
	}
	return table;
}

//***************************************************************************
// @brief GIF 헤더(6바이트) + Logical Screen Descriptor(7바이트)와, 있다면
//        전역 컬러 테이블까지 읽음
// @param data GIF 파일 데이터 시작 주소
// @param size GIF 파일 데이터 길이
// @param screen 채워질 논리 화면 정보
// @return 다음 블록이 시작되는 바이트 오프셋
//***************************************************************************
size_t GifCodec::ReadLogicalScreen(const uint8_t* data, size_t size, LogicalScreen& screen)
{
	if( size < 13 ) throw ImageException("GIF: file too small");

	screen.width = data[6] | (data[7] << 8);
	screen.height = data[8] | (data[9] << 8);
	uint8_t packed = data[10];
	screen.backgroundIndex = data[11];
	// data[12]는 pixel aspect ratio (사용하지 않음)

	size_t pos = 13;
	bool globalTableFlag = (packed & 0x80) != 0;
	int globalTableSize = packed & 0x07;
	if( globalTableFlag )
	{
		screen.globalTable = ReadColorTable(data, size, pos, globalTableSize);
	}
	return pos;
}

//***************************************************************************
// @brief GIF 데이터 서브블록(길이 바이트 + 데이터, 0바이트 종결)을 하나의
//        연속된 버퍼로 이어붙여 반환
// @param data GIF 파일 데이터 시작 주소
// @param size GIF 파일 데이터 길이
// @param pos 읽기 시작 위치(종결 바이트까지 읽은 만큼 전진시켜 반환)
// @return 이어붙인 원시 바이트 시퀀스
//***************************************************************************
std::vector<uint8_t> GifCodec::CollectSubBlocks(const uint8_t* data, size_t size, size_t& pos)
{
	std::vector<uint8_t> out;
	while( pos < size )
	{
		uint8_t blockSize = data[pos++];
		if( blockSize == 0 ) break; // 블록 종결자
		if( pos + blockSize > size ) throw ImageException("GIF: truncated sub-block");
		out.insert(out.end(), data + pos, data + pos + blockSize);
		pos += blockSize;
	}
	return out;
}

//***************************************************************************
// @brief 내용을 사용하지 않는 서브블록 시퀀스를 건너뜀(주석/플레인텍스트/
//        애플리케이션 확장 등)
// @param data GIF 파일 데이터 시작 주소
// @param size GIF 파일 데이터 길이
// @param pos 건너뛸 시작 위치(종결 바이트까지 건너뛴 만큼 전진시켜 반환)
//***************************************************************************
void GifCodec::SkipSubBlocks(const uint8_t* data, size_t size, size_t& pos)
{
	while( pos < size )
	{
		uint8_t blockSize = data[pos++];
		if( blockSize == 0 ) break;
		if( pos + blockSize > size ) throw ImageException("GIF: truncated sub-block");
		pos += blockSize;
	}
}

//***************************************************************************
// @brief GIF 전용 가변폭 LZW 압축 데이터를 컬러 인덱스 시퀀스로 해제
// @param packed CollectSubBlocks로 이어붙인 압축 원시 바이트
// @param minCodeSize 이미지 블록에 저장된 LZW 최소 코드 크기
// @param expectedPixels 예상 픽셀(인덱스) 개수 - 손상된 스트림에 대한 안전장치
// @return 해제된 컬러 인덱스 시퀀스(팔레트 인덱스)
//***************************************************************************
std::vector<uint8_t> GifCodec::LzwDecode(const std::vector<uint8_t>& packed, int minCodeSize, size_t expectedPixels)
{
	if( minCodeSize < 2 || minCodeSize > 8 ) throw ImageException("GIF: invalid LZW minimum code size");

	const int clearCode = 1 << minCodeSize;
	const int endCode = clearCode + 1;

	std::vector<std::vector<uint8_t>> dict;
	int codeSize = minCodeSize + 1;
	int nextCode = endCode + 1;

	auto resetDict = [&]() {
		dict.assign(clearCode + 2, std::vector<uint8_t>());
		for( int i = 0; i < clearCode; ++i ) dict[i].assign(1, static_cast<uint8_t>(i));
		nextCode = endCode + 1;
		codeSize = minCodeSize + 1;
		};
	resetDict();

	std::vector<uint8_t> output;
	output.reserve(expectedPixels);

	size_t bitPos = 0;
	size_t totalBits = packed.size() * 8;
	int prevCode = -1;

	auto readCode = [&](int size) -> int {
		if( bitPos + size > totalBits ) return -1;
		int code = 0;
		for( int i = 0; i < size; ++i )
		{
			size_t bytePos = bitPos >> 3;
			int bitIndex = static_cast<int>(bitPos & 7);
			int bit = (packed[bytePos] >> bitIndex) & 1;
			code |= (bit << i);
			++bitPos;
		}
		return code;
		};

	while( output.size() < expectedPixels + 1 )
	{ // +1 여유: 마지막 엔트리 처리 후 자연 종료 허용
		int code = readCode(codeSize);
		if( code < 0 || code == endCode ) break;

		if( code == clearCode )
		{
			resetDict();
			prevCode = -1;
			continue;
		}

		std::vector<uint8_t> entry;
		if( code < static_cast<int>(dict.size()) && !dict[code].empty() )
		{
			entry = dict[code];
		}
		else if( code == nextCode && prevCode >= 0 && prevCode < static_cast<int>(dict.size()) )
		{
			entry = dict[prevCode];
			entry.push_back(dict[prevCode][0]);
		}
		else
		{
			break; // 손상된 스트림
		}

		output.insert(output.end(), entry.begin(), entry.end());

		if( prevCode >= 0 && nextCode < 4096 )
		{
			std::vector<uint8_t> newEntry = dict[prevCode];
			newEntry.push_back(entry[0]);
			if( nextCode >= static_cast<int>(dict.size()) ) dict.resize(nextCode + 1);
			dict[nextCode] = std::move(newEntry);
			++nextCode;
			if( nextCode == (1 << codeSize) && codeSize < 12 ) ++codeSize;
		}
		prevCode = code;
	}

	if( output.size() > expectedPixels ) output.resize(expectedPixels);
	return output;
}

//***************************************************************************
// @brief GIF Adam7류 4-패스 인터레이스 순서로 디코드된 행들을 정상 순서로 재배치
// @param indices 인터레이스 순서로 채워진 컬러 인덱스 버퍼(in-place로 수정)
// @param width 이미지 가로 크기(픽셀)
// @param height 이미지 세로 크기(픽셀)
//***************************************************************************
void GifCodec::Deinterlace(std::vector<uint8_t>& indices, int width, int height)
{
	if( width <= 0 || height <= 0 ) return;
	std::vector<uint8_t> out(indices.size());

	// GIF 인터레이스 4-패스: 시작 행과 증분이 고정되어 있다.
	static const int startRow[4] = { 0, 4, 2, 1 };
	static const int step[4] = { 8, 8, 4, 2 };

	int srcRow = 0;
	for( int pass = 0; pass < 4; ++pass )
	{
		for( int y = startRow[pass]; y < height; y += step[pass] )
		{
			if( srcRow >= height ) break;
			std::memcpy(&out[static_cast<size_t>(y) * width], &indices[static_cast<size_t>(srcRow) * width],
				static_cast<size_t>(width));
			++srcRow;
		}
	}
	indices = std::move(out);
}

//***************************************************************************
// @brief 디코드된 컬러 인덱스 프레임을 팔레트/투명색을 적용해 캔버스 위에 합성
// @param canvas 합성 대상 캔버스(전체 화면 크기)
// @param indices 이 프레임의 컬러 인덱스 시퀀스(w*h개)
// @param left 캔버스 내 프레임의 좌상단 가로 좌표
// @param top 캔버스 내 프레임의 좌상단 세로 좌표
// @param w 프레임 가로 크기
// @param h 프레임 세로 크기
// @param palette 사용할 컬러 팔레트(지역 또는 전역 컬러 테이블)
// @param transparentFlag 투명색 사용 여부
// @param transparentIndex 투명 처리할 팔레트 인덱스
//***************************************************************************
void GifCodec::CompositeFrame(ImageBuffer& canvas, const std::vector<uint8_t>& indices,
	int left, int top, int w, int h,
	const std::vector<RgbColor>& palette,
	bool transparentFlag, uint8_t transparentIndex)
{
	if( palette.empty() ) throw ImageException("GIF: no color table available for image");

	for( int y = 0; y < h; ++y )
	{
		int cy = top + y;
		if( cy < 0 || cy >= static_cast<int>(canvas.Height()) ) continue;
		for( int x = 0; x < w; ++x )
		{
			int cx = left + x;
			if( cx < 0 || cx >= static_cast<int>(canvas.Width()) ) continue;

			uint8_t idx = indices[static_cast<size_t>(y) * w + x];
			if( transparentFlag && idx == transparentIndex ) continue; // 투명 - 캔버스 유지

			if( idx >= palette.size() ) idx = 0;
			const RgbColor& c = palette[idx];
			canvas.SetPixel(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy), c.r, c.g, c.b, 255);
		}
	}
}

//***************************************************************************
// @brief 캔버스의 지정 영역을 투명(RGBA 0,0,0,0)으로 초기화
// @param canvas 초기화할 캔버스
// @param left 영역 좌상단 가로 좌표
// @param top 영역 좌상단 세로 좌표
// @param w 영역 가로 크기
// @param h 영역 세로 크기
//***************************************************************************
void GifCodec::ClearRect(ImageBuffer& canvas, int left, int top, int w, int h)
{
	for( int y = 0; y < h; ++y )
	{
		int cy = top + y;
		if( cy < 0 || cy >= static_cast<int>(canvas.Height()) ) continue;
		for( int x = 0; x < w; ++x )
		{
			int cx = left + x;
			if( cx < 0 || cx >= static_cast<int>(canvas.Width()) ) continue;
			canvas.SetPixel(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy), 0, 0, 0, 0);
		}
	}
}

//***************************************************************************
// @brief "restore to previous" 디스포절을 위해 캔버스의 지정 영역을 스냅샷으로 캡처
// @param canvas 캡처할 캔버스
// @param left 영역 좌상단 가로 좌표
// @param top 영역 좌상단 세로 좌표
// @param w 영역 가로 크기
// @param h 영역 세로 크기
// @return 캡처된 영역을 담은 w x h 크기의 ImageBuffer
//***************************************************************************
ImageBuffer GifCodec::CaptureRect(const ImageBuffer& canvas, int left, int top, int w, int h)
{
	ImageBuffer saved(static_cast<uint32_t>(std::max(w, 0)), static_cast<uint32_t>(std::max(h, 0)));
	for( int y = 0; y < h; ++y )
	{
		int cy = top + y;
		if( cy < 0 || cy >= static_cast<int>(canvas.Height()) ) continue;
		for( int x = 0; x < w; ++x )
		{
			int cx = left + x;
			if( cx < 0 || cx >= static_cast<int>(canvas.Width()) ) continue;
			const uint8_t* p = canvas.At(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy));
			saved.SetPixel(static_cast<uint32_t>(x), static_cast<uint32_t>(y), p[0], p[1], p[2], p[3]);
		}
	}
	return saved;
}

//***************************************************************************
// @brief CaptureRect로 캡처해 둔 영역을 캔버스에 되돌림("restore to previous" 디스포절)
// @param canvas 되돌릴 대상 캔버스
// @param left 영역 좌상단 가로 좌표
// @param top 영역 좌상단 세로 좌표
// @param w 영역 가로 크기
// @param h 영역 세로 크기
// @param saved CaptureRect로 캡처해 둔 스냅샷
//***************************************************************************
void GifCodec::RestoreRect(ImageBuffer& canvas, int left, int top, int w, int h, const ImageBuffer& saved)
{
	for( int y = 0; y < h; ++y )
	{
		int cy = top + y;
		if( cy < 0 || cy >= static_cast<int>(canvas.Height()) ) continue;
		for( int x = 0; x < w; ++x )
		{
			int cx = left + x;
			if( cx < 0 || cx >= static_cast<int>(canvas.Width()) ) continue;
			const uint8_t* p = saved.At(static_cast<uint32_t>(x), static_cast<uint32_t>(y));
			canvas.SetPixel(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy), p[0], p[1], p[2], p[3]);
		}
	}
}

//***************************************************************************
// @brief GIF 바이트 시퀀스를 디코드하여 첫 프레임만 ImageBuffer로 반환
// @param data GIF 파일 데이터 시작 주소
// @param size GIF 파일 데이터 길이(바이트)
// @return 디코드된 첫 프레임의 RGBA8 ImageBuffer
//***************************************************************************
ImageBuffer GifCodec::Decode(const uint8_t* data, size_t size) const
{
	std::vector<Frame> frames = DecodeAllFrames(data, size);
	if( frames.empty() ) throw ImageException("GIF: no image found");
	return std::move(frames.front().image);
}

//***************************************************************************
// @brief GIF 인코딩은 지원하지 않으므로 항상 예외를 던짐
// @return (예외를 던지므로 반환하지 않음)
//***************************************************************************
std::vector<uint8_t> GifCodec::Encode(const ImageBuffer&) const
{
	throw ImageException("GIF: encoding not implemented (decode-only). PNG/BMP로 저장하세요.");
}

//***************************************************************************
// @brief GIF의 모든 프레임을 화면 전체 크기로 합성하여 애니메이션 프레임 목록으로 반환
// @param data GIF 파일 데이터 시작 주소
// @param size GIF 파일 데이터 길이(바이트)
// @return 합성된 프레임과 각 프레임의 지연시간 목록(정적 GIF는 1개 원소)
//***************************************************************************
std::vector<GifCodec::Frame> GifCodec::DecodeAllFrames(const uint8_t* data, size_t size) const
{
	if( !CanDecode(data, size) ) throw ImageException("GIF: invalid signature");

	LogicalScreen screen;
	size_t pos = ReadLogicalScreen(data, size, screen);
	if( screen.width <= 0 || screen.height <= 0 ) throw ImageException("GIF: invalid logical screen size");
	// 손상/조작된 헤더로 인한 거대 할당 시도(메모리 고갈)를 방지하기 위한 상한선
	if( static_cast<uint64_t>(screen.width) * static_cast<uint64_t>(screen.height) > 100'000'000ULL )
		throw ImageException("GIF: image dimensions too large");

	ImageBuffer canvas(static_cast<uint32_t>(screen.width), static_cast<uint32_t>(screen.height));
	// ImageBuffer는 0으로 초기화되므로 캔버스는 처음부터 완전 투명 상태이다.

	std::vector<Frame> frames;

	GraphicControl gc; // 다음 이미지에 적용할 제어 정보(기본값: 투명 없음, disposal 0)

	int prevDisposal = 0;
	int prevLeft = 0, prevTop = 0, prevW = 0, prevH = 0;
	ImageBuffer prevSaved; // disposal 3(restore to previous)를 위한 이전 프레임의 사전 스냅샷

	while( pos < size )
	{
		uint8_t marker = data[pos];

		if( marker == 0x3B )
		{
			break; // Trailer
		}
		else if( marker == 0x21 )
		{
			if( pos + 1 >= size ) break;
			uint8_t label = data[pos + 1];
			pos += 2;

			if( label == 0xF9 )
			{
				if( pos >= size ) break;
				uint8_t blockSize = data[pos];
				pos += 1;
				if( pos + blockSize > size ) throw ImageException("GIF: truncated graphic control extension");
				if( blockSize >= 4 )
				{
					uint8_t packed = data[pos];
					uint16_t delay = static_cast<uint16_t>(data[pos + 1] | (data[pos + 2] << 8));
					uint8_t transIndex = data[pos + 3];
					gc.disposalMethod = (packed >> 2) & 0x07;
					gc.transparentFlag = (packed & 0x01) != 0;
					gc.transparentIndex = transIndex;
					gc.delayCs = delay;
				}
				pos += blockSize;
				if( pos < size && data[pos] == 0 ) pos += 1; // 블록 종결자
			}
			else if( label == 0x01 )
			{
				if( pos + 13 > size ) throw ImageException("GIF: truncated plain text extension");
				pos += 13;
				SkipSubBlocks(data, size, pos);
			}
			else if( label == 0xFF )
			{
				if( pos >= size ) break;
				uint8_t blockSize = data[pos];
				pos += 1;
				if( pos + blockSize > size ) throw ImageException("GIF: truncated application extension");
				pos += blockSize;
				SkipSubBlocks(data, size, pos);
			}
			else
			{ // 0xFE(Comment) 등 그 외 확장
				SkipSubBlocks(data, size, pos);
			}
		}
		else if( marker == 0x2C )
		{
			pos += 1;
			if( pos + 9 > size ) throw ImageException("GIF: truncated image descriptor");

			int left = data[pos] | (data[pos + 1] << 8);
			int top = data[pos + 2] | (data[pos + 3] << 8);
			int w = data[pos + 4] | (data[pos + 5] << 8);
			int h = data[pos + 6] | (data[pos + 7] << 8);
			uint8_t packed = data[pos + 8];
			pos += 9;

			if( static_cast<uint64_t>(w) * static_cast<uint64_t>(h) > 100'000'000ULL )
				throw ImageException("GIF: image dimensions too large");

			bool localTableFlag = (packed & 0x80) != 0;
			bool interlaceFlag = (packed & 0x40) != 0;
			int localTableSize = packed & 0x07;

			std::vector<RgbColor> localTable;
			if( localTableFlag )
			{
				localTable = ReadColorTable(data, size, pos, localTableSize);
			}
			const std::vector<RgbColor>& palette = localTableFlag ? localTable : screen.globalTable;

			if( pos >= size ) throw ImageException("GIF: missing LZW minimum code size");
			uint8_t minCodeSize = data[pos];
			pos += 1;

			std::vector<uint8_t> packedData = CollectSubBlocks(data, size, pos);
			size_t expectedPixels = static_cast<size_t>(w) * static_cast<size_t>(h);
			std::vector<uint8_t> indices = LzwDecode(packedData, minCodeSize, expectedPixels);
			if( indices.size() < expectedPixels ) indices.resize(expectedPixels, 0);
			if( interlaceFlag ) Deinterlace(indices, w, h);

			// 이전 프레임의 디스포절을 이번 프레임을 그리기 직전에 적용한다.
			if( !frames.empty() )
			{
				if( prevDisposal == 2 )
				{
					ClearRect(canvas, prevLeft, prevTop, prevW, prevH);
				}
				else if( prevDisposal == 3 )
				{
					RestoreRect(canvas, prevLeft, prevTop, prevW, prevH, prevSaved);
				}
			}

			// 이번 프레임이 restore-to-previous라면, 그리기 전 상태를 스냅샷해 둔다.
			if( gc.disposalMethod == 3 )
			{
				prevSaved = CaptureRect(canvas, left, top, w, h);
			}

			CompositeFrame(canvas, indices, left, top, w, h, palette, gc.transparentFlag, gc.transparentIndex);

			Frame frame;
			frame.image = canvas.Clone();
			frame.delayCs = gc.delayCs;
			frames.push_back(std::move(frame));

			prevDisposal = gc.disposalMethod;
			prevLeft = left; prevTop = top; prevW = w; prevH = h;

			gc = GraphicControl(); // GCE는 다음 이미지 하나에만 적용되므로 소비 후 초기화
		}
		else
		{
			break; // 인식할 수 없는 블록 - 더 이상 안전하게 파싱할 수 없음
		}
	}

	if( frames.empty() ) throw ImageException("GIF: no image found");
	return frames;
}
