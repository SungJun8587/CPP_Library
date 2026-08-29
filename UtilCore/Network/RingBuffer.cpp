
//***************************************************************************
// RingBuffer.cpp: implementation of the CRingBuffer class.
//
//***************************************************************************

#include "pch.h"
#include "RingBuffer.h"

//***************************************************************************
// @brief 기본 크기(BUFFER_SIZE_DEFAULT)로 링버퍼를 초기화합니다.
//***************************************************************************
CRingBuffer::CRingBuffer() : CRingBuffer(BUFFER_SIZE_DEFAULT)
{
}

//***************************************************************************
// @brief 지정한 크기(bufferSize)로 링버퍼를 초기화합니다.
// @param bufferSize 할당할 버퍼 크기 (바이트, 2 이상 ~ ULONG_MAX 이하)
//***************************************************************************
CRingBuffer::CRingBuffer(int64 bufferSize)
	: _begin(nullptr)
	, _end(nullptr)
	, _read(nullptr)
	, _write(nullptr)
{
	constexpr int64 maxAllowedSize = static_cast<int64>((std::numeric_limits<ULONG>::max)());
	if( bufferSize <= 1 || bufferSize > maxAllowedSize )
	{
		throw std::invalid_argument(
			"CRingBuffer: bufferSize는 2 이상, ULONG_MAX 이하이어야 합니다.");
	}

	_begin = static_cast<char*>(RawAllocator::Alloc(static_cast<size_t>(bufferSize)));
	if( _begin == nullptr )
	{
		throw std::bad_alloc();
	}

	_end = _begin + bufferSize;
	_read = _write = _begin;
}

//***************************************************************************
// @brief 이동 생성자 구현 (BaseAllocator의 이동 생성자 명시적 호출)
// @param other 이동할 대상 CRingBuffer 객체
//***************************************************************************
CRingBuffer::CRingBuffer(CRingBuffer&& other) noexcept
	: BaseAllocator(std::move(other))
	, _begin(other._begin)
	, _end(other._end)
	, _read(other._read)
	, _write(other._write)
{
	other._begin = other._end = other._read = other._write = nullptr;
}

//***************************************************************************
// @brief 이동 대입 연산자 구현 (BaseAllocator의 이동 대입 연산자 명시적 호출)
// @param other 이동할 대상 CRingBuffer 객체
// @return 자기 자신에 대한 참조 (CRingBuffer&)
//***************************************************************************
CRingBuffer& CRingBuffer::operator=(CRingBuffer&& other) noexcept
{
	if( this != &other )
	{
		if( _begin != nullptr )
		{
			RawAllocator::Free(_begin);
		}

		_begin = other._begin;
		_end = other._end;
		_read = other._read;
		_write = other._write;

		other._begin = nullptr;
		other._end = nullptr;
		other._read = nullptr;
		other._write = nullptr;
	}
	return *this;
}

//***************************************************************************
// @brief 소멸자: 할당된 메모리를 해제합니다.
//***************************************************************************
CRingBuffer::~CRingBuffer()
{
	if( _begin != nullptr )
	{
		RawAllocator::Free(_begin);
	}
}

//***************************************************************************
// @brief 링버퍼에 데이터를 씁니다.
// @param data 쓸 데이터가 위치한 버퍼 포인터 (RingBuffer 내부 메모리와 Overlap 금지)
// @param requestSize 쓰기 요청 크기 (바이트)
// @param outEnqueueSize [out] 실제로 쓰여진 바이트 수가 저장될 변수 포인터
// @param isPartialEnqueueAvailable 부분 쓰기 허용 여부 (true인 경우 여유 공간만큼만 쓰고 성공 처리)
// @return true: 쓰기 성공, false: 요청 무효, 공간 부족 또는 실패
//***************************************************************************
bool CRingBuffer::Enqueue(const char* data, int64 requestSize, int64* outEnqueueSize, bool isPartialEnqueueAvailable)
{
	if( outEnqueueSize != nullptr ) *outEnqueueSize = 0;

	// 1. 음수 크기 차단
	if( requestSize < 0 )
	{
		return false;
	}

	// 2. requestSize == 0 인 경우 성공 처리
	if( requestSize == 0 )
	{
		return true;
	}

	// 3. requestSize > 0 인데 data가 nullptr 인 경우 실패 처리
	if( data == nullptr )
	{
		return false;
	}

	int64 freeSize = GetSizeFree();

	if( freeSize < requestSize )
	{
		if( isPartialEnqueueAvailable )
		{
			requestSize = freeSize;
		}
		else
		{
			return false;
		}
	}

	if( requestSize <= 0 )
	{
		return true;
	}

	int64 firstChunkSize = GetSizeDirectEnqueueAble();
	if( firstChunkSize > requestSize ) firstChunkSize = requestSize;
	memcpy(_write, data, firstChunkSize);

	int64 secondChunkSize = requestSize - firstChunkSize;
	if( secondChunkSize > 0 )
	{
		memcpy(_begin, data + firstChunkSize, secondChunkSize);
	}

	MoveWriteBuffer(requestSize);

	if( outEnqueueSize != nullptr ) *outEnqueueSize = requestSize;
	return true;
}

