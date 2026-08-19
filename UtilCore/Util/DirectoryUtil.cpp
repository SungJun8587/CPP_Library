
//***************************************************************************
// DirectoryUtil.cpp : implementation of the DirectoryUtil functions.
//
//***************************************************************************

#include "pch.h"
#include "DirectoryUtil.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cwctype>
#include <ctime>
#include <type_traits>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

namespace
{
	// 대소문자 무시 비교를 위한 소문자 변환 헬퍼 (_tstring이 string/wstring 어느 쪽이든 동작)
	_tstring ToLower(const _tstring& s)
	{
		_tstring out = s;
		std::transform(out.begin(), out.end(), out.begin(),
			[](_tstring::value_type c) -> _tstring::value_type
			{
				if constexpr( std::is_same_v<_tstring::value_type, wchar_t> )
					return static_cast<wchar_t>(std::towlower(c));
				else
					return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			});
		return out;
	}

	// fs::path의 확장자(점 포함, 예: ".txt")를 _tstring으로 변환
	_tstring PathExtensionToTString(const fs::path& p)
	{
#ifdef UNICODE
		return p.extension().wstring();
#else
		return p.extension().string();
#endif
	}

	// time_t를 로컬 타임존 기준 YYYYMMDD 정수로 변환
	int TimeTToYmd(std::time_t t)
	{
		std::tm tmVal{};
#if defined(_WIN32)
		localtime_s(&tmVal, &t);
#else
		localtime_r(&t, &tmVal);
#endif
		return (tmVal.tm_year + 1900) * 10000 + (tmVal.tm_mon + 1) * 100 + tmVal.tm_mday;
	}

#if defined(_WIN32)
	int FileTimeToYmd(const FILETIME& ft)
	{
		SYSTEMTIME utc{}, local{};
		FileTimeToSystemTime(&ft, &utc);
		SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local);
		return local.wYear * 10000 + local.wMonth * 100 + local.wDay;
	}
#endif

	// 파일의 날짜 정보를 YYYYMMDD 정수로 조회한다.
	// - Windows: 생성일/수정일을 모두 조회한다.
	// - 그 외 플랫폼: 수정일만 조회한다(createdYmd는 사용하지 않으므로 채우지 않음).
	// 실패 시 false.
	bool GetFileDates(const fs::path& path, int& createdYmd, int& modifiedYmd)
	{
#if defined(_WIN32)
		WIN32_FILE_ATTRIBUTE_DATA fad{};
		// fs::path::c_str()는 Windows에서 항상 wchar_t*(native 인코딩)이므로 W 버전을 직접 호출한다.
		if( !GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad) )
			return false;

		createdYmd = FileTimeToYmd(fad.ftCreationTime);
		modifiedYmd = FileTimeToYmd(fad.ftLastWriteTime);
		return true;
#else
		struct stat st {};
		// fs::path::c_str()는 POSIX에서 char*(native 인코딩)이므로 stat()에 그대로 전달 가능
		if( stat(path.c_str(), &st) != 0 )
			return false;

		modifiedYmd = TimeTToYmd(st.st_mtime);
		createdYmd = 0; // 사용하지 않음: POSIX는 생성일(birth time)을 표준으로 보장하지 않음
		return true;
#endif
	}

	// ymd가 [startDate, endDate] 범위(YYYYMMDD, 양끝 포함) 안에 있는지 확인.
	// 각 경계는 비어 있으면 해당 방향은 무제한으로 취급하고, 파싱 불가능한 값은 무시한다.
	bool IsWithinDateRange(int ymd, const _tstring& startDate, const _tstring& endDate)
	{
		try
		{
			if( !startDate.empty() && ymd < std::stoi(startDate) )
				return false;
		}
		catch( const std::exception& ) { /* 잘못된 날짜 문자열은 경계 없음으로 취급 */ }

		try
		{
			if( !endDate.empty() && ymd > std::stoi(endDate) )
				return false;
		}
		catch( const std::exception& ) { /* 잘못된 날짜 문자열은 경계 없음으로 취급 */ }

		return true;
	}
}

