TITLE BoardInfo64.asm

EXTERN GetSystemFirmwareTable : PROC
EXTERN LocalAlloc : PROC
EXTERN LocalFree : PROC

.code

; [수정] 'RSMB' 멀티 문자 리터럴을 C/C++ 컴파일러가 인코딩하는 방식은
; 첫 글자가 최상위 바이트('R'<<24 | 'S'<<16 | 'M'<<8 | 'B' = 0x52534D42)인데,
; 기존 값(424D5352h)은 바이트 순서가 거꾸로("BMSR")여서 GetSystemFirmwareTable이
; provider signature를 인식하지 못해 항상 0을 반환하고 있었습니다.
RSMB_SIGNATURE EQU 52534D42h    ; 'RSMB'

;***************************************************************************
; @brief   SMBIOS 데이터 구조체 내부에서 N번째 널 종단 문자열의 포인터를 반환합니다 (x64).
; @param   RCX - header_ptr
; @param   RDX - str_index (1-based)
; @return  RAX - 문자열 시작 메모리 주소 (실패 시 0)
;***************************************************************************
dmi_string_64 PROC
    push rbx

    test dl, dl
    jz str_not_found

    mov rax, rcx
    movzx rbx, BYTE PTR [rcx+1] ; header->length
    add rax, rbx                ; RAX = header_ptr + header->length (문자열 영역 시작)

    mov bl, dl                  ; BL = target str_index

str_loop:
    cmp bl, 1
    je str_found

str_skip:
    cmp BYTE PTR [rax], 0
    jz str_next_index
    inc rax
    jmp str_skip

str_next_index:
    inc rax                     ; Null 종단문자 다음 문자로 이동
    dec bl
    jmp str_loop

str_found:
    pop rbx
    ret

str_not_found:
    xor rax, rax
    pop rbx
    ret
dmi_string_64 ENDP


;***************************************************************************
; @brief   SMBIOS 전체 버퍼를 가져와 지정된 Type 및 Offset의 문자열을 복사합니다 (x64).
; @param   RCX - target_type
; @param   RDX - str_idx_offset
; @param   R8  - out_buffer
; @param   R9  - buffer_size
;***************************************************************************
get_smbios_string_64 PROC
    push rbp
    mov rbp, rsp
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    ; [수정] push rbp 이후 7개 레지스터(rbx,rsi,rdi,r12~r15)를 push하면
    ; [rbp-8]부터 [rbp-56]까지는 이미 그 레지스터들의 저장 슬롯으로
    ; 점유되어 있습니다. 그런데 기존 코드는 지역변수 3개(Size/버퍼 포인터/
    ; 끝 주소)를 바로 그 [rbp-8]/[rbp-16]/[rbp-24]에 덮어써서, 함수
    ; 종료 시 "pop rdi/rsi/rbx"가 원래 값이 아니라 지역변수 값을 그대로
    ; 복원해버리는 버그가 있었습니다(rsi의 경우 이미 LocalFree로 해제된
    ; SMBIOS 버퍼를 가리키는 값으로 덮어써짐). Windows x64 호출 규약에서
    ; rbx/rsi/rdi는 비휘발성 레지스터라, 이 함수를 호출할 때마다 호출자의
    ; 해당 레지스터가 조용히 훼손되고 있었습니다.
    ;
    ; 지역변수를 레지스터 저장 슬롯([rbp-8]~[rbp-56])과 겹치지 않는
    ; 안전한 영역으로 옮깁니다. 예약 공간도 "지역변수 24바이트 + 이 함수가
    ; 호출하는 API를 위한 섀도우 스페이스 32바이트 = 56바이트"로 맞춰
    ; sub rsp, 40 -> sub rsp, 56 으로 늘렸습니다(7*8=56바이트 push 이후이므로
    ; 56+56=112, 16의 배수라 스택 정렬도 유지됩니다).
    ;
    ; [rbp-64] : SMBIOS 크기
    ; [rbp-72] : SMBIOS 버퍼 포인터
    ; [rbp-80] : TableData 끝 주소
    sub rsp, 56

    mov r12d, ecx           ; r12d = target_type
    mov r13d, edx           ; r13d = str_idx_offset
    mov r14, r8             ; r14  = out_buffer
    mov r15d, r9d           ; r15d = buffer_size

    test r14, r14
    jz fn_fail
    test r15d, r15d
    jz fn_fail

    ; 1. SMBIOS 크기 구하기
    mov ecx, RSMB_SIGNATURE
    xor edx, edx
    xor r8, r8
    xor r9d, r9d
    call GetSystemFirmwareTable

    test eax, eax
    jz fn_fail
    mov [rbp-64], rax       ; Size 저장

    ; 2. 동적 메모리 할당 (LMEM_FIXED = 0)
    xor ecx, ecx
    mov rdx, [rbp-64]
    call LocalAlloc

    test rax, rax
    jz fn_fail
    mov [rbp-72], rax       ; Buffer 주소 저장

    ; 3. SMBIOS Raw Data 수집
    mov ecx, RSMB_SIGNATURE
    xor edx, edx
    mov r8, [rbp-72]
    mov r9, [rbp-64]
    call GetSystemFirmwareTable

    test eax, eax
    jz free_and_fail

    ; 4. SMBIOS 테이블 탐색 파라미터 설정
    mov rax, [rbp-72]
    mov ecx, [rax+4]        ; TableData Length
    add rax, 8              ; SMBIOSTableData 주소
    mov rsi, rax            ; rsi = 현재 탐색 주소
    add rcx, rax
    mov [rbp-80], rcx       ; [rbp-80] = 끝 다음 주소

