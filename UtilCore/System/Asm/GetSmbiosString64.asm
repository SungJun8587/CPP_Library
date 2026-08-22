TITLE get_smbios_string_64.asm
; SMBIOS 문자열/숫자 필드 리더 (x64). 전부 GetSystemFirmwareTable('RSMB')로 raw 테이블을
; 읽어 Type/offset 기준으로 구조체를 순회하는 동일 패턴을 쓴다.
;
;   get_smbios_instance_count_64(type) -> 해당 Type 구조체 개수
;   get_smbios_string_instance_64(type, offset, instance, buffer, buffer_size) -> 1/0
;       instance(0-based)번째 Type 구조체에서 offset 위치의 문자열 인덱스를 읽어 문자열 복사
;   get_smbios_string_64(type, offset, buffer, buffer_size) -> 1/0
;       위 함수의 instance=0 고정 래퍼 (기존 get_ram_manufacturer/get_ram_locator 호환용)
;   get_smbios_word_64(type, offset, instance, unsigned short* outValue) -> 1/0
;       문자열이 아닌 raw WORD 필드(예: RAM Size/Speed)를 직접 읽음
;
; Kernel32.lib 링크 필요.

EXTERN GetSystemFirmwareTable : PROC
EXTERN GetProcessHeap : PROC
EXTERN HeapAlloc : PROC
EXTERN HeapFree : PROC

.code

;***************************************************************************
; get_smbios_instance_count_64(int type) -> int count
; RCX = type
;***************************************************************************
get_smbios_instance_count_64 PROC
    push rbp
    mov rbp, rsp
    sub rsp, 112

    ; [rbp-8]  type
    ; [rbp-40] heap handle
    ; [rbp-48] raw SMBIOS 데이터 포인터
    ; [rbp-56] GetSystemFirmwareTable 필요 크기(임시)
    ; [rbp-64] 반환값 임시(cleanup용)
    ; [rbp-72] 테이블 데이터 끝 포인터(1회 계산)
    ; [rbp-80] 카운트
    ; [rbp-88] 테이블 데이터 길이(임시)

    mov [rbp-8], rcx

    mov ecx, 052534D42h              ; 'RSMB'
    xor edx, edx
    xor r8, r8
    xor r9d, r9d
    call GetSystemFirmwareTable
    test eax, eax
    jz smbios_cnt_fail_none
    mov DWORD PTR [rbp-56], eax

    call GetProcessHeap
    mov [rbp-40], rax

    mov rcx, [rbp-40]
    xor edx, edx
    mov r8d, DWORD PTR [rbp-56]
    call HeapAlloc
    test rax, rax
    jz smbios_cnt_fail_none
    mov [rbp-48], rax

    mov ecx, 052534D42h
    xor edx, edx
    mov r8, [rbp-48]
    mov r9d, DWORD PTR [rbp-56]
    call GetSystemFirmwareTable
    test eax, eax
    jz smbios_cnt_fail_free

    mov rax, [rbp-48]
    mov edx, DWORD PTR [rax+4]       ; Length
    mov QWORD PTR [rbp-88], rdx

    lea rax, [rax+8]
    mov r10, rax                     ; 현재 구조체 포인터
    add rax, QWORD PTR [rbp-88]
    mov [rbp-72], rax                ; 끝 포인터

    mov DWORD PTR [rbp-80], 0        ; 카운트 초기화

smbios_cnt_scan_loop:
    mov r11, [rbp-72]
    cmp r10, r11
    jae smbios_cnt_done

    movzx eax, BYTE PTR [r10]
    cmp al, 127
    je smbios_cnt_done

    movzx ecx, BYTE PTR [r10+1]      ; FormattedLength

    mov rdx, [rbp-8]
    cmp al, dl
    jne smbios_cnt_skip
    inc DWORD PTR [rbp-80]

smbios_cnt_skip:
    lea r9, [r10+rcx]
