.MODEL FLAT, C
.STACK 4096

; Windows API 외장 함수 선언
EXTERN GetSystemFirmwareTable@16 : PROC
EXTERN LocalAlloc@8 : PROC
EXTERN LocalFree@4 : PROC

.CODE

; SMBIOS 테이블 검색 상수 정의
; [수정] 'RSMB' 멀티 문자 리터럴을 C/C++ 컴파일러가 인코딩하는 방식은
; 첫 글자가 최상위 바이트('R'<<24 | 'S'<<16 | 'M'<<8 | 'B' = 0x52534D42)인데,
; 기존 값(424D5352h)은 바이트 순서가 거꾸로("BMSR")여서 GetSystemFirmwareTable이
; provider signature를 인식하지 못해 항상 0을 반환하고 있었습니다.
RSMB_SIGNATURE EQU 52534D42h    ; 'RSMB' (DWORD)


;***************************************************************************
; @brief   SMBIOS 데이터 구조체 내부에서 N번째 널 종단 문자열의 포인터를 반환합니다 (x86 32비트).
; @param   [ESP+4] - header_ptr (dmi_header 포인터)
; @param   [ESP+8] - str_index  (1부터 시작하는 문자열 인덱스)
; @return  EAX - 문자열 시작 메모리 주소 (실패 시 0)
; @detail  구조체 길이(length) 오프셋 뒤에 이어지는 연속된 널 종단 텍스트 블록을 스캔합니다.
;***************************************************************************
dmi_string_32 PROC
    push ebp
    mov ebp, esp
    push ebx
    push esi

    mov esi, [ebp+8]        ; header_ptr
    mov bl, [ebp+12]        ; str_index

    test bl, bl
    jz str_not_found        ; 인덱스가 0이면 문자열 없음

    xor eax, eax
    mov al, [esi+1]         ; header->length
    add esi, eax            ; esi = header 오프셋 직후 (문자열 영역 시작)

str_loop:
    cmp bl, 1
    je str_found            ; 목표 인덱스 도달

str_skip:
    lodsb                   ; AL = [ESI], ESI++
    test al, al
    jnz str_skip            ; 널 문자를 만날 때까지 루프
    dec bl                  ; 다음 문자열 인덱스로 이동
    jmp str_loop

str_found:
    mov eax, esi            ; 찾는 문자열의 시작 주소 반환
    jmp str_exit

str_not_found:
    xor eax, eax

str_exit:
    pop esi
    pop ebx
    pop ebp
    ret
dmi_string_32 ENDP


;***************************************************************************
; @brief   SMBIOS 전체 버퍼를 가져와 지정된 Type 및 Offset의 문자열을 복사합니다 (x86 32비트).
; @param   [ESP+4]  - target_type    (0: BIOS, 2: Board)
; @param   [ESP+8]  - str_idx_offset (구조체 내부 문자열 인덱스 바이트 오프셋)
; @param   [ESP+12] - out_buffer     (출력 문자열 버퍼 포인터)
; @param   [ESP+16] - buffer_size    (버퍼 바이트 크기)
; @return  EAX - 1: 성공, 0: 실패
; @detail  GetSystemFirmwareTable API를 실행하여 RSMB 영역 파싱을 수행합니다.
;***************************************************************************
get_smbios_string_32 PROC
    push ebp
    mov ebp, esp
    sub esp, 16             ; 지역 변수 공간 확보
    push ebx
    push esi
    push edi

    ; [ebp-4]  : 버퍼 크기
    ; [ebp-8]  : SMBIOS 버퍼 포인터
    ; [ebp-12] : TableData 시작 주소
    ; [ebp-16] : TableData 끝 주소

    ; out_buffer/buffer_size 입력 검증. buffer_size가 0이면 아래 copy_loop의
    ; "dec ecx"가 0에서 언더플로되어 사실상 무제한 루프로 이어질 수 있고,
    ; out_buffer가 NULL이면 그대로 역참조되어 크래시로 이어질 수 있습니다.
    cmp DWORD PTR [ebp+16], 0
    jz fn_fail               ; out_buffer == NULL
    cmp DWORD PTR [ebp+20], 0
    jz fn_fail               ; buffer_size == 0

    ; 1. SMBIOS 필요한 크기 조회
    push 0
    push 0
    push 0
    push RSMB_SIGNATURE
    call GetSystemFirmwareTable@16
    
    test eax, eax
    jz fn_fail
    mov [ebp-4], eax        ; 크기 저장

    ; 2. 동적 메모리 할당 (LMEM_FIXED = 0x0000)
    push DWORD PTR [ebp-4]
    push 0
    call LocalAlloc@8
    
    test eax, eax
    jz fn_fail
    mov [ebp-8], eax        ; 버퍼 포인터 저장

    ; 3. SMBIOS Raw Data 가져오기
    push DWORD PTR [ebp-4]
    push DWORD PTR [ebp-8]
    push 0
    push RSMB_SIGNATURE
    call GetSystemFirmwareTable@16

    test eax, eax
    jz free_and_fail

    ; 4. SMBIOS 테이블 데이터 오프셋 설정 (RawSMBIOSData.SMBIOSTableData 오프셋 = 8)
    mov eax, [ebp-8]
    mov ecx, [eax+4]        ; Length
    add eax, 8              ; TableData 포인터
    mov [ebp-12], eax
    add ecx, eax
    mov [ebp-16], ecx       ; 끝 주소

    mov esi, [ebp-12]       ; ESI = 탐색 포인터

