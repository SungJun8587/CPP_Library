.MODEL FLAT, C
TITLE get_smbios_string_32.asm
; get_smbios_string_32(int type, int offset, char* buffer, unsigned int buffer_size) -> int(1/0)
; cdecl, 인자는 [ebp+8]=type, [ebp+12]=offset, [ebp+16]=buffer, [ebp+20]=buffer_size
;
; x86은 비휘발 레지스터(ebx/esi/edi)가 3개뿐이라 호출 간 유지해야 할 상태가 많으므로
; 전부 스택 지역변수에 보관하고, 레지스터는 각 구간에서 스크래치 용도로만 사용한다.
;
; Kernel32.lib 링크 필요.

EXTERN GetSystemFirmwareTable@16 : PROC
EXTERN GetProcessHeap@0 : PROC
EXTERN HeapAlloc@12 : PROC
EXTERN HeapFree@12 : PROC

.CODE

get_smbios_string_32 PROC
    push ebp
    mov ebp, esp
    sub esp, 48
    push ebx
    push esi
    push edi

    ; 지역 변수 레이아웃
    ; [ebp-4]  type
    ; [ebp-8]  offset
    ; [ebp-12] out buffer
    ; [ebp-16] out buffer_size
    ; [ebp-20] heap handle
    ; [ebp-24] raw SMBIOS 데이터 버퍼 포인터
    ; [ebp-28] 테이블 데이터 길이 (RawSMBIOSData.Length)
    ; [ebp-32] GetSystemFirmwareTable 필요 크기(임시)
    ; [ebp-36] 반환값 임시 저장(cleanup용)
    ; [ebp-40] 테이블 데이터 끝 포인터 (1회만 계산, 이후 불변)

    mov eax, DWORD PTR [ebp+8]
    mov [ebp-4], eax
    mov eax, DWORD PTR [ebp+12]
    mov [ebp-8], eax
    mov eax, DWORD PTR [ebp+16]
    mov [ebp-12], eax
    mov eax, DWORD PTR [ebp+20]
    mov [ebp-16], eax

    ; 1차 호출: 필요한 버퍼 크기 조회
    push 0                            ; BufferSize = 0
    push 0                            ; pFirmwareTableBuffer = NULL
    push 0                            ; FirmwareTableID = 0
    push 052534D42h                   ; 'RSMB'
    call GetSystemFirmwareTable@16
    test eax, eax
    jz smbios32_fail_none
    mov [ebp-32], eax

    call GetProcessHeap@0
    mov [ebp-20], eax

    push DWORD PTR [ebp-32]           ; dwBytes
    push 0                            ; dwFlags
    push DWORD PTR [ebp-20]           ; hHeap
    call HeapAlloc@12
    test eax, eax
    jz smbios32_fail_none
    mov [ebp-24], eax

    ; 2차 호출: 실제 데이터 취득
    push DWORD PTR [ebp-32]
    push DWORD PTR [ebp-24]
    push 0
    push 052534D42h
    call GetSystemFirmwareTable@16
    test eax, eax
    jz smbios32_fail_free

    ; RawSMBIOSData 헤더 파싱
    mov esi, [ebp-24]
    mov eax, DWORD PTR [esi+4]        ; Length
    mov [ebp-28], eax

    lea esi, [esi+8]                  ; 현재 구조체 포인터 (시작)
    mov eax, [ebp-24]
    add eax, 8
    add eax, DWORD PTR [ebp-28]
    mov [ebp-40], eax                 ; 테이블 데이터 끝 (1회 계산, 고정)

smbios32_scan_loop:
    mov edx, [ebp-40]
    cmp esi, edx
    jae smbios32_fail_free

    movzx eax, BYTE PTR [esi]         ; Type
    cmp al, 127
    je smbios32_fail_free

    movzx ecx, BYTE PTR [esi+1]       ; FormattedLength

    mov edx, [ebp-4]
    cmp al, dl
    jne smbios32_skip

    mov edx, [ebp-8]                  ; offset
    cmp cl, dl
    jbe smbios32_skip                 ; FormattedLength <= offset : 필드 없음

    movzx eax, BYTE PTR [esi+edx]     ; 문자열 인덱스(1-based)
    test al, al
    jz smbios32_fail_free
    mov ebx, eax                      ; 목표 문자열 번호

    lea edi, [esi+ecx]                ; 문자열 셋 시작
    mov edx, 1

smbios32_find_string:
    cmp dl, bl
    je smbios32_string_found
smbios32_skip_one_char:
    cmp BYTE PTR [edi], 0
    je smbios32_skip_one_end
    inc edi
    jmp smbios32_skip_one_char
smbios32_skip_one_end:
    inc edi
    inc dl
    cmp BYTE PTR [edi], 0
    je smbios32_fail_free             ; 해당 번호의 문자열 없음
    jmp smbios32_find_string

smbios32_string_found:
    mov esi, [ebp-12]                 ; out buffer (원래 esi값은 더 이상 불필요)
    mov ecx, [ebp-16]                 ; out buffer_size
    xor eax, eax
smbios32_copy_loop:
    cmp eax, ecx
    jae smbios32_copy_done
    movzx edx, BYTE PTR [edi+eax]
    test dl, dl
    jz smbios32_copy_done
    mov BYTE PTR [esi+eax], dl
    inc eax
    jmp smbios32_copy_loop
smbios32_copy_done:
    cmp eax, ecx
    jb smbios32_copy_null_ok
    dec eax
smbios32_copy_null_ok:
    mov BYTE PTR [esi+eax], 0
    mov eax, 1
    jmp smbios32_cleanup

smbios32_skip:
    lea edi, [esi+ecx]                ; 포맷 영역 끝 = 문자열 셋 시작
smbios32_skip_scan:
    cmp BYTE PTR [edi], 0
    jne smbios32_skip_adv
    cmp BYTE PTR [edi+1], 0
    jne smbios32_skip_adv
    lea esi, [edi+2]                  ; 다음 구조체 시작
    jmp smbios32_scan_loop
smbios32_skip_adv:
    inc edi
    jmp smbios32_skip_scan

smbios32_fail_free:
    xor eax, eax
    mov [ebp-36], eax
    push DWORD PTR [ebp-24]
    push 0
    push DWORD PTR [ebp-20]
    call HeapFree@12
    mov eax, [ebp-36]
    jmp smbios32_exit

smbios32_cleanup:
    mov [ebp-36], eax
    push DWORD PTR [ebp-24]
    push 0
    push DWORD PTR [ebp-20]
    call HeapFree@12
    mov eax, [ebp-36]
    jmp smbios32_exit

smbios32_fail_none:
    xor eax, eax                      ; 힙 할당 전 실패 -> HeapFree 불필요

smbios32_exit:
    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret
get_smbios_string_32 ENDP

END