smbios_cnt_skip_scan:
    cmp BYTE PTR [r9], 0
    jne smbios_cnt_skip_scan_adv
    cmp BYTE PTR [r9+1], 0
    jne smbios_cnt_skip_scan_adv
    lea r10, [r9+2]
    jmp smbios_cnt_scan_loop
smbios_cnt_skip_scan_adv:
    inc r9
    jmp smbios_cnt_skip_scan

smbios_cnt_done:
    mov eax, DWORD PTR [rbp-80]

smbios_cnt_cleanup:
    mov DWORD PTR [rbp-64], eax
    mov rcx, [rbp-40]
    xor edx, edx
    mov r8, [rbp-48]
    call HeapFree
    mov eax, DWORD PTR [rbp-64]
    jmp smbios_cnt_exit

smbios_cnt_fail_free:
    xor eax, eax
    jmp smbios_cnt_cleanup

smbios_cnt_fail_none:
    xor eax, eax

smbios_cnt_exit:
    add rsp, 112
    pop rbp
    ret
get_smbios_instance_count_64 ENDP


;***************************************************************************
; get_smbios_string_instance_64(int type, int offset, int instance, char* buffer, unsigned int buffer_size) -> int(1/0)
; RCX=type, RDX=offset, R8d=instance(0-based), R9=buffer, [rbp+40]=buffer_size(5번째 인자, 스택)
;***************************************************************************
get_smbios_string_instance_64 PROC
    push rbp
    mov rbp, rsp
    sub rsp, 112

    ; [rbp-8]  type
    ; [rbp-16] offset
    ; [rbp-24] instance(목표)
    ; [rbp-32] out buffer
    ; [rbp-40] heap handle
    ; [rbp-48] raw SMBIOS 데이터 포인터
    ; [rbp-56] 필요 크기(임시)
    ; [rbp-64] 반환값 임시
    ; [rbp-72] 테이블 데이터 끝 포인터
    ; [rbp-80] 지금까지 찾은 매치 수
    ; [rbp-88] 테이블 데이터 길이(임시)
    ; buffer_size는 캐스터의 스택 프레임에 그대로 있으므로 복사하지 않고 [rbp+40]에서 직접 읽음

    mov [rbp-8], rcx
    mov [rbp-16], rdx
    mov DWORD PTR [rbp-24], r8d
    mov [rbp-32], r9

    mov ecx, 052534D42h
    xor edx, edx
    xor r8, r8
    xor r9d, r9d
    call GetSystemFirmwareTable
    test eax, eax
    jz smbios_inst_fail_none
    mov DWORD PTR [rbp-56], eax

    call GetProcessHeap
    mov [rbp-40], rax

    mov rcx, [rbp-40]
    xor edx, edx
    mov r8d, DWORD PTR [rbp-56]
    call HeapAlloc
    test rax, rax
    jz smbios_inst_fail_none
    mov [rbp-48], rax

    mov ecx, 052534D42h
    xor edx, edx
    mov r8, [rbp-48]
    mov r9d, DWORD PTR [rbp-56]
    call GetSystemFirmwareTable
    test eax, eax
    jz smbios_inst_fail_free

    mov rax, [rbp-48]
    mov edx, DWORD PTR [rax+4]
    mov QWORD PTR [rbp-88], rdx

    lea rax, [rax+8]
    mov r10, rax
    add rax, QWORD PTR [rbp-88]
    mov [rbp-72], rax

    mov DWORD PTR [rbp-80], 0

smbios_inst_scan_loop:
    mov r11, [rbp-72]
    cmp r10, r11
    jae smbios_inst_fail_free

    movzx eax, BYTE PTR [r10]
    cmp al, 127
    je smbios_inst_fail_free

    movzx ecx, BYTE PTR [r10+1]       ; FormattedLength

    mov rdx, [rbp-8]
    cmp al, dl
    jne smbios_inst_skip

    mov eax, DWORD PTR [rbp-80]
    cmp eax, DWORD PTR [rbp-24]
    jne smbios_inst_not_target
    jmp smbios_inst_use_this

