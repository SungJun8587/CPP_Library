
//***************************************************************************
// RingBuffer.h : interface for the CRingBuffer class.
//
//***************************************************************************

#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

#include <cassert>
#include <new>
#include <stdexcept>
#include <limits>

#ifndef	__ALLOCATOR_H__
#include <Memory/Allocator.h>
#endif

// Windows IOCP 환경에서 WSABUF를 사용하기 위한 헤더
#ifndef _WINSOCK2API_
#include <winsock2.h>
#endif

//***************************************************************************
// @class CRingBuffer
// @brief 멀티스레드 및 IOCP 환경의 제로카피(Zero-Copy) 네트워킹에 최적화된 링 버퍼 클래스입니다.
//
// @details
// 윈도우 IOCP(Input/Output Completion Port) 환경에서 WSABUF를 이용한 제로카피 I/O를 수행할 수 있도록
// 버퍼의 여유/사용 영역을 최대 2개의 연속된 메모리 청크로 나누어 제공합니다.
//
// 주요 처리 및 특징:
//  - 제로카피 네트워킹 지원 (GetWSARecvBuffers, GetWSASendBuffers)
//  - 부분 처리(Partial Enqueue/Dequeue) 및 Peek 모드 지원
//  - 고성능 메모리 관리 및 안전한 타입 캐스팅 검증
//
// 주의사항 (Note):
//  1) 이 클래스 자체는 동기화를 수행하지 않습니다(락프리 아님).
//     멀티스레드 환경(프로듀서/컨슈머 분리 등)에서 사용할 경우 호출자가 외부에서 락(SRWLOCK 등)을 걸어 접근을 직렬화해야 합니다.
//  2) GetWSARecvBuffers()/GetWSASendBuffers()로 얻은 포인터를 실제 I/O에 사용하는 동안(Overlapped 완료 전)
//     같은 방향(recv 또는 send)으로 다시 이 함수들을 호출해 새 I/O를 걸면 안 됩니다. 소켓당 recv 1개, send 1개까지만 
//     동시에 in-flight 상태를 가정합니다.
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

	CRingBuffer(const CRingBuffer&) = delete;
	CRingBuffer& operator=(const CRingBuffer&) = delete;
	CRingBuffer(CRingBuffer&& other) noexcept;
	CRingBuffer& operator=(CRingBuffer&& other) noexcept;

	//***************************************************************************
	// @brief 링버퍼의 전체 용량을 반환합니다 (실제 사용 가능한 최대 데이터 크기).
	// @return 전체 버퍼 크기 (바이트)
	//***************************************************************************
	int64 GetSizeTotal() const
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
			return _end - _write;
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

	//***************************************************************************
	// @brief 링버퍼에 데이터를 씁니다.
	// @param data 쓸 데이터가 위치한 버퍼 포인터
	// @param requestSize 쓰기 요청 크기 (바이트)
	// @param outEnqueueSize [out] 실제로 쓰여진 바이트 수가 저장될 변수 포인터
	// @param isPartialEnqueueAvailable 부분 쓰기 허용 여부 (true인 경우 여유 공간만큼만 쓰고 성공 처리)
	// @return true: 쓰기 성공, false: 공간 부족 및 실패
	// @note data == nullptr 이면서 requestSize > 0 인 경우 실제로는 아무것도
	//       쓰지 않고 outEnqueueSize에 0을 채운 채 true를 반환합니다.
	//***************************************************************************
	bool Enqueue(const char* data, int64 requestSize, int64* outEnqueueSize, bool isPartialEnqueueAvailable = false);

	//***************************************************************************
	// @brief 링버퍼에서 데이터를 읽고 커서를 이동합니다.
	// @param outData 데이터를 복사받을 버퍼 포인터
	// @param requestSize 읽기 요청 크기 (바이트)
	// @param outDequeueSize [out] 실제로 읽혀진 바이트 수가 저장될 변수 포인터
	// @param isPartialDequeueAvailable 부분 읽기 허용 여부
	// @param isPeekMode 읽기 전용 모드 여부 (true인 경우 읽은 후 읽기 커서를 이동하지 않음)
	// @return true: 읽기 성공, false: 데이터 부족 및 실패
	//***************************************************************************
	bool Dequeue(char* outData, int64 requestSize, int64* outDequeueSize, bool isPartialDequeueAvailable = true, bool isPeekMode = false);

	//***************************************************************************
	// @brief 링버퍼에서 데이터를 읽기 커서 이동 없이 복사합니다 (Peek).
	// @param outData 데이터를 복사받을 버퍼 포인터
	// @param requestSize 확인 요청 크기 (바이트)
	// @param outPeekSize [out] 실제로 복사된 바이트 수가 저장될 변수 포인터
	// @param isPartialPeekAvailable 부분 확인 허용 여부
	// @return true: 성공, false: 실패
	//***************************************************************************
	bool Peek(char* outData, int64 requestSize, int64* outPeekSize, bool isPartialPeekAvailable = true);

	//***************************************************************************
	// @brief IOCP WSARecv를 위한 쓰기 가능 영역 WSABUF 배열을 생성합니다 (최대 2개 청크).
	// @param outBuffers [out] 크기 2인 WSABUF 배열 (참조로 받아 컴파일 타임 크기 강제)
	// @return 생성된 청크 개수 (0 ~ 2)
	//***************************************************************************
	int GetWSARecvBuffers(WSABUF(&outBuffers)[2]) const;

	//***************************************************************************
	// @brief IOCP WSASend를 위한 읽기 가능 영역 WSABUF 배열을 생성합니다 (최대 2개 청크).
	// @param outBuffers [out] 크기 2인 WSABUF 배열 (참조로 받아 컴파일 타임 크기 강제)
	// @return 생성된 청크 개수 (0 ~ 2)
	//***************************************************************************
	int GetWSASendBuffers(WSABUF(&outBuffers)[2]) const;

	//***************************************************************************
	// @brief 링버퍼의 모든 데이터를 초기화하고 읽기/쓰기 커서를 시작점으로 되돌립니다.
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
	// @note 버퍼 총 용량이 int32 범위 이하로 제한되어 있으나, 방어적 코드로서
	//       Debug 빌드에서는 assert, Release 빌드에서는 clamp 처리를 수행합니다.
	//***************************************************************************
	static ULONG SafeCastToULong(int64 value)
	{
		constexpr int64 maxVal = static_cast<int64>((std::numeric_limits<ULONG>::max)());

		assert(value >= 0 && value <= maxVal && "WSABUF 길이가 ULONG 범위를 초과했습니다.");

		if( value < 0 ) return 0;
		if( value > maxVal ) return static_cast<ULONG>(maxVal);

		return static_cast<ULONG>(value);
	}

	char* _begin; // 버퍼 메모리 블록의 시작 주소 (고정)
	char* _end;   // 버퍼 메모리 블록의 끝 주소 (_begin + bufferSize, 경계 체크용 고정 주소)
	char* _read;  // 다음에 읽어갈 데이터가 위치한 버퍼 내 읽기 커서 포인터
	char* _write; // 다음에 데이터를 쓸 빈 공간이 위치한 버퍼 내 쓰기 커서 포인터
};

#endif // __RINGBUFFER_H__