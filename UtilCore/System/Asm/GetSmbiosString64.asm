TITLE get_smbios_string_64.asm
; SMBIOS 문자열/숫자 필드 리더 (x64). 전부 GetSystemFirmwareTable('RSMB')로 raw 테이블을
; 읽어 Type/offset 기준으로 구조체를 순회하는 동일 패턴을 쓴다.
;
;   smbios_cache_open_64() -> 1/0
;       SMBIOS 테이블을 한 번 가져와 전역에 캐시. 이후 get_smbios_* 호출들은 매번
;       새로 fetch하지 않고 이 캐시를 재사용함. 여러 필드를 연달아 읽을 때(RAM 슬롯
;       루프 등) GetSystemFirmwareTable/HeapAlloc 반복 호출을 없애기 위함.
;       스레드 안전하지 않음(전역 상태) - 단일 스레드에서만 사용할 것.
;   smbios_cache_close_64() -> 없음
;       캐시 해제. open을 불렀으면 반드시 짝을 맞춰 호출해야 함.
;   get_smbios_instance_count_64(type) -> 해당 Type 구조체 개수
;   get_smbios_string_instance_64(type, offset, instance, buffer, buffer_size) -> 1/0
;       instance(0-based)번째 Type 구조체에서 offset 위치의 문자열 인덱스를 읽어 문자열 복사
;   get_smbios_string_64(type, offset, buffer, buffer_size) -> 1/0
;       위 함수의 instance=0 고정 래퍼 (기존 get_ram_manufacturer/get_ram_locator 호환용)
;   get_smbios_word_64(type, offset, instance, unsigned short* outValue) -> 1/0
;       문자열이 아닌 raw WORD 필드(예: RAM Size/Speed)를 직접 읽음
;
; 위 4개 조회 함수는 캐시가 열려있으면(smbios_cache_open_64 호출됨) 그걸 재사용하고,
; 안 열려있으면 예전처럼 자체적으로 fetch+alloc+free를 매 호출마다 수행함(하위 호환).
;
; Kernel32.lib 링크 필요.

EXTERN GetSystemFirmwareTable : PROC
EXTERN GetProcessHeap : PROC
EXTERN HeapAlloc : PROC
EXTERN HeapFree : PROC

.data
; SMBIOS 캐시 전역 상태. g_pSmbiosCache64=0이면 "캐시 없음" - 각 조회 함수가 이 경우
; 예전처럼 매번 직접 fetch한다. 스레드 안전하지 않음(멀티스레드에서 쓰려면 호출부가
; 직접 동기화해야 함).
g_pSmbiosCache64   QWORD 0     ; 캐시된 raw SMBIOS 버퍼 포인터
g_dwSmbiosCacheLen64 DWORD 0   ; 캐시된 테이블 데이터 길이(RawSMBIOSData.Length)
g_hSmbiosCacheHeap64 QWORD 0   ; 캐시 버퍼를 할당한 힙 핸들

.code

;***************************************************************************
; smbios_cache_open_64() -> int(1/0)
; @detail 이미 열려있으면(재진입) 그냥 성공(1)을 반환한다.
;***************************************************************************
smbios_cache_open_64 PROC
    cmp QWORD PTR [g_pSmbiosCache64], 0
    jne sco_success

    push rbp
    mov rbp, rsp
    sub rsp, 64

    mov ecx, 052534D42h              ; 'RSMB'
    xor edx, edx
    xor r8, r8
    xor r9d, r9d
    call GetSystemFirmwareTable
    test eax, eax
    jz sco_fail
    mov DWORD PTR [rbp-8], eax       ; 필요 크기

    call GetProcessHeap
    mov [rbp-16], rax                ; heap handle

    mov rcx, [rbp-16]
    xor edx, edx
    mov r8d, DWORD PTR [rbp-8]
    call HeapAlloc
    test rax, rax
    jz sco_fail
    mov [rbp-24], rax                ; 할당된 버퍼

    mov ecx, 052534D42h
    xor edx, edx
    mov r8, [rbp-24]
    mov r9d, DWORD PTR [rbp-8]
    call GetSystemFirmwareTable
    test eax, eax
    jz sco_fail_free

    mov rax, [rbp-24]
    mov edx, DWORD PTR [rax+4]       ; Length
    mov DWORD PTR [g_dwSmbiosCacheLen64], edx
    mov rcx, [rbp-16]
    mov QWORD PTR [g_hSmbiosCacheHeap64], rcx
    mov QWORD PTR [g_pSmbiosCache64], rax

    mov eax, 1
    jmp sco_exit

