
//***************************************************************************
// FileStream.cpp: implementation of the CFileStream class.
//
//***************************************************************************

#include "pch.h"
#include "FileStream.h"

//***************************************************************************
// @brief fullPath의 파일을 바이너리 모드로 연다.
// @param fullPath 열고자 하는 파일의 경로
// @return 성공 시 true(실패 시 IsOpen()도 false로 유지됨).
//***************************************************************************
bool CFileStream::Open(const std::filesystem::path& fullPath)
{
	// 이미 열려있는 스트림에 다시 Open()을 호출하는 경우를 대비해 먼저 닫는다.
	Close();

	_file.open(fullPath, std::ios::binary | std::ios::ate);
	if( !_file.is_open() )
		return false;

	const std::streamoff size = _file.tellg();
	if( size < 0 )
	{
		Close();
		return false;
	}

	_fileSize = static_cast<int64_t>(size);
	_file.seekg(0, std::ios::beg);
	if( !_file )
	{
		Close();
		return false;
	}

	return true;
}

//***************************************************************************
// @brief 다음 ReadChunk()가 시작될 파일 내 절대 위치를 옮긴다.
// @param offset 이동시킬 위치 (0 ~ _fileSize)
// @return 성공 시 true(offset이 범위 밖이면 false).
//***************************************************************************
bool CFileStream::Seek(int64 offset)
{
	if( !IsOpen() || offset < 0 || offset > _fileSize )
		return false;

	_file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
	return _file.good();
}

//***************************************************************************
// @brief 현재 위치부터 최대 bufferSize바이트를 읽어 buffer에 채운다.
// @param buffer 데이터를 읽어올 버퍼 메모리 포인터
// @param bufferSize 버퍼의 크기(바이트)
// @return 실제로 읽은 바이트 수.
//***************************************************************************
size_t CFileStream::ReadChunk(BYTE* buffer, size_t bufferSize)
{
	if( !IsOpen() || buffer == nullptr || bufferSize == 0 )
		return 0;

	// 이전 read()에서 EOF 상태가 이미 설정된 경우에는
	// 더 읽을 데이터가 없으므로 즉시 0을 반환한다.
	if( _file.eof() )
		return 0;

	_file.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(bufferSize));

	// read()는 요청한 만큼 다 못 읽으면(파일 끝에 도달) failbit+eofbit를
	// 세우지만, gcount()는 그래도 "실제로 읽은 바이트 수"를 정확히
	// 돌려준다 — 그 값을 그대로 쓰면 된다(에러로 취급하지 않음).
	return static_cast<size_t>(_file.gcount());
}

//***************************************************************************
// @brief 열려있는 파일 스트림을 닫고 상태를 초기화한다.
//***************************************************************************
void CFileStream::Close()
{
	if( _file.is_open() )
		_file.close();

	_file.clear(); // eofbit/failbit 등 상태 플래그 초기화(재사용 대비)
	_fileSize = 0;
}

//***************************************************************************
// @brief EOF가 아닌 실제 스트림 I/O 오류(badbit) 발생 여부를 반환한다.
// @return badbit가 설정되어 있으면 true, 그렇지 않으면 false.
//***************************************************************************
bool CFileStream::HasError() const
{
	return _file.bad();
}