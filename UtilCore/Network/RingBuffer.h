
//***************************************************************************
// RingBuffer.h : interface for the CRingBuffer class.
//
//***************************************************************************

#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

#ifndef	__ALLOCATOR_H__
#include <Memory/Allocator.h>
#endif

//***************************************************************************
// @class CRingBuffer
// @brief 멀티스레드 환경의 버퍼링에 최적화된 링 버퍼(Circular Buffer) 클래스입니다.
//
// @details
// 고정 크기의 메모리를 할당하여 데이터의 Enqueue 및 Dequeue를 효율적으로 수행하며,
// 부분 인큐/디큐 및 Peek 기능을 제공합니다. 
// (※ 주의: 단일 스레드 혹은 외부 동기화 환경을 가정하며, 
//  링버퍼 자체의 스레드 세이프티가 필요한 경우 외부에서 락 처리를 동반해야 합니다.)
//
// 주요 처리 및 특징:
//  - BaseAllocator 및 RawAllocator 기반의 고성능 동적 메모리 순환 큐 구현
//  - 부분 데이터 처리(Partial Enqueue/Dequeue) 지원
//  - 데이터 삭제 없는 조회(Peek 모드) 지원
//***************************************************************************
class CRingBuffer : public BaseAllocator
{
	enum Constants
	{
		BUFFER_SIZE_DEFAULT = 10240
	};

public:
	CRingBuffer();
	CRingBuffer(int bufferSize);
	~CRingBuffer();

	// begin이 소유권을 갖는 raw pointer이므로 얕은 복사(이중 Free/댕글링)를 막기 위해 복사는 금지하고,
	// 대신 소유권 이전이 가능한 이동만 허용합니다.
	CRingBuffer(const CRingBuffer&) = delete;
	CRingBuffer& operator=(const CRingBuffer&) = delete;
	CRingBuffer(CRingBuffer&& other) noexcept;
	CRingBuffer& operator=(CRingBuffer&& other) noexcept;

	//***************************************************************************
	// @brief 버퍼의 총 크기를 리턴합니다.
	// @return 버퍼의 총 크기
	//***************************************************************************
	int64 GetSizeTotal() const
	{
		return end - begin - 1;
	}

	//***************************************************************************
	// @brief 버퍼의 남은 크기를 리턴합니다.
	// @return 버퍼의 남은 크기
	//***************************************************************************
	int64 GetSizeFree() const
	{
		if( write >= read )
		{
			return (end - write) + (read - begin) - 1;
		}
		return read - write - 1;
	}

	//***************************************************************************
	// @brief 버퍼의 사용중인 크기를 리턴합니다.
	// @return 버퍼의 사용중인 크기
	//***************************************************************************
	int64 GetSizeUsed() const
	{
		if( write >= read )
		{
			return write - read;
		}
		return (write - begin) + (end - read);
	}

	//***************************************************************************
	// @brief 끊김 없이 연속으로 인큐 가능한 사이즈를 리턴합니다.
	// @return 끊김 없이 연속으로 인큐 가능한 사이즈
	//***************************************************************************
	int64 GetSizeDirectEnqueueAble() const
	{
		if( write >= read )
		{
			return end - write;
		}
		return read - write - 1;
	}

	//***************************************************************************
	// @brief 끊김 없이 연속으로 디큐 가능한 사이즈를 리턴합니다.
	// @return 끊김 없이 연속으로 디큐 가능한 사이즈
	//***************************************************************************
	int64 GetSizeDirectDequeueAble() const
	{
		if( write >= read )
		{
			return write - read;
		}
		return end - read;
	}

	bool Enqueue(const char* data, int64 requestSize, int64* outEnqueueSize, bool isPartialEnqueueAvailable = false);
	bool Dequeue(char* outData, int64 requestSize, int64* outDequeueSize, bool isPartialDequeueAvailable = true, bool isPeekMode = false);
	bool Peek(char* outData, int64 requestSize, int64* outPeekSize, bool isPartialPeekAvailable = true);

	//***************************************************************************
	// @brief 링버퍼를 초기화합니다.
	//***************************************************************************
	void Clear()
	{
		read = write = begin;
	}

	//***************************************************************************
	// @brief 링버퍼의 읽기 포인터를 이동시킵니다.
	// @param moveSize 포인터를 이동시킬 크기
	// @return 성공 여부
	//***************************************************************************
	bool MoveReadBuffer(int64 moveSize)
	{
		// moveSize가 실제 사용 중인(읽어갈 수 있는) 크기를 넘어서면 read가 write를 앞지르며
		// 링버퍼 불변식이 깨지므로(예: GetWriteBuffer()로 직접 써넣는 외부 코드의 크기 계산 실수),
		// 여기서 상한을 방어적으로 검증합니다.
		if( moveSize < 0 || moveSize > GetSizeUsed() ) return false;
		char* readPointer = read + moveSize;
		if( readPointer >= end )
		{
			int64 adjust = readPointer - end;
			read = begin + adjust;
			return true;
		}
		read = readPointer;
		return true;
	}

	//***************************************************************************
	// @brief 링버퍼의 쓰기 포인터를 이동시킵니다.
	// @param moveSize 포인터를 이동시킬 크기
	// @return 성공 여부
	//***************************************************************************
	bool MoveWriteBuffer(int64 moveSize)
	{
		// moveSize가 실제 남은 공간을 넘어서면 write가 read를 앞지르며 링버퍼 불변식이 깨지므로
		// (예: GetWriteBuffer()로 직접 recv() 후 실제 수신 크기를 잘못 넘기는 실수 방지),
		// 여기서 상한을 방어적으로 검증합니다.
		if( moveSize < 0 || moveSize > GetSizeFree() ) return false;
		char* writePointer = write + moveSize;
		if( writePointer >= end )
		{
			int64 adjust = writePointer - end;
			write = begin + adjust;
			return true;
		}
		write = writePointer;
		return true;
	}

	//***************************************************************************
	// @brief 링버퍼의 읽기 포인터를 리턴합니다.
	// @return 읽기 포인터
	//***************************************************************************
	inline char* GetReadBuffer() const
	{
		return read;
	}

	//***************************************************************************
	// @brief 링버퍼의 쓰기 포인터를 리턴합니다.
	// @return 쓰기 포인터
	//***************************************************************************
	inline char* GetWriteBuffer() const
	{
		return write;
	}

	//***************************************************************************
	// @brief 링버퍼의 시작 포인터를 리턴합니다.
	// @return 시작 포인터
	//***************************************************************************
	inline char* GetBufferBegin() const
	{
		return begin;
	}

	//***************************************************************************
	// @brief 링버퍼의 끝 포인터를 리턴합니다.
	// @return 끝 포인터
	inline char* GetBufferEnd() const
	{
		return end;
	}

private:
	char*	begin;				// 버퍼의 시작 메모리 주소
	char*	end;				// 버퍼의 끝 메모리 주소 (begin + bufferSize)
	int32	bufferSize;			// 버퍼의 전체 크기
	char*	read;				// 데이터를 읽어갈 위치를 가리키는 읽기 포인터
	char*	write;				// 데이터를 기록할 위치를 가리키는 쓰기 포인터
};

#endif // __RINGBUFFER_H__