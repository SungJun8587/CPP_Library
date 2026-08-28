
//***************************************************************************
// BufferWriter.h : interface for the CBufferWriter class.
//
//***************************************************************************

#ifndef UC_BUFFERWRITER_H
#define UC_BUFFERWRITER_H

//***************************************************************************
// @class CBufferWriter
// @brief 메모리 버퍼에 데이터를 순차적으로 기록(직렬화)하는 클래스입니다.
//
// @details
// 소켓이나 파일로 전송하기 전, 다양한 데이터 타입(기본 자료형, 구조체 등)을
// 메모리 버퍼에 안전하고 편리하게 쓸 수 있도록 쓰기 커서(_pos)를 관리합니다.
//
// 주요 처리 및 특징:
//  - 남은 공간(FreeSize) 검증을 통한 오버플로우 방지
//  - 템플릿과 연산자 오버로딩(operator<<)을 통한 직관적인 데이터 쓰기 지원
//  - 특정 패킷 구조를 위해 미리 공간을 확보하는 Reserve 기능 제공
//  - 명시적 타입별 Write 메서드 및 버퍼 재설정(Reset) 기능 지원
//***************************************************************************
class CBufferWriter
{
public:
	CBufferWriter();
	CBufferWriter(BYTE* buffer, uint32 size, uint32 pos = 0);
	~CBufferWriter();

	BYTE* Buffer() { return _buffer; }
	uint32			Size() { return _size; }
	uint32			WriteSize() { return _pos; }
	uint32			FreeSize() { return _size - _pos; }

	//***************************************************************************
	// @brief 버퍼 대상 메모리와 크기, 커서 위치를 재설정하여 객체를 재사용합니다.
	// @param buffer 새롭게 지정할 메모리 버퍼 포인터
	// @param size 버퍼의 전체 크기
	// @param pos 초기 쓰기 커서 위치 (기본값: 0)
	//***************************************************************************
	void			Reset(BYTE* buffer, uint32 size, uint32 pos = 0);

	//***************************************************************************
	// @brief 현재 쓰기 커서 위치(작성된 바이트 수)만 0으로 초기화합니다.
	//***************************************************************************
	void			Clear() { _pos = 0; }

	template<typename T>
	bool			Write(T* src) { return Write(src, sizeof(T)); }
	bool			Write(const void* src, uint32 len);

	//***************************************************************************
	// @brief 전달받은 데이터를 복사하여 버퍼에 기록합니다.
	// @param src 버퍼에 기록할 데이터
	// @return 성공 여부 (여유 공간 부족 시 false)
	//***************************************************************************
	template<typename T>
	bool			WriteValue(const T& src) { return Write(&src, sizeof(T)); }

	//***************************************************************************
	// @brief 8비트 정수(BYTE/uint8) 데이터를 버퍼에 기록합니다.
	// @param val 기록할 uint8 값
	// @return 성공 여부 (여유 공간 부족 시 false)
	//***************************************************************************
	bool			WriteByte(uint8 val) { return WriteValue(val); }

	//***************************************************************************
	// @brief 16비트 정수(int16) 데이터를 버퍼에 기록합니다.
	// @param val 기록할 int16 값
	// @return 성공 여부 (여유 공간 부족 시 false)
	//***************************************************************************
	bool			WriteInt16(int16 val) { return WriteValue(val); }

	//***************************************************************************
	// @brief 32비트 정수(int32) 데이터를 버퍼에 기록합니다.
	// @param val 기록할 int32 값
	// @return 성공 여부 (여유 공간 부족 시 false)
	//***************************************************************************
	bool			WriteInt32(int32 val) { return WriteValue(val); }

	//***************************************************************************
	// @brief 64비트 정수(int64) 데이터를 버퍼에 기록합니다.
	// @param val 기록할 int64 값
	// @return 성공 여부 (여유 공간 부족 시 false)
	//***************************************************************************
	bool			WriteInt64(int64 val) { return WriteValue(val); }

	//***************************************************************************
	// @brief 단정밀도 부동소수점(float) 데이터를 버퍼에 기록합니다.
	// @param val 기록할 float 값
	// @return 성공 여부 (여유 공간 부족 시 false)
	//***************************************************************************
	bool			WriteFloat(float val) { return WriteValue(val); }

	//***************************************************************************
	// @brief 논리형(bool) 데이터를 버퍼에 기록합니다.
	// @param val 기록할 bool 값
	// @return 성공 여부 (여유 공간 부족 시 false)
	//***************************************************************************
	bool			WriteBool(bool val) { return WriteValue(val); }

	template<typename T>
	T* Reserve(uint16 count = 1);

	template<typename T>
	CBufferWriter& operator<<(T&& src);

private:
	BYTE*			_buffer = nullptr;	// 기록 대상 메모리 버퍼 포인터
	uint32			_size = 0;			// 전체 버퍼 용량
	uint32			_pos = 0;			// 현재 쓰기 커서 위치 (기록된 바이트 수)
};

//***************************************************************************
// @brief 버퍼에 특정 타입의 공간을 미리 예약하고 해당 포인터를 반환합니다.
// @param count 예약할 데이터 개수 (기본값: 1)
// @return 예약된 메모리 시작 주소 (여유 공간 부족 시 nullptr 반환)
//***************************************************************************
template<typename T>
T* CBufferWriter::Reserve(uint16 count)
{
	if( FreeSize() < (sizeof(T) * count) )
		return nullptr;

	T* ret = reinterpret_cast<T*>(&_buffer[_pos]);
	_pos += (sizeof(T) * count);
	return ret;
}

//***************************************************************************
// @brief 연산자 오버로딩을 통해 데이터를 버퍼에 연속적으로 기록합니다.
// @param src 기록할 데이터 (Rvalue/Lvalue 모두 지원)
// @return CBufferWriter& (연속 체이닝 가능)
// @note 메모리 경계 오버런을 방지합니다.
//***************************************************************************
template<typename T>
CBufferWriter& CBufferWriter::operator<<(T&& src)
{
	using DataType = std::remove_cvref_t<T>;
	DataType temp = std::forward<T>(src);
	Write(&temp, sizeof(DataType));
	return *this;
}

#endif // ndef UC_BUFFERWRITER_H