sco_fail_free:
    mov rcx, [rbp-16]
    xor edx, edx
    mov r8, [rbp-24]
    call HeapFree

sco_fail:
    xor eax, eax

sco_exit:
    add rsp, 64
    pop rbp
    ret

sco_success:
    mov eax, 1
    ret
smbios_cache_open_64 ENDP


;***************************************************************************
; smbios_cache_close_64() -> 없음
;***************************************************************************
smbios_cache_close_64 PROC
    cmp QWORD PTR [g_pSmbiosCache64], 0
    je scc_exit

    sub rsp, 40
    mov rcx, QWORD PTR [g_hSmbiosCacheHeap64]
    xor edx, edx
    mov r8, QWORD PTR [g_pSmbiosCache64]
    call HeapFree
    add rsp, 40

    mov QWORD PTR [g_pSmbiosCache64], 0
    mov QWORD PTR [g_hSmbiosCacheHeap64], 0
    mov DWORD PTR [g_dwSmbiosCacheLen64], 0

scc_exit:
    ret
smbios_cache_close_64 ENDP


;***************************************************************************
; get_smbios_instance_count_64(int type) -> int count
; RCX = type
;***************************************************************************
get_smbios_instance_count_64 PROC
    push rbp
    mov rbp, rsp
    sub rsp, 112

    ; [rbp-8]  type
    ; [rbp-40] heap handle (자체 fetch한 경우만 유효)
    ; [rbp-48] raw SMBIOS 데이터 포인터 (캐시 또는 자체 fetch)
    ; [rbp-56] GetSystemFirmwareTable 필요 크기(임시, 자체 fetch 경로에서만 사용)
    ; [rbp-64] 반환값 임시(cleanup용)
    ; [rbp-72] 테이블 데이터 끝 포인터(1회 계산, 이후 불변)
    ; [rbp-80] 카운트
    ; [rbp-88] 테이블 데이터 길이
    ; [rbp-96] 이 버퍼를 이 함수가 직접 소유했는지(1=자체 fetch, HeapFree 필요) /
    ;          캐시를 빌려썼는지(0, HeapFree 하면 안 됨)

    mov [rbp-8], rcx

    cmp QWORD PTR [g_pSmbiosCache64], 0
    je cnt_fetch_own

    ; 캐시 재사용 - fetch/alloc 완전히 생략
    mov rax, QWORD PTR [g_pSmbiosCache64]
    mov [rbp-48], rax
    mov eax, DWORD PTR [g_dwSmbiosCacheLen64]
    mov DWORD PTR [rbp-88], eax
    mov DWORD PTR [rbp-96], 0
    jmp cnt_have_data

cnt_fetch_own:
    mov DWORD PTR [rbp-96], 1

    mov ecx, 052534D42h
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
    mov DWORD PTR [rbp-88], edx

cnt_have_data:
    mov rax, [rbp-48]
    lea rax, [rax+8]
    mov r10, rax                     ; 현재 구조체 포인터
    movsxd rdx, DWORD PTR [rbp-88]
    add rax, rdx
    mov [rbp-72], rax                ; 끝 포인터(1회 계산, 이후 불변)

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
    mov r11, [rbp-72]
    lea rax, [r9+1]
    cmp rax, r11
    jae smbios_cnt_done               ; 안전하게 읽을 수 있는 바이트가 없음 -> 손상 데이터, 여기까지만 카운트
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
    cmp DWORD PTR [rbp-96], 0
    je smbios_cnt_skip_free
    mov rcx, [rbp-40]
    xor edx, edx
    mov r8, [rbp-48]
    call HeapFree
smbios_cnt_skip_free:
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
; RCX=type, RDX=offset, R8d=instance(0-based), R9=buffer, [rbp+48]=buffer_size(5번째 인자, 스택)
;***************************************************************************
get_smbios_string_instance_64 PROC
    push rbp
    mov rbp, rsp
    sub rsp, 112

    ; [rbp-8]  type
    ; [rbp-16] offset
    ; [rbp-24] instance(목표)
    ; [rbp-32] out buffer
    ; [rbp-40] heap handle (자체 fetch한 경우만 유효)
    ; [rbp-48] raw SMBIOS 데이터 포인터 (캐시 또는 자체 fetch)
    ; [rbp-56] 필요 크기(임시, 자체 fetch 경로에서만 사용)
    ; [rbp-64] 반환값 임시
    ; [rbp-72] 테이블 데이터 끝 포인터
    ; [rbp-80] 지금까지 찾은 매치 수
    ; [rbp-88] 테이블 데이터 길이
    ; [rbp-96] 버퍼 소유 여부(1=자체 fetch, HeapFree 필요 / 0=캐시 재사용, HeapFree 금지)
    ;
    ; 5번째 인자(buffer_size) 스택 오프셋: push rbp 직후 rbp = (진입시 rsp) - 8 이므로
    ; 5번째 인자는 (진입시 rsp)+40 = rbp+48. (rbp+40은 틀린 위치 - 과거 버그였음)

    mov [rbp-8], rcx
    mov [rbp-16], rdx
    mov DWORD PTR [rbp-24], r8d
    mov [rbp-32], r9

    cmp QWORD PTR [g_pSmbiosCache64], 0
    je inst_fetch_own

    mov rax, QWORD PTR [g_pSmbiosCache64]
    mov [rbp-48], rax
    mov eax, DWORD PTR [g_dwSmbiosCacheLen64]
    mov DWORD PTR [rbp-88], eax
    mov DWORD PTR [rbp-96], 0
    jmp inst_have_data

