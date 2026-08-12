
//***************************************************************************
// RecvBuffer.h : interface for the CRecvBuffer class.
//
//***************************************************************************

#ifndef __RECVBUFFER_H__
#define __RECVBUFFER_H__

#ifndef	__IOCPCOMMON_H__
#include <Network/IOCP/IocpCommon.h>
#endif

//***************************************************************************
// @class RecvBuffer
// @brief 네트워크 수신 데이터를 버퍼링하고 관리하는 클래스입니다.
//
// @details
// 소켓으로부터 수신한 데이터를 버퍼에 저장하고, 읽기/쓰기 커서 위치(ReadPos, WritePos)를
// 조절하며, 여유 공간이 부족할 경우 데이터를 앞으로 당겨 정돈(Clean)하는 기능을 제공합니다.
//
// 주요 처리 및 특징:
//  - 단위 버퍼 크기의 배수(BUFFER_COUNT)만큼 전체 수신 버퍼 용량 확보
//  - 읽기/쓰기 작업에 따른 커서 이동 및 유효성 검증
//  - 여유 공간 부족 시 데이터 시프트(Memcpy)를 통한 공간 최적화
//***************************************************************************
class CRecvBuffer
{
	enum { BUFFER_COUNT = 10 };

public:
	CRecvBuffer(int32 bufferSize);
	~CRecvBuffer();

	void			Clean();
	bool			OnRead(int32 numOfBytes);
	bool			OnWrite(int32 numOfBytes);

	BYTE*			ReadPos() { return &_buffer[_readPos]; }
	BYTE*			WritePos() { return &_buffer[_writePos]; }
	int32			DataSize() { return _writePos - _readPos; }
	int32			FreeSize() { return _capacity - _writePos; }

private:
	int32			_capacity = 0;		// 전체 버퍼 용량
	int32			_bufferSize = 0;	// 단위 버퍼 크기
	int32			_readPos = 0;		// 읽기 커서 위치
	int32			_writePos = 0;		// 쓰기 커서 위치
	CVector<BYTE>	_buffer;			// 수신 데이터 저장 버퍼
};

#endif // __RECVBUFFER_H__