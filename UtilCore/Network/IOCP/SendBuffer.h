
//***************************************************************************
// SendBuffer.h : interface for the CSendBuffer class.
//
//***************************************************************************

#ifndef __SENDBUFFER_H__
#define __SENDBUFFER_H__

#ifndef	__CONTAINERS_H__
#include <Memory/Containers.h>
#endif

#ifndef	__IOCPCOMMON_H__
#include <Network/IOCP/IocpCommon.h>
#endif

#include <array>

extern thread_local CSendBufferChunkRef	LSendBufferChunk;

class CSendBufferChunk;

//***************************************************************************
// @class SendBuffer
// @brief 네트워크 전송 데이터를 버퍼링하고 관리하는 클래스입니다.
//
// @details
// SendBufferChunk로부터 메모리를 할당받아 전송할 데이터를 기록하고,
// 실제 전송 크기를 설정(Close)하여 소유 중인 청크에 완료를 알립니다.
//
// 주요 처리 및 특징:
//  - 청크 기반의 메모리 관리 및 효율적인 버퍼 재사용
//  - 할당된 크기와 실제 기록된 크기의 분리를 통한 안전성 확보
//***************************************************************************
class CSendBuffer
{
public:
	CSendBuffer(CSendBufferChunkRef owner, BYTE* buffer, uint32 allocSize);
	~CSendBuffer();

	BYTE*		Buffer() { return _buffer; }
	uint32		AllocSize() { return _allocSize; }
	uint32		WriteSize() { return _writeSize; }
	void		Close(uint32 writeSize);

private:
	BYTE*				_buffer;		// 버퍼 시작 주소
	uint32				_allocSize = 0;	// 할당된 크기
	uint32				_writeSize = 0;	// 실제 기록된 크기
	CSendBufferChunkRef	_owner;			// 소유 중인 청크 참조
};


//***************************************************************************
// @class SendBufferChunk
// @brief SendBuffer들을 담는 일정한 크기의 메모리 청크 클래스입니다.
//
// @details
// 고정 크기(SEND_BUFFER_CHUNK_SIZE)의 메모리 배열을 관리하며,
// 요청된 크기만큼 SendBuffer를 생성하여 반환합니다.
//
// 주요 처리 및 특징:
//  - 고정 크기 버퍼 배열(SEND_BUFFER_CHUNK_SIZE) 활용
//  - 스마트 포인터(enable_shared_from_this) 기반의 수명 주기 관리
//  - 남은 여유 공간(FreeSize) 계산 및 오픈/클로즈 상태 관리
//***************************************************************************
class CSendBufferChunk : public enable_shared_from_this<CSendBufferChunk>
{
	enum
	{
		SEND_BUFFER_CHUNK_SIZE = 6000
	};

public:
	CSendBufferChunk();
	~CSendBufferChunk();

	void				Reset();
	CSendBufferRef		Open(uint32 allocSize);
	void				Close(uint32 writeSize);

	bool				IsOpen() { return _open; }
	BYTE*				Buffer() { return &_buffer[_usedSize]; }
	uint32				FreeSize() { return static_cast<uint32>(_buffer.size()) - _usedSize; }

private:
	std::array<BYTE, SEND_BUFFER_CHUNK_SIZE>	_buffer = {};		// 청크 메모리 버퍼
	bool										_open = false;		// 오픈 상태 여부
	uint32										_usedSize = 0;		// 사용된 메모리 크기
};

//***************************************************************************
// @class SendBufferManager
// @brief SendBufferChunk를 관리하고 할당하는 매니저 클래스입니다.
//
// @details
// 스레드 컨텍스트에서 SendBufferChunk를 효율적으로 공급하며,
// 청크의 여유 공간이 부족할 경우 새로운 청크를 풀에서 가져와 교체합니다.
//
// 주요 처리 및 특징:
//  - 객체 풀(ObjectPool)을 통한 SendBufferChunk 재사용 및 성능 최적화
//  - 필요에 따른 동적 청크 교체 및 오픈 처리
//***************************************************************************
class CSendBufferManager
{
public:
	CSendBufferRef		Open(uint32 size);

private:
	static CSendBufferChunkRef	GetChunk();
};

#endif // __SENDBUFFER_H__