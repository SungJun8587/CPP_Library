TITLE CpuInfo64.asm

.code

;***************************************************************************
; @brief   CPU의 CPUID 지원 여부를 검사합니다 (x64).
; @param   없음
; @return  RAX - 1: 지원함, 0: 미지원
; @detail  RFLAGS 비트 21 변경 유무로 지원 여부를 판별합니다.
;***************************************************************************
cpu_id_supported PROC
    push rbx
    pushfq
    pop rax
    mov rbx, rax
    xor rax, 200000h
    push rax
    popfq
    pushfq
    pop rax
    cmp rax, rbx
    jz not_supported
    
    mov eax, 1
    jmp exit_cpuid_check

not_supported:
    xor eax, eax

exit_cpuid_check:
    pop rbx
    ret
cpu_id_supported ENDP


;***************************************************************************
; @brief   CPUID 지시어를 실행하고 결과를 레지스터별 포인터로 반환합니다 (x64).
; @param   RCX - eax*
; @param   RDX - ebx*
; @param   R8  - ecx*
; @param   R9  - edx*
; @return  없음
; @detail  x64 Microsoft FastCall 규약으로 레지스터 포인터 조작을 수행합니다.
;***************************************************************************
cpu_id PROC
    push rbx
    push rsi
    push rdi

    mov rsi, rcx
    mov rdi, rdx

    xor eax, eax
    test rsi, rsi
    jz skip_eax_in
    mov eax, DWORD PTR [rsi]
skip_eax_in:

    xor ecx, ecx
    test r8, r8
    jz skip_ecx_in
    mov ecx, DWORD PTR [r8]
skip_ecx_in:

    cpuid

    test rsi, rsi
    jz skip_eax_out
    mov DWORD PTR [rsi], eax
skip_eax_out:

    test rdi, rdi
    jz skip_ebx_out
    mov DWORD PTR [rdi], ebx
skip_ebx_out:

    test r8, r8
    jz skip_ecx_out
    mov DWORD PTR [r8], ecx
skip_ecx_out:

    test r9, r9
    jz skip_edx_out
    mov DWORD PTR [r9], edx
skip_edx_out:

    pop rdi
    pop rsi
    pop rbx
    ret
cpu_id ENDP


;***************************************************************************
; @brief   요청된 레벨의 캐시 크기를 KB 단위로 계산합니다 (x64).
; @param   RCX - cache_level
; @return  RAX - 캐시 크기 (KB)
; @detail  CPUID Leaf 4 데이터를 분석 및 계산합니다.
;***************************************************************************
cpu_cache_size_kb PROC
    call cpu_id_supported
    test eax, eax
    jz cache_not_found

    push rbx

    mov r8d, ecx      ; target cache_level
    xor r9d, r9d      ; 서브리프 카운터 (CPUID로 훼손되지 않음)

find_cache_loop:
    mov eax, 4
    mov ecx, r9d
    ; [수정] 기존에는 동일한 입력(eax=4, ecx=r9d)으로 CPUID를 두 번 호출했지만,
    ; 첫 호출의 EAX가 이미 Cache Type(bits 4:0)과 Cache Level(bits 7:5)을
    ; 모두 담고 있어 두 번째 호출은 완전히 중복이었습니다. CPUID는 파이프라인을
    ; 직렬화시키는 무거운 명령이라 한 서브리프당 한 번만 실행하도록 정리합니다.
    cpuid

    mov r10d, eax
    and r10d, 1Fh
    jz cache_not_found_pop   ; Cache Type == 0 -> 더 이상 캐시 없음

    mov r11d, eax
    shr r11d, 5
    and r11d, 7               ; Cache Level
    cmp r11d, r8d
    jne next_subleaf

    ; 목표 레벨과 일치: EBX/ECX는 방금 실행한 CPUID의 결과 그대로 사용
    mov eax, ebx
    shr eax, 22
    inc eax

    mov r10d, ebx
    shr r10d, 12
    and r10d, 3FFh
    inc r10d
    imul eax, r10d

    mov r10d, ebx
    and r10d, 0FFFh
    inc r10d
    imul eax, r10d

    inc ecx                   ; Sets - 1 (이번 서브리프의 진짜 CPUID 출력값)
    imul eax, ecx

    shr eax, 10
    pop rbx
    ret

next_subleaf:
    inc r9d
    jmp find_cache_loop

cache_not_found_pop:
    pop rbx

cache_not_found:
    xor eax, eax
    ret
cpu_cache_size_kb ENDP


;***************************************************************************
; @brief   실시간 타임스탬프 카운터를 구합니다 (x64).
; @param   없음
; @return  RAX - 64비트 TSC 사이클 수
; @detail  RDTSC 결과를 64비트 RAX 레지스터로 구성합니다.
;***************************************************************************
cpu_read_tsc PROC
    rdtsc
    shl rdx, 32
    or  rax, rdx
    ret
cpu_read_tsc ENDP