//***************************************************************************
// @brief 링버퍼에서 데이터를 읽고 커서를 이동합니다.
// @param outData 데이터를 복사받을 버퍼 포인터 (RingBuffer 내부 메모리와 Overlap 금지)
// @param requestSize 읽기 요청 크기 (바이트)
// @param outDequeueSize [out] 실제로 읽혀진 바이트 수가 저장될 변수 포인터
// @param isPartialDequeueAvailable 부분 읽기 허용 여부
// @param isPeekMode 읽기 전용 모드 여부 (true인 경우 읽은 후 읽기 커서를 이동하지 않음)
// @return true: 읽기 성공, false: 요청 무효, 데이터 부족 또는 실패
//***************************************************************************
bool CRingBuffer::Dequeue(char* outData, int64 requestSize, int64* outDequeueSize, bool isPartialDequeueAvailable, bool isPeekMode)
{
	if( outDequeueSize != nullptr ) *outDequeueSize = 0;

	// 1. 음수 크기 차단
	if( requestSize < 0 )
	{
		return false;
	}

	// 2. requestSize == 0 인 경우 성공 처리
	if( requestSize == 0 )
	{
		return true;
	}

	// 3. requestSize > 0 인데 outData가 nullptr 인 경우 실패 처리
	if( outData == nullptr )
	{
		return false;
	}

	int64 usedSize = GetSizeUsed();

	if( usedSize < requestSize )
	{
		if( isPartialDequeueAvailable )
		{
			requestSize = usedSize;
		}
		else
		{
			return false;
		}
	}

	if( requestSize <= 0 )
	{
		return true;
	}

	int64 firstChunkSize = GetSizeDirectDequeueAble();
	if( firstChunkSize > requestSize ) firstChunkSize = requestSize;
	memcpy(outData, _read, firstChunkSize);

	int64 secondChunkSize = requestSize - firstChunkSize;
	if( secondChunkSize > 0 )
	{
		memcpy(outData + firstChunkSize, _begin, secondChunkSize);
	}

	if( !isPeekMode ) MoveReadBuffer(requestSize);

	if( outDequeueSize != nullptr ) *outDequeueSize = requestSize;
	return true;
}

//***************************************************************************
// @brief 링버퍼에서 데이터를 읽기 커서 이동 없이 복사합니다 (Peek).
// @param outData 데이터를 복사받을 버퍼 포인터 (RingBuffer 내부 메모리와 Overlap 금지)
// @param requestSize 확인 요청 크기 (바이트)
// @param outPeekSize [out] 실제로 복사된 바이트 수가 저장될 변수 포인터
// @param isPartialPeekAvailable 부분 확인 허용 여부
// @return true: 성공, false: 실패
//***************************************************************************
bool CRingBuffer::Peek(char* outData, int64 requestSize, int64* outPeekSize, bool isPartialPeekAvailable)
{
	return Dequeue(outData, requestSize, outPeekSize, isPartialPeekAvailable, true);
}

//***************************************************************************
// @brief IOCP WSARecv를 위한 쓰기 가능 영역 WSABUF 배열을 생성합니다 (최대 2개 청크).
// @param outBuffers [out] 크기 2인 WSABUF 배열 (참조로 받아 컴파일 타임 크기 강제)
// @return 생성된 청크 개수 (0 ~ 2)
//***************************************************************************
int CRingBuffer::GetWSARecvBuffers(WSABUF(&outBuffers)[2]) const
{
	int count = 0;
	int64 freeSize = GetSizeFree();
	if( freeSize <= 0 ) return 0;

	if( _write >= _read )
	{
		// 1. _write부터 _end 직전까지 (단, _read == _begin인 경우 Full 상태와의 구분을 위해 N-1 규칙 적용하여 _end - 1 까지만 사용)
		char* chunk1End = (_read == _begin) ? (_end - 1) : _end;
		if( chunk1End > _write )
		{
			outBuffers[count].buf = _write;
			outBuffers[count].len = SafeCastToULong(chunk1End - _write);
			count++;
		}

		// 2. Wrap-around 발생 시: _begin부터 (_read - 1) 직전까지
		if( _read > _begin )
		{
			int64 chunk2Len = _read - _begin - 1;
			if( chunk2Len > 0 )
			{
				outBuffers[count].buf = _begin;
				outBuffers[count].len = SafeCastToULong(chunk2Len);
				count++;
			}
		}
	}
	else
	{
		// _write < _read 인 경우: _write부터 (_read - 1) 직전까지
		char* chunkEnd = _read - 1;
		if( chunkEnd > _write )
		{
			outBuffers[0].buf = _write;
			outBuffers[0].len = SafeCastToULong(chunkEnd - _write);
			count = 1;
		}
	}
	return count;
}

