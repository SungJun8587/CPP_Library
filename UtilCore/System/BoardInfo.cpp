
//***************************************************************************
// BoardInfo.cpp: Windows SMBIOS 테이블 기반 메인보드 및 BIOS 정보 취득 구현부
//
//***************************************************************************

#include "pch.h"
#include "BoardInfo.h"

#include <windows.h>
#include <cstring>
#include <cstdlib>

#pragma pack(push, 1)
struct RawSMBIOSData {
    BYTE  Used20CallingMethod;
    BYTE  MajorVersion;
    BYTE  MinorVersion;
    BYTE  DmiRevision;
    DWORD Length;
    BYTE  SMBIOSTableData[1];
};

struct dmi_header {
    BYTE type;
    BYTE length;
    WORD handle;
};
#pragma pack(pop)

//***************************************************************************
// @brief   SMBIOS 테이블에서 지정된 번호의 N번째 문자열 포인터를 추출합니다.
// @param   header [in] dmi_header 구조체 포인터
// @param   index  [in] 가져올 문자열 인덱스 (1부터 시작)
// @return  const char* - 문자열 포인터 (실패 시 빈 문자열 "")
// @detail  SMBIOS 헤더 직후의 널 문자 종료 텍스트 블록을 스캔하여 인덱스에 일치하는 텍스트 위치를 탐색합니다.
//***************************************************************************
static const char* dmi_string(const struct dmi_header* header, BYTE index) {
    if( index == 0 ) return "";

    const char* str = reinterpret_cast<const char*>(header) + header->length;
    while( index > 1 && *str ) {
        str += strlen(str) + 1;
        index--;
    }
    return *str ? str : "";
}

//***************************************************************************
// @brief   SMBIOS Raw Table Data 버퍼 전체를 로드합니다.
// @param   buffer_size [out] 할당 및 반환된 버퍼의 바이트 크기 수신 포인터
// @return  RawSMBIOSData* - 동적 할당된 SMBIOS 데이터 포인터 (사용 후 free 필요)
// @detail  GetSystemFirmwareTable API를 호출하여 'RSMB' 펌웨어 테이블 영역을 가져옵니다.
//***************************************************************************
static RawSMBIOSData* get_smbios_buffer(DWORD* buffer_size) {
    DWORD size = GetSystemFirmwareTable('RSMB', 0, NULL, 0);
    if( size == 0 ) return nullptr;

    RawSMBIOSData* buffer = reinterpret_cast<RawSMBIOSData*>(malloc(size));
    if( !buffer ) return nullptr;

    if( GetSystemFirmwareTable('RSMB', 0, buffer, size) == 0 ) {
        free(buffer);
        return nullptr;
    }

    if( buffer_size ) *buffer_size = size;
    return buffer;
}

//***************************************************************************
// @brief   지정된 SMBIOS 구조체 타입(Type) 영역을 탐색하여 문자열 항목을 안전하게 복사합니다.
// @param   type        [in] 찾으려는 SMBIOS 테이블 타입 (0: BIOS, 2: Board)
// @param   str_index_offset [in] 테이블 구조체 내부의 문자열 인덱스 값 오프셋
// @param   buffer      [out] 문자열을 담을 출력 버퍼
// @param   buffer_size [in] 출력 버퍼의 크기
// @return  int - 1: 성공, 0: 실패
// @detail  전체 SMBIOS 버퍼 순회를 거쳐 대상 구조체 타입을 탐색 후 dmi_string을 통해 텍스트를 파싱합니다.
//***************************************************************************
static int get_smbios_field_string(BYTE type, BYTE str_index_offset, char* buffer, unsigned int buffer_size) {
    if( !buffer || buffer_size == 0 ) return 0;

    DWORD size = 0;
    RawSMBIOSData* smbios = get_smbios_buffer(&size);
    if( !smbios ) return 0;

    int result = 0;
    BYTE* data = smbios->SMBIOSTableData;
    BYTE* end = data + smbios->Length;

    while( data + sizeof(dmi_header) <= end ) {
        dmi_header* header = reinterpret_cast<dmi_header*>(data);

        if( header->type == type && (header->length > str_index_offset) ) {
            BYTE str_index = data[str_index_offset];
            const char* parsed_str = dmi_string(header, str_index);

            strncpy_s(buffer, buffer_size, parsed_str, _TRUNCATE);
            result = 1;
            break;
        }

        data += header->length;
        while( data < end - 1 && !(data[0] == 0 && data[1] == 0) ) {
            data++;
        }
        data += 2;
    }

    free(smbios);
    return result;
}