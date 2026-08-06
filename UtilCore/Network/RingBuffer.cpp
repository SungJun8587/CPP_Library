
//***************************************************************************
// RingBuffer.cpp: implementation of the CRingBuffer class.
//
//***************************************************************************

#include "pch.h"
#include "RingBuffer.h"
#include <cassert>
#include <new>      // std::bad_alloc

//***************************************************************************
// @brief 기본 생성자. 기본 버퍼 사이즈(BUFFER_SIZE_DEFAULT)로 초기화합니다.
//***************************************************************************
CRingBuffer::CRingBuffer() : CRingBuffer(BUFFER_SIZE_DEFAULT)
{

}

//***************************************************************************
// @brief 매개변수로 받은 버퍼 사이즈로 CRingBuffer 객체를 생성합니다.
// @param bufferSize 초기화할 버퍼의 크기
//***************************************************************************
CRingBuffer::CRingBuffer(int bufferSize) : bufferSize(bufferSize)
{
	// capacity-1 방식(빈 상태와 꽉 찬 상태 구분용 1바이트 예약)이므로
	// 최소 2바이트가 있어야 실질적으로 사용 가능한 공간이 생깁니다.
	assert(bufferSize > 1 && "bufferSize는 최소 2 이상이어야 합니다.");

	begin = static_cast<char*>(RawAllocator::Alloc(bufferSize));
	if( begin == nullptr )
	{
		// RawAllocator::Alloc은 실패 시 nullptr을 반환할 수 있음 - 여기서 잡지 않으면
		// end = begin + bufferSize가 nullptr 기반 UB가 되고 이후 memcpy에서 크래시로 이어짐.
		throw std::bad_alloc();
	}
	end = begin + bufferSize;
	read = write = begin;
}

//***************************************************************************
// @brief 이동 생성자. 소유권을 이전하고 원본은 빈 상태로 만듭니다.
//***************************************************************************
CRingBuffer::CRingBuffer(CRingBuffer&& other) noexcept
	: begin(other.begin), end(other.end), bufferSize(other.bufferSize)
	, read(other.read), write(other.write)
{
	other.begin = other.end = other.read = other.write = nullptr;
	other.bufferSize = 0;
}

//***************************************************************************
// @brief 이동 대입 연산자. 기존 버퍼를 해제하고 소유권을 이전합니다.
//***************************************************************************
CRingBuffer& CRingBuffer::operator=(CRingBuffer&& other) noexcept
{
	if( this != &other )
	{
		RawAllocator::Free(begin);

		begin = other.begin;
		end = other.end;
		bufferSize = other.bufferSize;
		read = other.read;
		write = other.write;

		other.begin = other.end = other.read = other.write = nullptr;
		other.bufferSize = 0;
	}
	return *this;
}

//***************************************************************************
// @brief 소멸자. 동적 할당된 메모리를 해제합니다.
//***************************************************************************
CRingBuffer::~CRingBuffer()
{
	RawAllocator::Free(begin);
}

//***************************************************************************
// @brief 큐에 데이터를 삽입합니다.
// @param data 삽입할 버퍼 데이터
// @param requestSize 삽입할 요청 버퍼 크기
// @param outEnqueueSize [out] 성공적으로 삽입된 크기
// @param isPartialEnqueueAvailable true인 경우, 남은 공간만큼 부분 데이터 삽입을 허용합니다
// @return 버퍼 삽입 성공 여부
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
	memcpy(write, data, firstChunkSize);

	int64 secondChunkSize = requestSize - firstChunkSize;
	if( secondChunkSize > 0 )
	{
		memcpy(begin, data + firstChunkSize, secondChunkSize);
	}

	MoveWriteBuffer(requestSize);

	resultSize = requestSize;
	if( outEnqueueSize != nullptr ) *outEnqueueSize = resultSize;
	return true;
}

//***************************************************************************
// @brief 큐에서 데이터를 추출하거나 조회합니다.
// @param outData [out] 데이터를 저장할 목적지 버퍼
// @param requestSize 요청할 버퍼 크기
// @param outDequeueSize [out] 성공적으로 추출된 크기
// @param isPartialDequeueAvailable true인 경우, 사용 중인 크기만큼 부분 데이터 추출을 허용합니다
// @param isPeekMode true인 경우, 데이터를 제거하지 않고 복사만 수행합니다
// @return 버퍼 추출/조회 성공 여부
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
	memcpy(outData, read, firstChunkSize);

	int64 secondChunkSize = requestSize - firstChunkSize;
	if( secondChunkSize > 0 )
	{
		memcpy(outData + firstChunkSize, begin, secondChunkSize);
	}

	if( !isPeekMode ) MoveReadBuffer(requestSize);

	resultSize = requestSize;
	if( outDequeueSize != nullptr ) *outDequeueSize = resultSize;
	return true;
}

//***************************************************************************
// @brief 큐에서 데이터를 제거하지 않고 조회합니다.
// @param outData [out] 데이터를 저장할 목적지 버퍼
// @param requestSize 요청할 버퍼 크기
// @param outPeekSize [out] 성공적으로 조회된 크기
// @param isPartialPeekAvailable true인 경우, 부분 데이터 조회를 허용합니다
// @return 버퍼 조회 성공 여부
//***************************************************************************
bool CRingBuffer::Peek(char* outData, int64 requestSize, int64* outPeekSize, bool isPartialPeekAvailable)
{
	return Dequeue(outData, requestSize, outPeekSize, isPartialPeekAvailable, true);
}