;***************************************************************************
; @brief   CPU 제조사 문자열 취득 (x64)
; @param   RCX - highestcpuid* (OUT)
; @param   RDX - vendorname*   (OUT, 최소 13바이트 버퍼)
; @return  RAX - 1: 성공, 0: 실패
;***************************************************************************
cpu_vendor PROC
    push rbx
    push rsi
    push rdi                 ; RDI 레지스터 보존 추가

    mov rsi, rcx             ; highestcpuid* 보관
    mov rdi, rdx             ; vendorname* 포인터를 RDI로 안전하게 보관 (RDX 오염 방지)

    xor eax, eax             ; Leaf 0
    cpuid                    ; CPUID 실행: EAX, EBX, ECX, RDX 레지스터의 값이 변경됨

    ; 최고 지원 Leaf 저장
    test rsi, rsi
    jz skip_highest
    mov DWORD PTR [rsi], eax
skip_highest:

    ; 벤더 문자열 버퍼 검사 (RDI에 보관된 진짜 포인터 사용)
    test rdi, rdi
    jz vendor_fail

    ; EBX -> EDX -> ECX 순서로 버퍼 복사
    mov DWORD PTR [rdi], ebx
    mov DWORD PTR [rdi+4], edx
    mov DWORD PTR [rdi+8], ecx
    mov BYTE PTR [rdi+12], 0 ; Null termination

    mov eax, 1
    pop rdi                  ; 레지스터 복원
    pop rsi
    pop rbx
    ret

vendor_fail:
    xor eax, eax
    pop rdi                  ; 레지스터 복원
    pop rsi
    pop rbx
    ret
cpu_vendor ENDP


;***************************************************************************
; @brief   CPU 브랜드 이름 파트 0~5 취득 (x64)
;***************************************************************************
cpu_brand_part0 PROC
    push rbx
    mov eax, 80000002h
    cpuid
    shl rbx, 32
    mov eax, eax
    or rax, rbx
    pop rbx
    ret
cpu_brand_part0 ENDP

cpu_brand_part1 PROC
    push rbx
    mov eax, 80000002h
    cpuid
    shl rdx, 32
    mov eax, ecx
    or rax, rdx
    pop rbx
    ret
cpu_brand_part1 ENDP

cpu_brand_part2 PROC
    push rbx
    mov eax, 80000003h
    cpuid
    shl rbx, 32
    mov eax, eax
    or rax, rbx
    pop rbx
    ret
cpu_brand_part2 ENDP

cpu_brand_part3 PROC
    push rbx
    mov eax, 80000003h
    cpuid
    shl rdx, 32
    mov eax, ecx
    or rax, rdx
    pop rbx
    ret
cpu_brand_part3 ENDP

cpu_brand_part4 PROC
    push rbx
    mov eax, 80000004h
    cpuid
    shl rbx, 32
    mov eax, eax
    or rax, rbx
    pop rbx
    ret
cpu_brand_part4 ENDP

cpu_brand_part5 PROC
    push rbx
    mov eax, 80000004h
    cpuid
    shl rdx, 32
    mov eax, ecx
    or rax, rdx
    pop rbx
    ret
cpu_brand_part5 ENDP


;***************************************************************************
; @brief   CPU 브랜드 이름 전체(최대 48자)를 취득합니다 (x64).
; @param   RCX - brandname 버퍼 포인터 (최소 49바이트 이상)
; @return  RAX - 1: 성공, 0: 실패
;***************************************************************************
cpu_brand PROC
    push rbx
    push rdi

    test rcx, rcx
    jz brand_fail

    mov rdi, rcx            ; 버퍼 포인터 보관

    ; Extended Leaf 80000002h
    mov eax, 80000002h
    cpuid
    mov DWORD PTR [rdi], eax
    mov DWORD PTR [rdi+4], ebx
    mov DWORD PTR [rdi+8], ecx
    mov DWORD PTR [rdi+12], edx

    ; Extended Leaf 80000003h
    mov eax, 80000003h
    cpuid
    mov DWORD PTR [rdi+16], eax
    mov DWORD PTR [rdi+20], ebx
    mov DWORD PTR [rdi+24], ecx
    mov DWORD PTR [rdi+28], edx

    ; Extended Leaf 80000004h
    mov eax, 80000004h
    cpuid
    mov DWORD PTR [rdi+32], eax
    mov DWORD PTR [rdi+36], ebx
    mov DWORD PTR [rdi+40], ecx
    mov DWORD PTR [rdi+44], edx

    mov BYTE PTR [rdi+48], 0  ; Null termination

    mov eax, 1
    pop rdi
    pop rbx
    ret

brand_fail:
    xor eax, eax
    pop rdi
    pop rbx
    ret
cpu_brand ENDP


;***************************************************************************
; @brief   하이브리드 코어 유형 판별 (x64)
; @return  RAX - 0x20: E-Core, 0x40: P-Core, 0: 일반/미지원
;***************************************************************************
cpu_core_type PROC
    push rbx

    mov eax, 1Ah             ; Hybrid Core Native Model ID Leaf
    xor ecx, ecx
    cpuid

    shr eax, 24              ; EAX[31:24] 비트 추출
    and eax, 0FFh

    pop rbx
    ret
cpu_core_type ENDP


END