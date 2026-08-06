
//***************************************************************************
// BufferReader.h : interface for the CBufferReader class.
//
//***************************************************************************

#ifndef __BUFFER_READER_H__
#define __BUFFER_READER_H__

//***************************************************************************
// @class BufferReader
// @brief 메모리 버퍼로부터 데이터를 순차적으로 읽어들이는(역직렬화) 클래스입니다.
//
// @details
// 수신받은 패킷 바이트 배열에서 읽기 커서(_pos)를 이동시키며
// 원하는 데이터 타입 및 크기만큼 안전하게 추출할 수 있도록 지원합니다.
//
// 주요 처리 및 특징:
//  - 커서 이동 없이 데이터를 미리 살펴볼 수 있는 Peek 기능 제공
//  - 템플릿과 연산자 오버로딩(operator>>)을 통한 직관적인 데이터 읽기 지원
//  - 남은 유효 데이터 크기(FreeSize) 검증을 통한 오버플로우 방지
//***************************************************************************
class CBufferReader
{
public:
	CBufferReader();
	CBufferReader(BYTE* buffer, uint32 size, uint32 pos = 0);
	~CBufferReader();

	BYTE*			Buffer() { return _buffer; }
	uint32			Size() { return _size; }
	uint32			ReadSize() { return _pos; }
	uint32			FreeSize() { return _size - _pos; }

	template<typename T>
	bool			Peek(T* dest) { return Peek(dest, sizeof(T)); }
	bool			Peek(void* dest, uint32 len);

	template<typename T>
	bool			Read(T* dest) { return Read(dest, sizeof(T)); }
	bool			Read(void* dest, uint32 len);

	template<typename T>
	CBufferReader&	operator>>(OUT T& dest);

private:
	BYTE*			_buffer = nullptr;	// 읽어올 대상 메모리 버퍼 포인터
	uint32			_size = 0;			// 전체 버퍼 용량
	uint32			_pos = 0;			// 현재 읽기 커서 위치 (읽어들인 바이트 수)
};

//************************************************---------------------------
// @brief 연산자 오버로딩을 통해 버퍼로부터 데이터를 연속적으로 추출합니다.
// @param dest 데이터를 담을 변수 (출력 매개변수)
// @return BufferReader& (연속 체이닝 가능)
//************************************************---------------------------
template<typename T>
inline CBufferReader& CBufferReader::operator>>(OUT T& dest)
{
	dest = *reinterpret_cast<T*>(&_buffer[_pos]);
	_pos += sizeof(T);
	return *this;
}

#endif // __BUFFER_READER_H__