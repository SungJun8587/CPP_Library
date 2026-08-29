
//***************************************************************************
// RingBuffer.h : interface for the CRingBuffer class.
//
//***************************************************************************

#ifndef UC_RINGBUFFER_H
#define UC_RINGBUFFER_H

#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

#include <WinSock2.h>
#include <MSWSock.h>

#include <Memory/Allocator.h>

//***************************************************************************
// @class CRingBuffer
// @brief IOCP/RIO 환경의 제로카피(Zero-Copy) 네트워킹에 사용할 수 있도록 설계된 링 버퍼 클래스입니다.
//
// @details
// 윈도우 IOCP(WSABUF) 및 RIO(RIO_BUF) 환경에서 제로카피 I/O를 수행할 수 있도록
// 버퍼의 여유/사용 영역을 최대 2개의 연속된 메모리 청크로 나누어 제공합니다.
//
// 주요 처리 및 특징:
//  - 제로카피 네트워킹 지원 (GetWSARecvBuffers, GetWSASendBuffers, GetRioRecvBuffers, GetRioSendBuffers)
//  - 부분 처리(Partial Enqueue/Dequeue) 및 Peek 모드 지원
//  - 고성능 메모리 관리 및 안전한 타입 캐스팅 검증
//
// [중요 계약 및 주의사항 (Critical Notes)]:
//  1) 동기화 미지원 (Thread Safety):
//     이 클래스 자체는 내부 동기화를 수행하지 않습니다 (락프리 아님).
//     멀티스레드 환경에서는 호출자(예: CRioSession)가 SRWLOCK 등을 통해 접근을 직렬화해야 합니다.
//  2) Move Semantics (이동 연산 계약):
//     이동 연산(Move)에 의해 이동된(moved-from) CRingBuffer 객체는 
//     소멸(Destruction) 또는 새로운 값으로의 이동 대입(Move Assignment) 외의
//     일반적인 RingBuffer 연산(Enqueue, Dequeue, GetSize 등)에 절대 사용할 수 없습니다.
//  3) Overlap 금지 (Buffer Overlap Contract):
//     Enqueue/Dequeue/Peek에 전달되는 외부 데이터 버퍼(data, outData)는 
//     RingBuffer 내부 메모리 영역과 절대 중첩(Overlap)되어서는 안 됩니다.
//  4) I/O Outstanding Lifetime & Cursor Safety (IOCP / RIO 필수 규칙):
//     GetWSARecvBuffers / GetWSASendBuffers / GetRioRecvBuffers / GetRioSendBuffers 로 얻은
//     포인터/버퍼 정보를 실제 I/O에 게시(In-flight)한 경우, **Completion(완료 통지)이 발생하기 전까지**:
//       - 해당 메모리 영역을 덮어쓰거나 재사용해서는 안 됩니다.
//       - MoveReadBuffer(), MoveWriteBuffer(), Clear() 등을 호출하여 커서를 조작해서는 안 됩니다.
//     소켓/세션당 동시에 진행 중인 I/O는 Recv 1개, Send 1개 이하를 전제로 설계되었습니다.
//***************************************************************************
class CRingBuffer : public BaseAllocator
{
	enum Constants : int64
	{
		BUFFER_SIZE_DEFAULT = 10240
	};

public:
	CRingBuffer();
	explicit CRingBuffer(int64 bufferSize);
	~CRingBuffer();

	CRingBuffer(const CRingBuffer&) = delete;
	CRingBuffer& operator=(const CRingBuffer&) = delete;

	CRingBuffer(CRingBuffer&& other) noexcept;
	CRingBuffer& operator=(CRingBuffer&& other) noexcept;

	//***************************************************************************
	// @brief 링버퍼에 저장 가능한 최대 데이터 크기(논리 Capacity, N-1)를 반환합니다.
	// @return 최대 저장 가능 데이터 크기 (바이트)
	//***************************************************************************
	int64 GetCapacity() const
	{
		return _end - _begin - 1;
	}

	//***************************************************************************
	// @brief 링버퍼에 남은 빈 공간의 크기를 반환합니다.
	// @return 잔여 여유 공간 크기 (바이트)
	//***************************************************************************
	int64 GetSizeFree() const
	{
		if( _write >= _read )
		{
			return (_end - _write) + (_read - _begin) - 1;
		}
		return _read - _write - 1;
	}

	//***************************************************************************
	// @brief 링버퍼에 저장된 데이터의 총 크기를 반환합니다.
	// @return 사용 중인 데이터 크기 (바이트)
	//***************************************************************************
	int64 GetSizeUsed() const
	{
		if( _write >= _read )
		{
			return _write - _read;
		}
		return (_write - _begin) + (_end - _read);
	}

	//***************************************************************************
	// @brief 포인터 래핑 없이 한 번에 연속으로 쓸 수 있는 최대 크기를 반환합니다.
	// @return 직접 쓰기 가능한 청크 크기 (바이트)
	//***************************************************************************
	int64 GetSizeDirectEnqueueAble() const
	{
		if( _write >= _read )
		{
			return (_read == _begin) ? (_end - _write - 1) : (_end - _write);
		}
		return _read - _write - 1;
	}