//***************************************************************************
// @brief 파일 확장자가 필터 조건(멀티 확장자 포함)에 일치하는지 확인하는 함수
// @param filePath 검사할 파일의 전체 경로
// @param extFilter 구분자(;)로 분리된 확장자 필터 문자열 (예: "txt;log;csv")
// @return 필터 목록에 존재하면 true, 없으면 false
//***************************************************************************
bool IsMatchedExtension(const fs::path& filePath, const _tstring& extFilter)
{
	if( extFilter.empty() )
		return false;

	// 전체 허용 와일드카드 처리
	const _tstring lowerFilter = ToLower(extFilter);
	if( lowerFilter == _T("*") || lowerFilter == _T("*.*") )
		return true;

	// 경로에서 확장자 추출 (점 제거)
	_tstring ext = PathExtensionToTString(filePath);
	if( !ext.empty() && ext.front() == _T('.') )
		ext = ext.substr(1);
	ext = ToLower(ext);

	// 세미콜론(;)으로 구분된 확장자 목록을 직접 순회 비교
	size_t start = 0;
	while( true )
	{
		const size_t sep = extFilter.find(_T(';'), start);
		_tstring item = (sep == _tstring::npos) ? extFilter.substr(start) : extFilter.substr(start, sep - start);

		const size_t nonSpaceStart = item.find_first_not_of(_T(" \t"));
		if( nonSpaceStart != _tstring::npos )
		{
			const size_t nonSpaceEnd = item.find_last_not_of(_T(" \t"));
			item = item.substr(nonSpaceStart, nonSpaceEnd - nonSpaceStart + 1);

			if( !item.empty() && item.front() == _T('.') )
				item = item.substr(1);

			if( ToLower(item) == ext )
				return true; // 일치함 발견
		}

		if( sep == _tstring::npos )
			break;
		start = sep + 1;
	}

	return false; // 일치함 없음
}

//***************************************************************************
// @brief 파일이 필터 정책(확장자 화이트/블랙리스트 + 날짜 범위)에 적합한지 최종 판정하는 함수
// @param sourceFullPath 검사할 파일의 전체 경로
// @param shApplyFileInfo 파일 필터링 옵션 구조체
//                        * m_nFilterMode 의미:
//                          - 0 : 확장자 필터링 없음 (전체 허용)
//                          - 1 : 화이트리스트 (지정한 확장자만 허용)
//                          - 2 : 블랙리스트 (지정한 확장자는 비허용/제외)
//                        * m_tszModifyStDate/m_tszModifyEdDate :
//                          - Windows: 생성일 또는 수정일 중 하나라도 범위 안에 들면 허용
//                          - 그 외 플랫폼: 수정일만 범위 판정에 사용
//                          (둘 다 비어 있으면 날짜 필터 자체를 적용하지 않음)
// @return 허용 대상이면 true, 제외 대상이면 false
//***************************************************************************
bool IsAbleFile(const fs::path& sourceFullPath, const SH_APPLY_FILEINFO& shApplyFileInfo)
{
	// 1. 확장자 필터 판정
	if( shApplyFileInfo.m_nFilterMode != 0 )
	{
		const bool bIsMatched = IsMatchedExtension(sourceFullPath, shApplyFileInfo.m_tszApplyExt);

		// 1번 모드(화이트리스트): 목록에 있어야만 허용 / 2번 모드(블랙리스트): 목록에 있으면 차단
		const bool bExtOk = (shApplyFileInfo.m_nFilterMode == 1) ? bIsMatched : !bIsMatched;
		if( !bExtOk )
			return false;
	}

	// 2. 날짜 필터 판정 (시작일/종료일이 모두 비어 있으면 적용하지 않음)
	if( shApplyFileInfo.m_tszModifyStDate.empty() && shApplyFileInfo.m_tszModifyEdDate.empty() )
		return true;

	int createdYmd = 0, modifiedYmd = 0;
	if( !GetFileDates(sourceFullPath, createdYmd, modifiedYmd) )
		return true; // 날짜 조회 실패(파일 소멸/권한 등) 시에는 걸러내지 않고 통과시킴

#if defined(_WIN32)
	const bool bModifiedInRange = IsWithinDateRange(modifiedYmd, shApplyFileInfo.m_tszModifyStDate, shApplyFileInfo.m_tszModifyEdDate);
	const bool bCreatedInRange = IsWithinDateRange(createdYmd, shApplyFileInfo.m_tszModifyStDate, shApplyFileInfo.m_tszModifyEdDate);
	return bModifiedInRange || bCreatedInRange;
#else
	return IsWithinDateRange(modifiedYmd, shApplyFileInfo.m_tszModifyStDate, shApplyFileInfo.m_tszModifyEdDate);
#endif
}

