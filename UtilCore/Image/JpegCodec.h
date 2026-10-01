
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
// @brief JPEG(JFIF) baseline 이미지를 디코드/인코드하는 코덱
// @details 디코드는 마커/청크 파싱, 허프만 복호, IDCT, 색공간 변환, 크로마
//          업샘플링을 전부 직접 구현한다(프로그레시브 JPEG는 미지원).
//          인코드는 4:4:4(서브샘플링 없음)로만 동작하며, 표준(Annex K)
//          양자화/허프만 테이블을 품질값으로 스케일링해 사용한다.
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

	std::vector<uint8_t> EncodeQuality(const ImageBuffer& image, int quality) const;

private:
	// JPEG 지그재그 스캔 순서 <-> 8x8 블록 자연(raster) 순서 변환 테이블.
	// Decoder/Encoder가 공통으로 사용하므로 바깥 클래스(JpegCodec) 스코프에 둔다.
	static constexpr int kZigZag[64] = {
		0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
		35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
	};

	//***************************************************************************
	// @brief 정규 허프만 코드 테이블(DC 또는 AC) 하나를 표현하는 복호용 테이블
	//***************************************************************************
	struct HuffTable
	{
		// JPEG 표준(ITU T.81 Annex F, Figure F.15/F.16)이 그대로 설명하는 정규
		// 허프만 증분 복호 방식: 코드 길이별 min/max 코드값과 심볼 배열 내
		// 시작 오프셋만으로 해시 없이 O(1) 범위 비교로 심볼을 복원한다.
		// (이전 unordered_map 기반 구현보다 비트당 비용이 훨씬 낮다.)
		int minCode[17] = {};      // 길이 len(1~16)의 최소 코드값(인덱스 0 미사용)
		int maxCode[17] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }; // 길이 len의 최대 코드값, 코드가 없으면 -1 (Build 전 기본값도 안전하게 -1)
		int valPtr[17] = {};       // 길이 len의 코드들이 values_ 배열에서 시작하는 위치
		std::vector<uint8_t> values_; // 코드 길이 순으로 정렬된 심볼 목록

		void Build(const uint8_t counts[16], const std::vector<uint8_t>& symbols);
	};

	//***************************************************************************
	// @brief 심볼별 정규 허프만 코드/길이를 담는 인코드용 테이블
	// @details 디코드용 HuffTable과 반대 방향(심볼 -> 코드) 매핑이다.
	//***************************************************************************
	struct EncHuffTable
	{
		uint16_t code[256] = {};   // 심볼별 허프만 코드(우측 정렬)
		uint8_t length[256] = {};  // 심볼별 코드 길이(비트, 0이면 미사용 심볼)

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
	//        단위로 읽는 스트림(디코드용, LSB가 아닌 MSB-first)
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
		bool hasScanData_ = false;                        // SOS/엔트로피 스캔이 한 번이라도 처리되었는지 여부

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

	//***************************************************************************
	// @brief 표준(Annex K) 양자화/허프만 테이블 기반의 baseline(4:4:4) JPEG 인코더
	// @details 크로마 서브샘플링 없이 컴포넌트마다 8x8 블록 1개씩(4:4:4)만
	//          다루므로 MCU 구성이 단순하며, 별도의 최적화된(2-pass) 허프만
	//          테이블 생성 없이 JPEG 표준이 권장하는 고정 테이블을 그대로 쓴다.
	//***************************************************************************
	class Encoder
	{
	public:
		std::vector<uint8_t> Run(const ImageBuffer& image, int quality) const;

	private:
		//***************************************************************************
		// @brief JPEG 엔트로피 코딩용 MSB-first 비트 라이터(0xFF 바이트 스터핑 포함)
		//***************************************************************************
		class BitWriter
		{
		public:
			void PutBits(uint32_t value, int length);
			void Flush();
			std::vector<uint8_t> TakeBuffer();

		private:
			std::vector<uint8_t> out_; // 완성된 바이트를 누적하는 출력 버퍼
			uint64_t acc_ = 0;         // 아직 출력되지 않은 비트를 담는 누적기
			int nbits_ = 0;            // acc_에 쌓여 있는 유효 비트 수
		};

		static void BuildQuantTable(const uint8_t base[64], int quality, uint16_t out[64]);
		static void WriteSegment(std::vector<uint8_t>& out, uint16_t marker, const uint8_t* data, size_t len);
		static void FDCT8x8(const float in[64], float out[64]);
		static int CalcCategory(int value);
		static void EmitHuffman(BitWriter& bw, const EncHuffTable& table, int symbol);
		static void EmitCategoryBits(BitWriter& bw, int value, int category);
		static void EncodeBlock(BitWriter& bw, const float block[64], const uint16_t quantTable[64],
			const EncHuffTable& dcTable, const EncHuffTable& acTable, int& dcPred);
	};
};

#endif // ndef UC_JPEGCODEC_H
