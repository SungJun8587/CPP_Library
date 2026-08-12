
//***************************************************************************
// SendBuffer.cpp: implementation of the CSendBuffer class.
//
//***************************************************************************

#include "pch.h"
#include "SendBuffer.h"

//***************************************************************************
// @brief CSendBuffer 객체를 생성합니다.
// @param owner 이 버퍼를 소유하는 SendBufferChunk 참조
// @param buffer 버퍼 시작 주소
// @param allocSize 할당된 크기
//***************************************************************************
CSendBuffer::CSendBuffer(CSendBufferChunkRef owner, BYTE* buffer, uint32 allocSize)
	: _owner(owner), _buffer(buffer), _allocSize(allocSize)
{
}

//***************************************************************************
// @brief CSendBuffer 소멸자입니다.
//***************************************************************************
CSendBuffer::~CSendBuffer()
{
}

//***************************************************************************
// @brief 전송 버퍼의 사용을 완료하고 실제 기록된 크기를 설정합니다.
// @param writeSize 실제 기록된 데이터 크기
//***************************************************************************
void CSendBuffer::Close(uint32 writeSize)
{
	ASSERT_CRASH(_allocSize >= writeSize);
	_writeSize = writeSize;
	_owner->Close(writeSize);
}


//***************************************************************************
// @brief CSendBufferChunk 객체를 생성합니다.
//***************************************************************************
CSendBufferChunk::CSendBufferChunk()
{
}

//***************************************************************************
// @brief CSendBufferChunk 소멸자입니다.
//***************************************************************************
CSendBufferChunk::~CSendBufferChunk()
{
}

//***************************************************************************
// @brief 청크의 상태를 초기화합니다.
//***************************************************************************
void CSendBufferChunk::Reset()
{
	_open = false;
	_usedSize = 0;
}

//***************************************************************************
// @brief 청크 내에서 지정된 크기만큼의 SendBuffer를 엽니다.
// @param allocSize 할당할 크기
// @return 생성된 SendBuffer 스마트 포인터 (실패 시 nullptr)
//***************************************************************************
CSendBufferRef CSendBufferChunk::Open(uint32 allocSize)
{
	ASSERT_CRASH(allocSize <= SEND_BUFFER_CHUNK_SIZE);
	ASSERT_CRASH(_open == false);

	if( allocSize > FreeSize() )
		return nullptr;

	_open = true;
	return CObjectPool<CSendBuffer>::MakeShared(shared_from_this(), Buffer(), allocSize);
}

//***************************************************************************
// @brief 청크 내 열린 버퍼의 사용을 완료합니다.
// @param writeSize 기록된 데이터 크기
//***************************************************************************
void CSendBufferChunk::Close(uint32 writeSize)
{
	ASSERT_CRASH(_open == true);
	_open = false;
	_usedSize += writeSize;
}


//***************************************************************************
// @brief 지정된 크기의 SendBuffer를 오픈하여 반환합니다.
// @param size 요청할 크기
// @return 생성된 SendBuffer 스마트 포인터
//***************************************************************************
CSendBufferRef CSendBufferManager::Open(uint32 size)
{
	if( LSendBufferChunk == nullptr )
		LSendBufferChunk = GetChunk();

	ASSERT_CRASH(LSendBufferChunk->IsOpen() == false);

	// 이 청크에 여유가 없으면 새로 교체
	if( LSendBufferChunk->FreeSize() < size )
		LSendBufferChunk = GetChunk();

	return LSendBufferChunk->Open(size);
}

//***************************************************************************
// @brief 새로운 SendBufferChunk를 가져오거나 풀에서 할당받습니다.
// @return 할당된 SendBufferChunkRef 스마트 포인터
//***************************************************************************
CSendBufferChunkRef CSendBufferManager::GetChunk()
{
	CSendBufferChunkRef chunk(CObjectPool<CSendBufferChunk>::Pop(), CObjectPool<CSendBufferChunk>::Push);
	chunk->Reset();
	return chunk;
}