inst_fetch_own:
    mov DWORD PTR [rbp-96], 1

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
    mov DWORD PTR [rbp-88], edx

inst_have_data:
    mov rax, [rbp-48]
    lea rax, [rax+8]
    mov r10, rax
    movsxd rdx, DWORD PTR [rbp-88]
    add rax, rdx
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
    mov r11, [rbp-72]
    cmp r9, r11
    jae smbios_inst_fail_free         ; 테이블 끝 도달 -> 손상 데이터
    cmp BYTE PTR [r9], 0
    je smbios_inst_skip_char_end
    inc r9
    jmp smbios_inst_skip_char
smbios_inst_skip_char_end:
    inc r9
    inc dl
    mov r11, [rbp-72]
    cmp r9, r11
    jae smbios_inst_fail_free
    cmp BYTE PTR [r9], 0
    je smbios_inst_fail_free          ; 해당 번호의 문자열 없음
    jmp smbios_inst_find_string

smbios_inst_string_found:
    mov r10, [rbp-32]                 ; out buffer (구조체 스캔용 r10은 더 이상 불필요)
    mov r8d, DWORD PTR [rbp+48]       ; out buffer_size (호출자 스택의 5번째 인자에서 직접 읽음)
    xor ecx, ecx
smbios_inst_copy_loop:
    cmp ecx, r8d
    jae smbios_inst_copy_done
    mov r11, [rbp-72]
    lea rax, [r9+rcx]
    cmp rax, r11
    jae smbios_inst_copy_done         ; 테이블 끝 도달 -> 있는 데이터까지만 복사
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
    mov r11, [rbp-72]
    lea rax, [r9+1]
    cmp rax, r11
    jae smbios_inst_fail_free
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
    cmp DWORD PTR [rbp-96], 0
    je smbios_inst_skip_free
    mov rcx, [rbp-40]
    xor edx, edx
    mov r8, [rbp-48]
    call HeapFree
smbios_inst_skip_free:
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

    cmp QWORD PTR [g_pSmbiosCache64], 0
    je word_fetch_own

    mov rax, QWORD PTR [g_pSmbiosCache64]
    mov [rbp-48], rax
    mov eax, DWORD PTR [g_dwSmbiosCacheLen64]
    mov DWORD PTR [rbp-88], eax
    mov DWORD PTR [rbp-96], 0
    jmp word_have_data

word_fetch_own:
    mov DWORD PTR [rbp-96], 1

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
    mov DWORD PTR [rbp-88], edx

word_have_data:
    mov rax, [rbp-48]
    lea rax, [rax+8]
    mov r10, rax
    movsxd rdx, DWORD PTR [rbp-88]
    add rax, rdx
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
    lea rax, [rdx+1]                  ; WORD 필드는 offset,offset+1 두 바이트만 있으면 충분
    cmp cl, al
    jbe smbios_word_fail_free         ; FormattedLength <= offset+1 : 필드 없음 (예전엔 +2로 과잉 엄격했음)

    movzx eax, WORD PTR [r10+rdx]     ; raw WORD 값 읽기 (r10 = 구조체 시작 포인터)
    mov r11, [rbp-32]                 ; out ptr
    mov WORD PTR [r11], ax
    mov eax, 1
    jmp smbios_word_cleanup

smbios_word_skip:
    lea r9, [r10+rcx]
smbios_word_skip_scan:
    mov r11, [rbp-72]
    lea rax, [r9+1]
    cmp rax, r11
    jae smbios_word_fail_free
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
    cmp DWORD PTR [rbp-96], 0
    je smbios_word_skip_free
    mov rcx, [rbp-40]
    xor edx, edx
    mov r8, [rbp-48]
    call HeapFree
smbios_word_skip_free:
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