smbios_inst_not_target:
    inc DWORD PTR [rbp-80]
    jmp smbios_inst_skip

smbios_inst_use_this:
    mov rdx, [rbp-16]                 ; offset
    cmp cl, dl
    jbe smbios_inst_fail_free         ; FormattedLength <= offset : 필드 없음

    movzx eax, BYTE PTR [r10+rdx]     ; 문자열 인덱스(1-based)
    test al, al
    jz smbios_inst_fail_free

    mov r8b, al                       ; 목표 문자열 번호
    lea r9, [r10+rcx]                 ; 문자열 셋 시작
    mov edx, 1

smbios_inst_find_string:
    cmp dl, r8b
    je smbios_inst_string_found
smbios_inst_skip_char:
    cmp BYTE PTR [r9], 0
    je smbios_inst_skip_char_end
    inc r9
    jmp smbios_inst_skip_char
smbios_inst_skip_char_end:
    inc r9
    inc dl
    cmp BYTE PTR [r9], 0
    je smbios_inst_fail_free
    jmp smbios_inst_find_string

smbios_inst_string_found:
    mov r10, [rbp-32]                 ; out buffer (구조체 스캔용 r10은 더 이상 불필요)
    mov r8d, DWORD PTR [rbp+40]       ; out buffer_size (호출자 스택 인자에서 직접 읽음)
    xor ecx, ecx
smbios_inst_copy_loop:
    cmp ecx, r8d
    jae smbios_inst_copy_done
    movzx eax, BYTE PTR [r9+rcx]
    test al, al
    jz smbios_inst_copy_done
    mov BYTE PTR [r10+rcx], al
    inc ecx
    jmp smbios_inst_copy_loop
smbios_inst_copy_done:
    cmp ecx, r8d
    jb smbios_inst_copy_null_ok
    dec ecx
smbios_inst_copy_null_ok:
    mov BYTE PTR [r10+rcx], 0
    mov eax, 1
    jmp smbios_inst_cleanup

smbios_inst_skip:
    lea r9, [r10+rcx]
smbios_inst_skip_scan:
    cmp BYTE PTR [r9], 0
    jne smbios_inst_skip_scan_adv
    cmp BYTE PTR [r9+1], 0
    jne smbios_inst_skip_scan_adv
    lea r10, [r9+2]
    jmp smbios_inst_scan_loop
smbios_inst_skip_scan_adv:
    inc r9
    jmp smbios_inst_skip_scan

smbios_inst_fail_free:
    xor eax, eax
smbios_inst_cleanup:
    mov DWORD PTR [rbp-64], eax
    mov rcx, [rbp-40]
    xor edx, edx
    mov r8, [rbp-48]
    call HeapFree
    mov eax, DWORD PTR [rbp-64]
    jmp smbios_inst_exit

smbios_inst_fail_none:
    xor eax, eax

smbios_inst_exit:
    add rsp, 112
    pop rbp
    ret
get_smbios_string_instance_64 ENDP


;***************************************************************************
; get_smbios_string_64(int type, int offset, char* buffer, unsigned int buffer_size) -> int(1/0)
; RCX=type, RDX=offset, R8=buffer, R9=buffer_size
; get_smbios_string_instance_64(type, offset, 0, buffer, buffer_size) 위임 (기존 호출부 호환용)
;***************************************************************************
get_smbios_string_64 PROC
    sub rsp, 40                        ; shadow space(32) + 5번째 인자 슬롯(8)
    mov QWORD PTR [rsp+32], r9         ; buffer_size -> 5번째 인자 자리
    mov r9, r8                         ; buffer
    xor r8d, r8d                       ; instance = 0
    call get_smbios_string_instance_64
    add rsp, 40
    ret
