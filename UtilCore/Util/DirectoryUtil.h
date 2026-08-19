
//***************************************************************************
// DirectoryUtil.h : interface for the DirectoryUtil Functions.
//
//***************************************************************************

#ifndef __DIRECTORYUTIL_H__
#define __DIRECTORYUTIL_H__

#ifndef __BASEREDEFINEDATATYPE_H__
#include <BaseRedefineDataType.h>
#endif

#include <filesystem>

//***************************************************************************
// @brief 파일 복사/이동 시 필터링 조건을 관리하는 구조체
//***************************************************************************
struct SH_APPLY_FILEINFO
{
	int      m_nFilterMode = 0;		// 확장자 필터 모드 (0: 필터링 없음, 1: 화이트리스트/허용, 2: 블랙리스트/제외)
	_tstring m_tszApplyExt;			// 필터링할 확장자 목록 (예: "txt" 또는 "txt;log;csv")

	// 날짜 필터 범위 (YYYYMMDD 형식, 예: "20260101"). 둘 다 비어 있으면 날짜 필터를 적용하지 않는다.
	// Windows: 생성일 또는 수정일 중 하나라도 이 범위 안에 들면 허용.
	// 그 외 플랫폼: 수정일만 기준으로 판정(생성일/birth time은 POSIX 표준에서 보장되지 않으므로 사용하지 않음).
	_tstring m_tszModifyStDate;		// 날짜 필터 시작일
	_tstring m_tszModifyEdDate;		// 날짜 필터 종료일
};

bool IsMatchedExtension(const std::filesystem::path& filePath, const _tstring& extFilter);
bool IsAbleFile(const std::filesystem::path& sourceFullPath, const SH_APPLY_FILEINFO& shApplyFileInfo);

bool IsDirectory(const std::filesystem::path& folder);
bool CreateDirectoryRecursive(const std::filesystem::path& folder);
bool RemoveDirectoryRecursive(const std::filesystem::path& folder, bool bSelfDel = true);
bool CopyFileRecursive(const std::filesystem::path& sourceFolder, const std::filesystem::path& destFolder, const SH_APPLY_FILEINFO& shApplyFileInfo);
bool MoveFileRecursive(const std::filesystem::path& sourceFolder, const std::filesystem::path& destFolder, const SH_APPLY_FILEINFO& shApplyFileInfo);

#endif // __DIRECTORYUTIL_H__