//***************************************************************************
// @brief 지정한 경로가 유효한 디렉토리인지 확인하는 함수
//***************************************************************************
bool IsDirectory(const fs::path& folder)
{
	std::error_code ec;
	return fs::is_directory(folder, ec);
}

//***************************************************************************
// @brief 지정한 폴더 경로를 하위 폴더까지 재귀적으로 생성하는 함수
// @return 성공(이미 존재하는 경우 포함) 시 true, 실패 시 false
//***************************************************************************
bool CreateDirectoryRecursive(const fs::path& folder)
{
	if( folder.empty() )
		return false;

	std::error_code ec;
	fs::create_directories(folder, ec);
	// create_directories()는 이미 폴더가 존재해 "새로 만든 게 없으면" false를 반환하지만
	// 그 경우 ec는 설정되지 않으므로, 실패 여부는 ec로만 판단한다.
	return !ec;
}

//***************************************************************************
// @brief 지정한 디렉토리와 그 하위의 모든 파일 및 폴더를 재귀적으로 삭제하는 함수
// @param bSelfDel 최상위 폴더 자체까지 삭제할 것인지 여부
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool RemoveDirectoryRecursive(const fs::path& folder, bool bSelfDel)
{
	std::error_code ec;
	if( !fs::exists(folder, ec) )
		return false;

	if( bSelfDel )
	{
		fs::remove_all(folder, ec);
		return !ec;
	}

	// 최상위 폴더 자체는 남기고 그 안의 항목들만 삭제
	// (range-for는 내부적으로 예외를 던지는 operator++()를 쓰므로, 순회 중 오류가 나면
	//  처리되지 않은 예외로 튈 수 있어 error_code 기반 while 루프를 직접 사용한다)
	std::error_code iterEc;
	fs::directory_iterator it(folder, fs::directory_options::skip_permission_denied, iterEc);
	fs::directory_iterator end;
	if( iterEc )
		return false;

	while( it != end )
	{
		std::error_code removeEc;
		fs::remove_all(it->path(), removeEc);
		if( removeEc )
			return false;

		it.increment(iterEc);
		if( iterEc )
		{
			std::fprintf(stderr, "[경고] 폴더 열람 중 오류로 나머지 항목을 건너뜀: %s (오류: %s)\n",
				folder.string().c_str(), iterEc.message().c_str());
			return false;
		}
	}

	return true;
}

