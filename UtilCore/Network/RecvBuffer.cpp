
//***************************************************************************
// RecvBuffer.cpp: implementation of the CRecvBuffer class.
//
//***************************************************************************

#include "pch.h"
#include "RecvBuffer.h"

//***************************************************************************
// @brief CRecvBuffer 객체를 생성합니다.
// @param bufferSize 단위 버퍼 크기
//***************************************************************************
CRecvBuffer::CRecvBuffer(int32 bufferSize) : _bufferSize(bufferSize)
{
	_capacity = bufferSize * BUFFER_COUNT;
	_buffer.resize(_capacity);
}

//***************************************************************************
// @brief CRecvBuffer 소멸자입니다.
//***************************************************************************
CRecvBuffer::~CRecvBuffer()
{
}

//***************************************************************************
// @brief 버퍼의 상태를 정돈하고 읽기/쓰기 커서를 재조정합니다.
//***************************************************************************
void CRecvBuffer::Clean()
{
	int32 dataSize = DataSize();
	if( dataSize == 0 )
	{
		// 남은 데이터가 없으면 읽기/쓰기 커서를 처음 위치로 초기화합니다.
		_readPos = _writePos = 0;
	}
	else
	{
		// 남은 데이터가 버퍼 크기 미만이면, 데이터를 버퍼 앞으로 복사하여 공간을 확보합니다.
		if( FreeSize() < _bufferSize )
		{
			::memcpy(&_buffer[0], &_buffer[_readPos], dataSize);
			_readPos = 0;
			_writePos = dataSize;
		}
	}
}

//***************************************************************************
// @brief 데이터를 읽은 후 읽기 커서를 이동합니다.
// @param numOfBytes 읽어 들인 바이트 수
// @return 성공 여부 (요청한 바이트가 남은 데이터 크기보다 크면 false)
//***************************************************************************
bool CRecvBuffer::OnRead(int32 numOfBytes)
{
	if( numOfBytes > DataSize() )
		return false;

	_readPos += numOfBytes;
	return true;
}

//***************************************************************************
// @brief 데이터를 쓴 후 쓰기 커서를 이동합니다.
// @param numOfBytes 기록된 바이트 수
// @return 성공 여부 (요청한 바이트가 여유 공간보다 크면 false)
//***************************************************************************
bool CRecvBuffer::OnWrite(int32 numOfBytes)
{
	if( numOfBytes > FreeSize() )
		return false;

	_writePos += numOfBytes;
	return true;
}