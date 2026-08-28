
//***************************************************************************
// BufferReader.h : interface for the CBufferReader class.
//
//***************************************************************************

#ifndef UC_BUFFERREADER_H
#define UC_BUFFERREADER_H

//***************************************************************************
// @class CBufferReader
// @brief 메모리 버퍼로부터 데이터를 순차적으로 읽어들이는(역직렬화) 클래스입니다.
//
// @details
// 수신받은 패킷 바이트 배열에서 읽기 커서(_pos)를 이동시키며
// 원하는 데이터 타입 및 크기만큼 안전하게 추출할 수 있도록 지원합니다.
//
// 주요 처리 및 특징:
//  - 커서 이동 없이 데이터를 미리 살펴볼 수 있는 Peek 기능 제공
//  - 템플릿과 연산자 오버로딩(operator>>)을 통한 직관적인 데이터 읽기 지원
//  - 남은 유효 데이터 크기(FreeSize) 검증을 통한 오버플로우 및 Crash 방지
//  - 명시적 타입별 Read 메서드 및 버퍼 재설정(Reset) 기능 지원
//***************************************************************************
class CBufferReader
{
public:
	CBufferReader();
	CBufferReader(BYTE* buffer, uint32 size, uint32 pos = 0);
	~CBufferReader();

	BYTE* Buffer() { return _buffer; }
	uint32			Size() { return _size; }
	uint32			ReadSize() { return _pos; }
	uint32			FreeSize() { return _size - _pos; }

	//***************************************************************************
	// @brief 버퍼 대상 메모리와 크기, 커서 위치를 재설정하여 객체를 재사용합니다.
	// @param buffer 새롭게 지정할 메모리 버퍼 포인터
	// @param size 버퍼의 전체 크기
	// @param pos 초기 읽기 커서 위치 (기본값: 0)
	//***************************************************************************
	void			Reset(BYTE* buffer, uint32 size, uint32 pos = 0);

	//***************************************************************************
	// @brief 현재 읽기 커서 위치(읽은 바이트 수)만 0으로 초기화합니다.
	//***************************************************************************
	void			Clear() { _pos = 0; }

	template<typename T>
	bool			Peek(T* dest) { return Peek(dest, sizeof(T)); }
	bool			Peek(void* dest, uint32 len);

	template<typename T>
	bool			Read(T* dest) { return Read(dest, sizeof(T)); }
	bool			Read(void* dest, uint32 len);

	//***************************************************************************
	// @brief 버퍼로부터 8비트 정수(BYTE/uint8) 데이터를 읽어옵니다.
	// @param dest 읽어온 데이터를 저장할 uint8 포인터
	// @return 성공 여부 (남은 데이터 부족 시 false)
	//***************************************************************************
	bool			ReadByte(uint8* dest) { return Read(dest); }

	//***************************************************************************
	// @brief 버퍼로부터 16비트 정수(int16) 데이터를 읽어옵니다.
	// @param dest 읽어온 데이터를 저장할 int16 포인터
	// @return 성공 여부 (남은 데이터 부족 시 false)
	//***************************************************************************
	bool			ReadInt16(int16* dest) { return Read(dest); }

	//***************************************************************************
	// @brief 버퍼로부터 32비트 정수(int32) 데이터를 읽어옵니다.
	// @param dest 읽어온 데이터를 저장할 int32 포인터
	// @return 성공 여부 (남은 데이터 부족 시 false)
	//***************************************************************************
	bool			ReadInt32(int32* dest) { return Read(dest); }

	//***************************************************************************
	// @brief 버퍼로부터 64비트 정수(int64) 데이터를 읽어옵니다.
	// @param dest 읽어온 데이터를 저장할 int64 포인터
	// @return 성공 여부 (남은 데이터 부족 시 false)
	//***************************************************************************
	bool			ReadInt64(int64* dest) { return Read(dest); }

	//***************************************************************************
	// @brief 버퍼로부터 단정밀도 부동소수점(float) 데이터를 읽어옵니다.
	// @param dest 읽어온 데이터를 저장할 float 포인터
	// @return 성공 여부 (남은 데이터 부족 시 false)
	//***************************************************************************
	bool			ReadFloat(float* dest) { return Read(dest); }

	//***************************************************************************
	// @brief 버퍼로부터 논리형(bool) 데이터를 읽어옵니다.
	// @param dest 읽어온 데이터를 저장할 bool 포인터
	// @return 성공 여부 (남은 데이터 부족 시 false)
	//***************************************************************************
	bool			ReadBool(bool* dest) { return Read(dest); }

	template<typename T>
	CBufferReader& operator>>(OUT T& dest);

private:
	BYTE* _buffer = nullptr;		// 읽어올 대상 메모리 버퍼 포인터
	uint32			_size = 0;		// 전체 버퍼 용량
	uint32			_pos = 0;		// 현재 읽기 커서 위치(읽어들인 바이트 수)
};

//***************************************************************************
// @brief 연산자 오버로딩을 통해 버퍼로부터 데이터를 연속적으로 추출합니다.
// @param dest 데이터를 담을 변수 (출력 매개변수)
// @return CBufferReader& (연속 체이닝 가능)
// @note 남은 유효 데이터 크기를 검증하여 메모리 오버런(Crash)을 방지합니다.
//***************************************************************************
template<typename T>
inline CBufferReader& CBufferReader::operator>>(OUT T& dest)
{
	if( Read(&dest, sizeof(T)) == false )
	{
		// 유효 범위 초과 시 안전하게 0으로 초기화
		::memset(&dest, 0, sizeof(T));
	}
	return *this;
}

#endif // ndef UC_BUFFERREADER_H