	//***************************************************************************
	// @brief 포인터 래핑 없이 한 번에 연속으로 읽을 수 있는 최대 크기를 반환합니다.
	// @return 직접 읽기 가능한 청크 크기 (바이트)
	//***************************************************************************
	int64 GetSizeDirectDequeueAble() const
	{
		if( _write >= _read )
		{
			return _write - _read;
		}
		return _end - _read;
	}

	bool Enqueue(const char* data, int64 requestSize, int64* outEnqueueSize = nullptr, bool isPartialEnqueueAvailable = false);
	bool Dequeue(char* outData, int64 requestSize, int64* outDequeueSize = nullptr, bool isPartialDequeueAvailable = true, bool isPeekMode = false);

	bool Peek(char* outData, int64 requestSize, int64* outPeekSize = nullptr, bool isPartialPeekAvailable = true);

	int GetWSARecvBuffers(WSABUF(&outBuffers)[2]) const;
	int GetWSASendBuffers(WSABUF(&outBuffers)[2]) const;

	int GetRioSendBuffers(RIO_BUF(&outBuffers)[2], RIO_BUFFERID bufferId) const;
	int GetRioRecvBuffers(RIO_BUF(&outBuffers)[2], RIO_BUFFERID bufferId) const;

	//***************************************************************************
	// @brief 링버퍼의 모든 데이터를 초기화하고 읽기/쓰기 커서를 시작점으로 되돌립니다.
	// @note 진행 중인(In-flight) Async I/O가 없을 때만 안전하게 호출할 수 있습니다.
	//***************************************************************************
	void Clear()
	{
		_read = _write = _begin;
	}

	//***************************************************************************
	// @brief 읽기 커서를 지정한 크기만큼 강제로 이동합니다.
	// @param moveSize 이동할 바이트 수
	// @return true: 이동 성공, false: 유효 범위를 벗어난 경우
	//***************************************************************************
	bool MoveReadBuffer(int64 moveSize)
	{
		if( moveSize < 0 || moveSize > GetSizeUsed() ) return false;
		char* readPointer = _read + moveSize;
		if( readPointer >= _end )
		{
			int64 adjust = readPointer - _end;
			_read = _begin + adjust;
			return true;
		}
		_read = readPointer;
		return true;
	}

	//***************************************************************************
	// @brief 쓰기 커서를 지정한 크기만큼 강제로 이동합니다.
	// @param moveSize 이동할 바이트 수
	// @return true: 이동 성공, false: 유효 범위를 벗어난 경우
	//***************************************************************************
	bool MoveWriteBuffer(int64 moveSize)
	{
		if( moveSize < 0 || moveSize > GetSizeFree() ) return false;
		char* writePointer = _write + moveSize;
		if( writePointer >= _end )
		{
			int64 adjust = writePointer - _end;
			_write = _begin + adjust;
			return true;
		}
		_write = writePointer;
		return true;
	}

	//***************************************************************************
	// @brief 현재 읽기 버퍼 포인터를 반환합니다.
	//***************************************************************************
	inline char* GetReadBuffer() const
	{
		return _read;
	}

	//***************************************************************************
	// @brief 현재 쓰기 버퍼 포인터를 반환합니다.
	//***************************************************************************
	inline char* GetWriteBuffer() const
	{
		return _write;
	}

	//***************************************************************************
	// @brief 버퍼 메모리의 시작 주소를 반환합니다.
	//***************************************************************************
	inline char* GetBufferBegin() const
	{
		return _begin;
	}

	//***************************************************************************
	// @brief 버퍼 메모리의 끝 주소를 반환합니다.
	//***************************************************************************
	inline char* GetBufferEnd() const
	{
		return _end;
	}

private:
	//***************************************************************************
	// @brief int64 크기 값을 WSABUF::len(ULONG, 32bit)으로 안전하게 캐스팅합니다.
	// @param value 캐스팅할 int64 크기 값
	// @return ULONG 타입으로 변환된 값
	//***************************************************************************
	static ULONG SafeCastToULong(int64 value)
	{
		constexpr int64 maxVal = static_cast<int64>((std::numeric_limits<ULONG>::max)());

		assert(value >= 0 && value <= maxVal && "WSABUF/RIO_BUF 길이가 ULONG 범위를 초과했습니다.");

		return static_cast<ULONG>(value);
	}

	char* _begin; // 버퍼 메모리 블록의 시작 주소 (고정)
	char* _end;   // 버퍼 메모리 블록의 끝 주소 (_begin + bufferSize, 경계 체크용 고정 주소)
	char* _read;  // 다음에 읽어갈 데이터가 위치한 버퍼 내 읽기 커서 포인터
	char* _write; // 다음에 데이터를 쓸 빈 공간이 위치한 버퍼 내 쓰기 커서 포인터
};

#endif // ndef UC_RINGBUFFER_H