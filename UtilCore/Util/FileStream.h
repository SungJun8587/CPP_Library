
//***************************************************************************
// FileStream.h : interface for the CFileStream class.
//
//***************************************************************************

#ifndef UC_FILESTREAM_H
#define UC_FILESTREAM_H

#include <filesystem>
#include <fstream>
#include <cstdint>

//***************************************************************************
// @class CFileStream
// @brief 대용량 파일의 메모리 과부하 없는 청크 단위 바이너리 읽기 스트림 클래스
// @details
// [사용처 및 설계 목적]
// 1. 대용량 바이너리 파일(동영상, 클라이언트 패치 파일, 압축 파일 등) 전송 및 다운로드 처리
// 2. 파일 전체를 메모리에 한 번에 로드하지 않고, 설정한 청크 크기(Chunk Size)만큼 분할해서 읽어오는 기능 제공
// 3. 네트워크 패킷 전송 시 고정된 버퍼 크기를 유지하여 서버 메모리 사용량 최소화
//***************************************************************************
class CFileStream
{
public:
	CFileStream() = default;
	~CFileStream() = default;

	// 복사 금지(std::ifstream 자체가 복사 불가)
	CFileStream(const CFileStream&) = delete;
	CFileStream& operator=(const CFileStream&) = delete;

	//***************************************************************************
	// @brief 이동 생성자.
	// @param other 이동할 대상 CFileStream 객체.
	// @details std::ifstream 스트림 자원을 이관하고 moved-from 객체의 _fileSize를 
	//          0으로 초기화하여 잔여 데이터 상태를 남기지 않는다.
	//***************************************************************************
	CFileStream(CFileStream&& other) noexcept
		: _file(std::move(other._file))
		, _fileSize(other._fileSize)
	{
		other._fileSize = 0;
	}

	//***************************************************************************
	// @brief 이동 대입 연산자.
	// @param other 이동할 대상 CFileStream 객체.
	// @return CFileStream 자기 자신에 대한 참조.
	// @details 기존 열려있던 스트림을 먼저 안전하게 Close()한 뒤 자원을 이관받는다.
	//          자기 자신 대입(self-assignment) 방지 및 moved-from 객체의 
	//          _fileSize 초기화를 수행한다.
	//***************************************************************************
	CFileStream& operator=(CFileStream&& other) noexcept
	{
		if( this != &other )
		{
			Close();
			_file = std::move(other._file);
			_fileSize = other._fileSize;
			other._fileSize = 0;
		}
		return *this;
	}

	//***************************************************************************
	// @brief fullPath의 파일을 바이너리 모드로 연다.
	// @param fullPath 열고자 하는 파일의 경로
	// @return 성공 시 true(실패 시 IsOpen()도 false로 유지됨).
	//***************************************************************************
	bool Open(const std::filesystem::path& fullPath);

	//***************************************************************************
	// @brief 파일 스트림이 열려 있는지 여부를 반환한다.
	// @return 파일이 열려 있으면 true, 그렇지 않으면 false.
	//***************************************************************************
	bool IsOpen() const { return _file.is_open(); }

	//***************************************************************************
	// @brief Open() 성공 시점에 확인해둔 파일 전체 크기(바이트)를 반환한다.
	// @return 파일 크기(바이트 단위).
	//***************************************************************************
	int64 GetFileSize() const { return _fileSize; }

	//***************************************************************************
	// @brief 다음 ReadChunk()가 시작될 파일 내 절대 위치를 옮긴다.
	// @param offset 이동시킬 위치 (0 ~ _fileSize)
	// @return 성공 시 true(offset이 범위 밖이면 false).
	//***************************************************************************
	bool Seek(int64 offset);

	//***************************************************************************
	// @brief 현재 위치부터 최대 bufferSize바이트를 읽어 buffer에 채운다.
	// @param buffer 데이터를 읽어올 버퍼 메모리 포인터
	// @param bufferSize 버퍼의 크기(바이트)
	// @return 실제로 읽은 바이트 수.
	//         0이면 데이터를 더 이상 읽지 못한 상태(EOF 등)를 의미하며,
	//         이 때 HasError()가 true이면 실제 스트림 I/O 오류이다.
	//***************************************************************************
	size_t ReadChunk(BYTE* buffer, size_t bufferSize);

	//***************************************************************************
	// @brief 열려있는 파일 스트림을 닫고 상태를 초기화한다.
	//***************************************************************************
	void Close();

	//***************************************************************************
	// @brief EOF가 아닌 실제 스트림 I/O 오류(badbit) 발생 여부를 반환한다.
	// @return badbit가 설정되어 있으면 true, 그렇지 않으면 false.
	// @details EOF 판정 시 설정되는 eofbit/failbit는 정상 수신 종료 상태이므로
	//          오류로 취급하지 않는다.
	//***************************************************************************
	bool HasError() const;

private:
	std::ifstream	_file;				// 파일 I/O 처리를 위한 C++ 표준 입력 파일 스트림 객체
	int64			_fileSize = 0;		// Open() 성공 시점에 측정해둔 전체 파일 크기(바이트 단위)
};

#endif // ndef UC_FILESTREAM_H