get_smbios_string_64 ENDP


;***************************************************************************
; get_smbios_word_64(int type, int offset, int instance, unsigned short* outValue) -> int(1/0)
; RCX=type, RDX=offset, R8d=instance(0-based), R9=out_uint16_ptr
;***************************************************************************
get_smbios_word_64 PROC
    push rbp
    mov rbp, rsp
    sub rsp, 112

    mov [rbp-8], rcx
    mov [rbp-16], rdx
    mov DWORD PTR [rbp-24], r8d
    mov [rbp-32], r9

    mov ecx, 052534D42h
    xor edx, edx
    xor r8, r8
    xor r9d, r9d
    call GetSystemFirmwareTable
    test eax, eax
    jz smbios_word_fail_none
    mov DWORD PTR [rbp-56], eax

    call GetProcessHeap
    mov [rbp-40], rax

    mov rcx, [rbp-40]
    xor edx, edx
    mov r8d, DWORD PTR [rbp-56]
    call HeapAlloc
    test rax, rax
    jz smbios_word_fail_none
    mov [rbp-48], rax

    mov ecx, 052534D42h
    xor edx, edx
    mov r8, [rbp-48]
    mov r9d, DWORD PTR [rbp-56]
    call GetSystemFirmwareTable
    test eax, eax
    jz smbios_word_fail_free

    mov rax, [rbp-48]
    mov edx, DWORD PTR [rax+4]
    mov QWORD PTR [rbp-88], rdx

    lea rax, [rax+8]
    mov r10, rax
    add rax, QWORD PTR [rbp-88]
    mov [rbp-72], rax

    mov DWORD PTR [rbp-80], 0

smbios_word_scan_loop:
    mov r11, [rbp-72]
    cmp r10, r11
    jae smbios_word_fail_free

    movzx eax, BYTE PTR [r10]
    cmp al, 127
    je smbios_word_fail_free

    movzx ecx, BYTE PTR [r10+1]

    mov rdx, [rbp-8]
    cmp al, dl
    jne smbios_word_skip

    mov eax, DWORD PTR [rbp-80]
    cmp eax, DWORD PTR [rbp-24]
    jne smbios_word_not_target
    jmp smbios_word_use_this

smbios_word_not_target:
    inc DWORD PTR [rbp-80]
    jmp smbios_word_skip

smbios_word_use_this:
    mov rdx, [rbp-16]                 ; offset
    lea rax, [rdx+2]                  ; WORD 필드이므로 offset+1까지 FormattedLength 안에 있어야 함
    cmp cl, al
    jbe smbios_word_fail_free

    movzx eax, WORD PTR [r10+rdx]     ; raw WORD 값 읽기 (r10 = 구조체 시작 포인터)
    mov r11, [rbp-32]                 ; out ptr
    mov WORD PTR [r11], ax
    mov eax, 1
    jmp smbios_word_cleanup

smbios_word_skip:
    lea r9, [r10+rcx]
smbios_word_skip_scan:
    cmp BYTE PTR [r9], 0
    jne smbios_word_skip_scan_adv
    cmp BYTE PTR [r9+1], 0
    jne smbios_word_skip_scan_adv
    lea r10, [r9+2]
    jmp smbios_word_scan_loop
smbios_word_skip_scan_adv:
    inc r9
    jmp smbios_word_skip_scan

smbios_word_fail_free:
    xor eax, eax
smbios_word_cleanup:
    mov DWORD PTR [rbp-64], eax
    mov rcx, [rbp-40]
    xor edx, edx
    mov r8, [rbp-48]
    call HeapFree
    mov eax, DWORD PTR [rbp-64]
    jmp smbios_word_exit

smbios_word_fail_none:
    xor eax, eax

smbios_word_exit:
    add rsp, 112
    pop rbp
    ret
get_smbios_word_64 ENDP

END