scan_table_loop:
    cmp esi, [ebp-16]
    jae free_and_fail       ; 끝 주소 도달 시 실패

    ; dmi_header 검사
    mov al, [esi]           ; header->type
    mov bl, [ebp+8]         ; target_type
    cmp al, bl
    jne skip_structure

    ; header->length가 str_idx_offset보다 작거나 같으면 이 구조체에는 해당
    ; 오프셋의 문자열 인덱스 필드 자체가 존재하지 않습니다. 검사 없이 바로
    ; [esi+offset]을 읽으면 구조체 경계를 넘어선 값을 str_index로 오인할
    ; 수 있어, 여기서 미리 걸러냅니다.
    movzx edx, BYTE PTR [esi+1]  ; header->length
    movzx eax, BYTE PTR [ebp+12] ; str_idx_offset
    cmp edx, eax
    jbe skip_structure

    ; Type 일치 + 유효한 오프셋 -> 원하는 Offset의 문자열 인덱스 추출
    mov bl, [esi+eax]       ; str_index
    
    ; 문자열 포인터 구하기
    push ebx
    push esi
    call dmi_string_32
    add esp, 8

    test eax, eax
    jz free_and_fail

    ; 문자열 복사 (EAX = source_str, EDI = out_buffer)
    mov esi, eax
    mov edi, [ebp+16]       ; out_buffer
    mov ecx, [ebp+20]       ; buffer_size
    dec ecx                 ; Null 종료 보장용 크기 차감

copy_loop:
    test ecx, ecx
    jz copy_done
    lodsb
    test al, al
    jz copy_done
    stosb
    dec ecx
    jmp copy_loop

copy_done:
    mov BYTE PTR [edi], 0   ; Null Terminator 추가
    
    ; 메모리 해제 및 성공 리턴
    push DWORD PTR [ebp-8]
    call LocalFree@4
    mov eax, 1
    jmp fn_exit

skip_structure:
    ; 다음 구조체로 이동 (header->length 만큼 이동 후 00 00 마커 스캔)
    movzx eax, BYTE PTR [esi+1]
    add esi, eax

find_marker:
    cmp esi, [ebp-16]
    jae free_and_fail
    mov ax, [esi]
    test ax, ax             ; WORD 0x0000 인지 확인
    jz found_marker
    inc esi
    jmp find_marker

found_marker:
    add esi, 2              ; 0x0000 끝을 지나 다음 구조체 시작으로
    jmp scan_table_loop

free_and_fail:
    push DWORD PTR [ebp-8]
    call LocalFree@4

fn_fail:
    xor eax, eax

fn_exit:
    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret
get_smbios_string_32 ENDP


;***************************************************************************
; Public C-Interface 파서 바인딩
;***************************************************************************

; 메인보드 제조사 취득 (Type 2, Offset 0x04)
get_board_manufacturer PROC
    push ebp
    mov ebp, esp
    push DWORD PTR [ebp+12] ; buffer_size
    push DWORD PTR [ebp+8]  ; buffer
    push 4                  ; Offset 0x04
    push 2                  ; Type 2
    call get_smbios_string_32
    add esp, 16
    pop ebp
    ret
get_board_manufacturer ENDP

; 메인보드 모델명 취득 (Type 2, Offset 0x05)
get_board_product_name PROC
    push ebp
    mov ebp, esp
    push DWORD PTR [ebp+12]
    push DWORD PTR [ebp+8]
    push 5                  ; Offset 0x05
    push 2                  ; Type 2
    call get_smbios_string_32
    add esp, 16
    pop ebp
    ret
get_board_product_name ENDP

; 메인보드 시리얼 번호 취득 (Type 2, Offset 0x07)
get_board_serial_number PROC
    push ebp
    mov ebp, esp
    push DWORD PTR [ebp+12]
    push DWORD PTR [ebp+8]
    push 7                  ; Offset 0x07
    push 2                  ; Type 2
    call get_smbios_string_32
    add esp, 16
    pop ebp
    ret
get_board_serial_number ENDP

; BIOS 제조사 취득 (Type 0, Offset 0x04)
get_bios_vendor PROC
    push ebp
    mov ebp, esp
    push DWORD PTR [ebp+12]
    push DWORD PTR [ebp+8]
    push 4                  ; Offset 0x04
    push 0                  ; Type 0
    call get_smbios_string_32
    add esp, 16
    pop ebp
    ret
get_bios_vendor ENDP

; BIOS 버전 취득 (Type 0, Offset 0x05)
get_bios_version PROC
    push ebp
    mov ebp, esp
    push DWORD PTR [ebp+12]
    push DWORD PTR [ebp+8]
    push 5                  ; Offset 0x05
    push 0                  ; Type 0
    call get_smbios_string_32
    add esp, 16
    pop ebp
    ret
get_bios_version ENDP

; BIOS 릴리즈 날짜 취득 (Type 0, Offset 0x08)
get_bios_release_date PROC
    push ebp
    mov ebp, esp
    push DWORD PTR [ebp+12]
    push DWORD PTR [ebp+8]
    push 8                  ; Offset 0x08
    push 0                  ; Type 0
    call get_smbios_string_32
    add esp, 16
    pop ebp
    ret
get_bios_release_date ENDP

END