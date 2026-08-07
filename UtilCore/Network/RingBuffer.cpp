
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
// @param bufferSize 할당할 버퍼 크기 (바이트, 최소 2 이상)
//***************************************************************************
CRingBuffer::CRingBuffer(int bufferSize)
{
	if( bufferSize <= 1 )
	{
		throw std::invalid_argument("CRingBuffer: bufferSize는 최소 2 이상이어야 합니다.");
	}

	_begin = static_cast<char*>(RawAllocator::Alloc(bufferSize));
	if( _begin == nullptr )
	{
		throw std::bad_alloc();
	}
	_end = _begin + bufferSize;
	_read = _write = _begin;
}

//***************************************************************************
// @brief 이동 생성자 구현
//***************************************************************************
CRingBuffer::CRingBuffer(CRingBuffer&& other) noexcept
	: _begin(other._begin), _end(other._end)
	, _read(other._read), _write(other._write)
{
	other._begin = other._end = other._read = other._write = nullptr;
}

//***************************************************************************
// @brief 이동 대입 연산자 구현
//***************************************************************************
CRingBuffer& CRingBuffer::operator=(CRingBuffer&& other) noexcept
{
	if( this != &other )
	{
		// NOTE: RawAllocator::Free(nullptr)의 안전성이 Allocator.h 구현에
		// 달려있어 확정할 수 없으므로, 방어적으로 nullptr이 아닐 때만 해제합니다.
		if( _begin != nullptr )
		{
			RawAllocator::Free(_begin);
		}

		_begin = other._begin;
		_end = other._end;
		_read = other._read;
		_write = other._write;

		other._begin = other._end = other._read = other._write = nullptr;
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
//***************************************************************************
bool CRingBuffer::Enqueue(const char* data, int64 requestSize, int64* outEnqueueSize, bool isPartialEnqueueAvailable)
{
	int64 resultSize = 0;
	int64 freeSize = GetSizeFree();

	if( freeSize < requestSize )
	{
		if( isPartialEnqueueAvailable )
		{
			requestSize = freeSize;
		}
		else
		{
			if( outEnqueueSize != nullptr ) *outEnqueueSize = resultSize;
			return false;
		}
	}

	if( requestSize <= 0 || data == nullptr )
	{
		if( outEnqueueSize != nullptr ) *outEnqueueSize = resultSize;
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

	resultSize = requestSize;
	if( outEnqueueSize != nullptr ) *outEnqueueSize = resultSize;
	return true;
}

//***************************************************************************
// @brief 링버퍼에서 데이터를 읽습니다.
//***************************************************************************
bool CRingBuffer::Dequeue(char* outData, int64 requestSize, int64* outDequeueSize, bool isPartialDequeueAvailable, bool isPeekMode)
{
	int64 resultSize = 0;
	int64 usedSize = GetSizeUsed();

	if( usedSize < requestSize )
	{
		if( isPartialDequeueAvailable )
		{
			requestSize = usedSize;
		}
		else
		{
			if( outDequeueSize != nullptr ) *outDequeueSize = resultSize;
			return false;
		}
	}

	if( requestSize <= 0 || outData == nullptr )
	{
		if( outDequeueSize != nullptr ) *outDequeueSize = resultSize;
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

	resultSize = requestSize;
	if( outDequeueSize != nullptr ) *outDequeueSize = resultSize;
	return true;
}

//***************************************************************************
// @brief 링버퍼에서 데이터를 읽기 커서 변경 없이 복사합니다.
//***************************************************************************
bool CRingBuffer::Peek(char* outData, int64 requestSize, int64* outPeekSize, bool isPartialPeekAvailable)
{
	return Dequeue(outData, requestSize, outPeekSize, isPartialPeekAvailable, true);
}

//***************************************************************************
// @brief WSARecv를 위한 쓰기 가능 영역 WSABUF 추출
//***************************************************************************
int CRingBuffer::GetWSARecvBuffers(WSABUF(&outBuffers)[2]) const
{
	int count = 0;
	int64 freeSize = GetSizeFree();
	if( freeSize <= 0 ) return 0;

	if( _write >= _read )
	{
		// 1. _write ~ (_end - 1 if _read == _begin else _end)
		char* chunk1End = (_read == _begin) ? (_end - 1) : _end;
		if( chunk1End > _write )
		{
			outBuffers[count].buf = _write;
			outBuffers[count].len = SafeCastToULong(chunk1End - _write);
			count++;
		}

		// 2. _begin ~ (_read - 1)
		if( _read > _begin )
		{
			char* chunk2End = _read - 1;
			if( chunk2End > _begin )
			{
				outBuffers[count].buf = _begin;
				outBuffers[count].len = SafeCastToULong(chunk2End - _begin);
				count++;
			}
		}
	}
	else
	{
		// _write < _read 인 경우: _write ~ (_read - 1)
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
// @brief WSASend를 위한 읽기 가능 영역 WSABUF 추출
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