//***************************************************************************
// @brief 원본 폴더의 파일 및 하위 폴더들을 대상 폴더로 재귀적으로 복사하는 함수
// @return 성공 시 true, 실패 시 false
//***************************************************************************
bool CopyFileRecursive(const fs::path& sourceFolder, const fs::path& destFolder, const SH_APPLY_FILEINFO& shApplyFileInfo)
{
	if( sourceFolder.empty() || destFolder.empty() )
		return false;

	std::error_code ec;
	if( !fs::exists(sourceFolder, ec) )
		return false;

	fs::create_directories(destFolder, ec);

	fs::directory_iterator it(sourceFolder, fs::directory_options::skip_permission_denied, ec);
	fs::directory_iterator end;
	if( ec )
		return false;

	while( it != end )
	{
		const fs::path& srcPath = it->path();
		const fs::path destPath = destFolder / srcPath.filename();

		std::error_code typeEc;
		bool bIsDir = it->is_directory(typeEc);

		if( typeEc )
		{
			// 상태 조회 자체가 실패한 항목 — 예전엔 로그도 없이 그냥 건너뛰어서
			// 이 경로로 빠진 파일이 있어도 알 방법이 없었다.
			std::fprintf(stderr, "[경고] 상태 조회 실패로 건너뜀: %s (오류: %s)\n",
				srcPath.string().c_str(), typeEc.message().c_str());
			return false;
		}

		if( bIsDir )
		{
			// 하위 디렉토리인 경우 대상 측에도 폴더 생성 후 재귀 호출
			if( !CopyFileRecursive(srcPath, destPath, shApplyFileInfo) )
				return false;
		}
		else
		{
			// 파일인 경우 필터 조건을 만족할 때만 복사 수행
			if( IsAbleFile(srcPath, shApplyFileInfo) )
			{
				std::error_code copyEc;
				fs::copy_file(srcPath, destPath, fs::copy_options::overwrite_existing, copyEc);
				if( copyEc )
					return false;
			}
		}

		// increment 직후 바로 오류를 확인해야 한다 — 표준 규격상 실패 시 it이 곧바로 end가 되어
		// 버려서, for(...; it.increment(ec))처럼 루프 조건에서 확인하면 이 오류를 절대 못 잡는다
		// (그 상태로 그냥 정상 종료된 것처럼 return true;까지 흘러가 버린다).
		it.increment(ec);
		if( ec )
		{
			std::fprintf(stderr, "[경고] 폴더 열람 중 오류로 나머지 항목을 건너뜀: %s (오류: %s)\n",
				sourceFolder.string().c_str(), ec.message().c_str());
			return false;
		}
	}

	return true;
}

//***************************************************************************
// @brief 원본 폴더의 파일 및 하위 폴더들을 대상 폴더로 재귀적으로 이동하는 함수
// @return 성공 시 true, 실패 시 false
// @note 하위 디렉토리도 진짜로 "이동"한다(파일 삭제까지 포함). 원본 구현은 하위
//       디렉토리에 대해 CopyFileRecursive만 호출하고 원본 파일을 지우지 않아
//       최상위 폴더만 이동되고 하위 폴더 내용은 복사된 채 원본이 남는 문제가 있었다.
//***************************************************************************
bool MoveFileRecursive(const fs::path& sourceFolder, const fs::path& destFolder, const SH_APPLY_FILEINFO& shApplyFileInfo)
{
	if( sourceFolder.empty() || destFolder.empty() )
		return false;

	std::error_code ec;
	if( !fs::exists(sourceFolder, ec) )
		return false;

	fs::create_directories(destFolder, ec);

	fs::directory_iterator it(sourceFolder, fs::directory_options::skip_permission_denied, ec);
	fs::directory_iterator end;
	if( ec )
		return false;

	while( it != end )
	{
		const fs::path& srcPath = it->path();
		const fs::path destPath = destFolder / srcPath.filename();

		std::error_code typeEc;
		bool bIsDir = it->is_directory(typeEc);

		if( typeEc )
		{
			std::fprintf(stderr, "[경고] 상태 조회 실패로 건너뜀: %s (오류: %s)\n",
				srcPath.string().c_str(), typeEc.message().c_str());
			return false;
		}

		if( bIsDir )
		{
			// 하위 디렉토리도 재귀적으로 진짜 이동
			if( !MoveFileRecursive(srcPath, destPath, shApplyFileInfo) )
				return false;
		}
		else
		{
			if( IsAbleFile(srcPath, shApplyFileInfo) )
			{
				std::error_code moveEc;
				fs::rename(srcPath, destPath, moveEc);

				if( moveEc )
				{
					// 서로 다른 볼륨/드라이브 간 이동 등으로 rename 실패 시 copy+remove로 폴백
					std::error_code copyEc;
					fs::copy_file(srcPath, destPath, fs::copy_options::overwrite_existing, copyEc);
					if( copyEc )
						return false;

					std::error_code removeEc;
					fs::remove(srcPath, removeEc);
					if( removeEc )
						return false;
				}
			}
		}

		// increment 직후 바로 오류를 확인해야 한다 (이유는 CopyFileRecursive 주석 참고)
		it.increment(ec);
		if( ec )
		{
			std::fprintf(stderr, "[경고] 폴더 열람 중 오류로 나머지 항목을 건너뜀: %s (오류: %s)\n",
				sourceFolder.string().c_str(), ec.message().c_str());
			return false;
		}
	}

	return true;
}