//***************************************************************************
// @brief IOCP WSASend를 위한 읽기 가능 영역 WSABUF 배열을 생성합니다 (최대 2개 청크).
// @param outBuffers [out] 크기 2인 WSABUF 배열 (참조로 받아 컴파일 타임 크기 강제)
// @return 생성된 청크 개수 (0 ~ 2)
//***************************************************************************
int CRingBuffer::GetWSASendBuffers(WSABUF(&outBuffers)[2]) const
{
	int count = 0;
	int64 usedSize = GetSizeUsed();
	if( usedSize <= 0 ) return 0;

	if( _write >= _read )
	{
		outBuffers[0].buf = _read;
		outBuffers[0].len = SafeCastToULong(_write - _read);
		if( outBuffers[0].len > 0 )
		{
			count = 1;
		}
	}
	else
	{
		// 1. _read ~ _end
		if( _end > _read )
		{
			outBuffers[count].buf = _read;
			outBuffers[count].len = SafeCastToULong(_end - _read);
			count++;
		}
		// 2. _begin ~ _write
		if( _write > _begin )
		{
			outBuffers[count].buf = _begin;
			outBuffers[count].len = SafeCastToULong(_write - _begin);
			count++;
		}
	}
	return count;
}

//***************************************************************************
// @brief RIO RIOSend를 위한 읽기 가능 영역 RIO_BUF 배열을 생성합니다 (최대 2개 청크).
// @param outBuffers [out] 크기 2인 RIO_BUF 배열 (참조로 받아 컴파일 타임 크기 강제)
// @param bufferId RIO 등록 버퍼 식별자 (RIORegisterBuffer로 발급받은 RIO_BUFFERID)
// @return 생성된 청크 개수 (0 ~ 2)
//***************************************************************************
int CRingBuffer::GetRioSendBuffers(RIO_BUF(&outBuffers)[2], RIO_BUFFERID bufferId) const
{
	int count = 0;
	int64 usedSize = GetSizeUsed();
	if( usedSize <= 0 ) return 0;

	if( _write >= _read )
	{
		outBuffers[0].BufferId = bufferId;
		outBuffers[0].Offset = static_cast<ULONG>(_read - _begin);
		outBuffers[0].Length = SafeCastToULong(_write - _read);
		if( outBuffers[0].Length > 0 )
		{
			count = 1;
		}
	}
	else
	{
		// 1. _read ~ _end
		if( _end > _read )
		{
			outBuffers[count].BufferId = bufferId;
			outBuffers[count].Offset = static_cast<ULONG>(_read - _begin);
			outBuffers[count].Length = SafeCastToULong(_end - _read);
			count++;
		}

		// 2. _begin ~ _write
		if( _write > _begin )
		{
			outBuffers[count].BufferId = bufferId;
			outBuffers[count].Offset = 0;
			outBuffers[count].Length = SafeCastToULong(_write - _begin);
			count++;
		}
	}
	return count;
}

//***************************************************************************
// @brief RIO RIOReceive를 위한 쓰기 가능 영역 RIO_BUF 배열을 생성합니다 (최대 2개 청크).
// @param outBuffers [out] 크기 2인 RIO_BUF 배열 (참조로 받아 컴파일 타임 크기 강제)
// @param bufferId RIO 등록 버퍼 식별자 (RIORegisterBuffer로 발급받은 RIO_BUFFERID)
// @return 생성된 청크 개수 (0 ~ 2)
//***************************************************************************
int CRingBuffer::GetRioRecvBuffers(RIO_BUF(&outBuffers)[2], RIO_BUFFERID bufferId) const
{
	int count = 0;
	int64 freeSize = GetSizeFree();
	if( freeSize <= 0 ) return 0;

	if( _write >= _read )
	{
		// 1. _write부터 _end 직전까지 (단, _read == _begin인 경우 Full 상태와의 구분을 위해 N-1 규칙 적용하여 _end - 1 까지만 사용)
		char* chunk1End = (_read == _begin) ? (_end - 1) : _end;
		if( chunk1End > _write )
		{
			outBuffers[count].BufferId = bufferId;
			outBuffers[count].Offset = static_cast<ULONG>(_write - _begin);
			outBuffers[count].Length = SafeCastToULong(chunk1End - _write);
			count++;
		}

		// 2. Wrap-around 발생 시: _begin부터 (_read - 1) 직전까지
		if( _read > _begin )
		{
			int64 chunk2Len = _read - _begin - 1;
			if( chunk2Len > 0 )
			{
				outBuffers[count].BufferId = bufferId;
				outBuffers[count].Offset = 0;
				outBuffers[count].Length = SafeCastToULong(chunk2Len);
				count++;
			}
		}
	}
	else
	{
		// _write < _read 인 경우: _write부터 (_read - 1) 직전까지
		char* chunkEnd = _read - 1;
		if( chunkEnd > _write )
		{
			outBuffers[0].BufferId = bufferId;
			outBuffers[0].Offset = static_cast<ULONG>(_write - _begin);
			outBuffers[0].Length = SafeCastToULong(chunkEnd - _write);
			count = 1;
		}
	}
	return count;
}