scan_table_loop:
    mov rdx, [rbp-80]
    sub rdx, 4
    cmp rsi, rdx
    jae free_and_fail

    movzx eax, BYTE PTR [rsi]   ; header->type
    cmp eax, r12d
    jne skip_structure

    movzx eax, BYTE PTR [rsi+1] ; header->length
    cmp eax, r13d
    jbe skip_structure

    ; Type & Offset 조건 만족 -> str_index 추출
    mov rdx, r13
    movzx rdx, BYTE PTR [rsi+rdx] ; str_index
    test dl, dl
    jz skip_structure           ; 인덱스가 0이면 스킵 후 다음 구조체 탐색

    mov rcx, rsi
    call dmi_string_64

    test rax, rax
    jz skip_structure

    ; 5. 문자열 데이터 복사 (RAX -> R14)
    mov rsi, rax
    mov rdi, r14
    mov ecx, r15d
    dec ecx                     ; Null 종료 공간 확보

copy_loop:
    test ecx, ecx
    jz copy_done
    mov al, BYTE PTR [rsi]
    test al, al
    jz copy_done
    mov BYTE PTR [rdi], al
    inc rsi
    inc rdi
    dec ecx
    jmp copy_loop

copy_done:
    mov BYTE PTR [rdi], 0       ; Null Termination

    mov rcx, [rbp-72]
    call LocalFree
    mov rax, 1                  ; 성공 반환 (1)
    jmp fn_exit

skip_structure:
    movzx rax, BYTE PTR [rsi+1]
    add rsi, rax

find_marker:
    mov rdx, [rbp-80]
    dec rdx
    cmp rsi, rdx
    jae free_and_fail

    cmp BYTE PTR [rsi], 0
    jne find_marker_next
    cmp BYTE PTR [rsi+1], 0
    je found_marker

find_marker_next:
    inc rsi
    jmp find_marker

found_marker:
    add rsi, 2                  ; 0x0000 패딩 건너뛰기
    jmp scan_table_loop

free_and_fail:
    mov rcx, [rbp-72]
    call LocalFree

fn_fail:
    xor rax, rax

fn_exit:
    add rsp, 56
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
get_smbios_string_64 ENDP


;***************************************************************************
; Public C-Interface 파서 바인딩 (윈도우 x64 호출 규약 준수 완료)
; C++ 호출 예시: func(buffer, buffer_size)
; 입력: RCX = buffer, RDX = buffer_size
;***************************************************************************

get_board_manufacturer PROC
    sub rsp, 40
    mov r8, rcx             ; R8  = out_buffer (C++ RCX)
    mov r9d, edx            ; R9D = buffer_size (C++ RDX)
    mov ecx, 2              ; ECX = Type 2 (Base Board)
    mov edx, 4              ; EDX = Offset 4 (Manufacturer)
    call get_smbios_string_64
    add rsp, 40
    ret
get_board_manufacturer ENDP

get_board_product_name PROC
    sub rsp, 40
    mov r8, rcx
    mov r9d, edx
    mov ecx, 2              ; Type 2
    mov edx, 5              ; Offset 5 (Product Name)
    call get_smbios_string_64
    add rsp, 40
    ret
get_board_product_name ENDP

get_board_serial_number PROC
    sub rsp, 40
    mov r8, rcx
    mov r9d, edx
    mov ecx, 2              ; Type 2
    mov edx, 7              ; Offset 7 (Serial Number)
    call get_smbios_string_64
    add rsp, 40
    ret
get_board_serial_number ENDP

get_bios_vendor PROC
    sub rsp, 40
    mov r8, rcx
    mov r9d, edx
    mov ecx, 0              ; Type 0 (BIOS)
    mov edx, 4              ; Offset 4 (Vendor)
    call get_smbios_string_64
    add rsp, 40
    ret
get_bios_vendor ENDP

get_bios_version PROC
    sub rsp, 40
    mov r8, rcx
    mov r9d, edx
    mov ecx, 0              ; Type 0
    mov edx, 5              ; Offset 5 (Version)
    call get_smbios_string_64
    add rsp, 40
    ret
get_bios_version ENDP

get_bios_release_date PROC
    sub rsp, 40
    mov r8, rcx
    mov r9d, edx
    mov ecx, 0              ; Type 0
    mov edx, 8              ; Offset 8 (Release Date)
    call get_smbios_string_64
    add rsp, 40
    ret
get_bios_release_date ENDP

END