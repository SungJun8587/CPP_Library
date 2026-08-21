
//***************************************************************************
// BoardInfo.h: Motherboard & BIOS Information Library
//
//***************************************************************************

#ifndef __BOARDINFO_H__
#define __BOARDINFO_H__

extern "C" {
    //***************************************************************************
    // @brief   메인보드 제조사(Manufacturer) 이름을 가져옵니다.
    // @param   buffer      [out] 문자열을 저장할 버퍼 포인터
    // @param   buffer_size [in] 버퍼의 바이트 크기
    // @return  int - 1: 성공, 0: 실패
    // @detail  GetSystemFirmwareTable을 통해 SMBIOS Type 2(Baseboard Information) 테이블의 제조사 문자열을 추출합니다.
    //***************************************************************************
    int get_board_manufacturer(char* buffer, unsigned int buffer_size);

    //***************************************************************************
    // @brief   메인보드 모델명(Product Name)을 가져옵니다.
    // @param   buffer      [out] 문자열을 저장할 버퍼 포인터
    // @param   buffer_size [in] 버퍼의 바이트 크기
    // @return  int - 1: 성공, 0: 실패
    // @detail  SMBIOS Type 2 테이블 파싱을 통해 메인보드 제품 모델명을 버퍼에 복사합니다.
    //***************************************************************************
    int get_board_product_name(char* buffer, unsigned int buffer_size);

    //***************************************************************************
    // @brief   메인보드 시리얼 번호(Serial Number)를 가져옵니다.
    // @param   buffer      [out] 문자열을 저장할 버퍼 포인터
    // @param   buffer_size [in] 버퍼의 바이트 크기
    // @return  int - 1: 성공, 0: 실패
    // @detail  SMBIOS Type 2 테이블의 시리얼 번호 오프셋 데이터를 파싱합니다.
    //***************************************************************************
    int get_board_serial_number(char* buffer, unsigned int buffer_size);

    //***************************************************************************
    // @brief   BIOS 제조사(Vendor) 이름을 가져옵니다.
    // @param   buffer      [out] 문자열을 저장할 버퍼 포인터
    // @param   buffer_size [in] 버퍼의 바이트 크기
    // @return  int - 1: 성공, 0: 실패
    // @detail  SMBIOS Type 0(BIOS Information) 테이블 파싱을 통해 펌웨어 개발사 이름을 가져옵니다.
    //***************************************************************************
    int get_bios_vendor(char* buffer, unsigned int buffer_size);

    //***************************************************************************
    // @brief   BIOS 버전을 가져옵니다.
    // @param   buffer      [out] 문자열을 저장할 버퍼 포인터
    // @param   buffer_size [in] 버퍼의 바이트 크기
    // @return  int - 1: 성공, 0: 실패
    // @detail  SMBIOS Type 0 테이블에서 현재 탑재된 BIOS 펌웨어 버전 문자열을 읽어옵니다.
    //***************************************************************************
    int get_bios_version(char* buffer, unsigned int buffer_size);

    //***************************************************************************
    // @brief   BIOS 배포/릴리즈 날짜를 가져옵니다.
    // @param   buffer      [out] 문자열을 저장할 버퍼 포인터
    // @param   buffer_size [in] 버퍼의 바이트 크기
    // @return  int - 1: 성공, 0: 실패
    // @detail  SMBIOS Type 0 테이블 파싱을 통해 BIOS 빌드 날짜(MM/DD/YYYY)를 파싱합니다.
    //***************************************************************************
    int get_bios_release_date(char* buffer, unsigned int buffer_size);
}

#endif // __BOARDINFO_H__