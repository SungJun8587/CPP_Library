
#ifndef UC_JPEGCODEC_H
#define UC_JPEGCODEC_H

#include "ICodec.h"
#include <array>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>

//***************************************************************************
// @brief JPEG(JFIF) baseline 이미지를 디코드하는 코덱(인코드는 미구현)
// @details 마커/청크 파싱, 허프만 복호, IDCT, 색공간 변환, 크로마 업샘플링을
//          전부 직접 구현한다. 프로그레시브 JPEG는 지원하지 않는다.
//***************************************************************************
class JpegCodec : public ICodec
{
public:
	//***************************************************************************
	// @brief 이 코덱이 처리하는 이미지 포맷을 반환
	// @return ImageFormat::JPEG
	//***************************************************************************
	ImageFormat Format() const override { return ImageFormat::JPEG; }

	bool CanDecode(const uint8_t* data, size_t size) const override;
	ImageBuffer Decode(const uint8_t* data, size_t size) const override;
	std::vector<uint8_t> Encode(const ImageBuffer& image) const override;

private:
	//***************************************************************************
	// @brief 정규 허프만 코드 테이블(DC 또는 AC) 하나를 표현하는 룩업 테이블
	// @details key = (코드 길이 << 16) | 코드 값, value = 심볼(바이트).
	//***************************************************************************
	struct HuffTable
	{
		std::unordered_map<uint32_t, uint8_t> lut; // (길이,코드) -> 심볼 룩업 테이블

		void Build(const uint8_t counts[16], const std::vector<uint8_t>& symbols);
	};

	//***************************************************************************
	// @brief SOF/SOS에서 파싱한 컴포넌트(Y/Cb/Cr 등)별 상태와 디코드된 평면
	//***************************************************************************
	struct Component
	{
		int id = 0;             // 컴포넌트 식별자(SOF에서 지정)
		int hSamp = 1;          // 수평 샘플링 비율
		int vSamp = 1;          // 수직 샘플링 비율
		int quantTableId = 0;   // 사용할 양자화 테이블 인덱스
		int dcTableId = 0;      // 사용할 DC 허프만 테이블 인덱스
		int acTableId = 0;      // 사용할 AC 허프만 테이블 인덱스
		int dcPred = 0;         // DC 계수 예측값(스캔 내 누적)
		int width = 0;          // 컴포넌트 평면의 가로 크기(서브샘플됨)
		int height = 0;         // 컴포넌트 평면의 세로 크기(서브샘플됨)
		std::vector<uint8_t> plane; // 디코드된 컴포넌트 평면(레벨시프트 완료, 0..255)
	};

	//***************************************************************************
	// @brief 0xFF 0x00 바이트 스터핑을 제거하며 엔트로피 코딩 구간을 비트
	//        단위로 읽는 스트림
	//***************************************************************************
	class BitStream
	{
	public:
		//***************************************************************************
		// @brief 엔트로피 코딩 데이터 범위로 스트림을 초기화
		// @param data 엔트로피 코딩 데이터 시작 주소
		// @param size 엔트로피 코딩 데이터 길이(바이트)
		//***************************************************************************
		BitStream(const uint8_t* data, size_t size) : data_(data), size_(size) {}

		int ReadBit();
		int ReadBits(int n);

		//***************************************************************************
		// @brief RST 마커 뒤에서 재동기화하기 위해 비트 버퍼를 비움
		//***************************************************************************
		void ResetToByteBoundary() { bitsLeft_ = 0; }

		//***************************************************************************
		// @brief 현재까지 소비한 바이트 오프셋을 반환
		// @return 다음에 읽을 바이트의 인덱스
		//***************************************************************************
		size_t Pos() const { return pos_; }

		//***************************************************************************
		// @brief 스트림 읽기 위치를 지정한 오프셋으로 이동하고 비트 버퍼를 비움
		// @param p 이동할 바이트 오프셋
		//***************************************************************************
		void SetPos(size_t p) { pos_ = p; bitsLeft_ = 0; }

	private:
		const uint8_t* data_;    // 엔트로피 코딩 데이터 시작 주소
		size_t size_;            // 엔트로피 코딩 데이터 전체 길이
		size_t pos_ = 0;         // 다음에 읽을 바이트 인덱스
		uint8_t cur_ = 0;        // 현재 처리 중인 바이트
		int bitsLeft_ = 0;       // cur_ 내에서 남은 비트 수
	};

	//***************************************************************************
	// @brief SOF/DQT/DHT/SOS 파싱 결과와 엔트로피 디코딩 상태를 담는 내부 디코더
	//***************************************************************************
	class Decoder
	{
	public:
		ImageBuffer Run(const uint8_t* data, size_t size);

	private:
		int width_ = 0;    // 이미지 가로 크기(픽셀)
		int height_ = 0;   // 이미지 세로 크기(픽셀)
		int maxH_ = 1;     // 모든 컴포넌트 중 최대 수평 샘플링 비율
		int maxV_ = 1;     // 모든 컴포넌트 중 최대 수직 샘플링 비율
		std::vector<Component> comps_;                   // SOF에서 파싱한 컴포넌트 목록
		std::array<std::array<int, 64>, 4> quantTables_{}; // 양자화 테이블(DQT, 0~3번 슬롯)
		std::array<bool, 4> quantSet_{};                  // 양자화 테이블 슬롯별 설정 여부
		std::array<HuffTable, 4> dcTables_;               // DC 허프만 테이블(0~3번 슬롯)
		std::array<HuffTable, 4> acTables_;               // AC 허프만 테이블(0~3번 슬롯)
		int restartInterval_ = 0;                         // DRI로 지정된 재시작 간격(MCU 단위)
		std::vector<int> scanCompOrder_;                  // SOS에 나열된 컴포넌트 인덱스 순서

		// JPEG 지그재그 스캔 순서 <-> 8x8 블록 자연 순서 변환 테이블
		static constexpr int kZigZag[64] = {
			0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
			35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
		};

		static uint16_t ReadU16(const uint8_t* p);
		static size_t FindNextMarker(const uint8_t* data, size_t size, size_t from);
		void ParseDQT(const uint8_t* seg, size_t len);
		void ParseSOF0(const uint8_t* seg, size_t len);
		void ParseDHT(const uint8_t* seg, size_t len);
		void ParseSOS(const uint8_t* seg, size_t len);
		static int Extend(int value, int numBits);
		int DecodeHuffSymbol(BitStream& bs, const HuffTable& table);
		void DecodeBlock(BitStream& bs, Component& comp, int block[64]);
		static void IDCT8x8(const float in[64], uint8_t out[64]);
		void DecodeScan(const uint8_t* entropyData, size_t entropyLen);
		ImageBuffer ComposeImage();
		uint8_t SamplePlane(const Component& c, int x, int y) const;
		static uint8_t Clamp(int v);
	};
};

#endif // ndef UC_JPEGCODEC_H
