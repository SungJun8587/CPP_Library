.MODEL FLAT, C
.CODE

;***************************************************************************
; @brief   CPU가 CPUID 지시어를 지원하는지 여부를 검사합니다 (x86 32비트).
; @param   없음
; @return  EAX - 1: 지원함, 0: 미지원
; @detail  EFLAGS 레지스터의 Bit 21(ID 비트)을 토글하여 CPUID 지원 여부를 판별합니다.
;***************************************************************************
cpu_id_supported PROC
    push ebp
    mov ebp, esp
    push ebx

    pushfd                  ; EFLAGS 저장
    pop eax                 ; EAX에 EFLAGS 로드
    mov ebx, eax            ; 원본 EFLAGS를 EBX에 보관
    
    xor eax, 00200000h      ; Bit 21 (ID Bit) 토글
    push eax
    popfd                   ; 변경된 값으로 EFLAGS 설정
    
    pushfd
    pop eax                 ; EFLAGS 다시 읽기
    
    cmp eax, ebx            ; Bit 21이 변경되었는지 비교
    jz not_supported

    mov eax, 1              ; 변경 성공 = CPUID 지원
    jmp exit_check

not_supported:
    xor eax, eax            ; 변경 불가 = CPUID 미지원

exit_check:
    pop ebx
    pop ebp
    ret
cpu_id_supported ENDP


;***************************************************************************
; @brief   CPUID 지시어를 실행하고 결과를 레지스터별 포인터로 반환합니다 (x86 32비트).
; @param   [ESP+4]  - eax* (IN/OUT)
; @param   [ESP+8]  - ebx* (OUT)
; @param   [ESP+12] - ecx* (IN/OUT)
; @param   [ESP+16] - edx* (OUT)
; @return  없음
; @detail  32비트 cdecl 호출 규약에 따라 포인터에서 입력값을 받아 CPUID 실행 후 결과를 할당합니다.
;***************************************************************************
cpu_id PROC
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    ; 입력 EAX 파싱
    mov esi, [ebp+8]        ; eax*
    xor eax, eax
    test esi, esi
    jz skip_in_eax
    mov eax, [esi]          ; *eax 값 읽기
skip_in_eax:

    ; 입력 ECX 파싱 (Sub-leaf 용도)
    mov edi, [ebp+16]       ; ecx*
    xor ecx, ecx
    test edi, edi
    jz skip_in_ecx
    mov ecx, [edi]          ; *ecx 값 읽기
skip_in_ecx:

    ; CPUID 실행
    cpuid

    ; EAX 결과 출력
    test esi, esi
    jz skip_out_eax
    mov [esi], eax
skip_out_eax:

    ; EBX 결과 출력
    mov esi, [ebp+12]       ; ebx*
    test esi, esi
    jz skip_out_ebx
    mov [esi], ebx
skip_out_ebx:

    ; ECX 결과 출력
    test edi, edi
    jz skip_out_ecx
    mov [edi], ecx
skip_out_ecx:

    ; EDX 결과 출력
    mov esi, [ebp+20]       ; edx*
    test esi, esi
    jz skip_out_edx
    mov [esi], edx
skip_out_edx:

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
cpu_id ENDP


;***************************************************************************
; @brief   CPU 제조사 문자열(Vendor String)을 취득합니다 (x86 32비트).
; @param   [ESP+4] - highestcpuid* (OUT)
; @param   [ESP+8] - vendorname*   (OUT, 최소 13바이트 버퍼)
; @return  EAX - 1: 성공, 0: 실패
; @detail  CPUID Leaf 0을 실행하여 EBX-EDX-ECX 레지스터의 아스키 명칭을 전달받은 버퍼에 복사합니다.
;***************************************************************************
cpu_vendor PROC
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov edi, [ebp+12]       ; vendorname*
    test edi, edi
    jz vendor_fail

    ; [수정] highestcpuid* 포인터를 CPUID 실행 "전에" ESI로 미리 옮겨둡니다.
    ; 기존 코드는 CPUID 실행 후 이 포인터를 ECX에 로드했는데, 이 시점의 ECX는
    ; CPUID가 반환한 벤더 문자열의 마지막 4바이트를 담고 있어 그 값이
    ; 훼손되고, 결국 [edi+8]에 벤더 문자열 대신 포인터 주소값이 복사되는
    ; 버그가 있었습니다. 별도 레지스터(ESI)를 사용해 CPUID의 출력
    ; 레지스터(ebx/ecx/edx)를 전혀 건드리지 않도록 합니다.
    mov esi, [ebp+8]        ; highestcpuid*

    xor eax, eax            ; Leaf 0
    cpuid

    ; 최고 지원 Leaf 복사 (ESI는 CPUID로 훼손되지 않음)
    test esi, esi
    jz skip_highest
    mov [esi], eax
skip_highest:

    ; EBX -> EDX -> ECX 순서로 버퍼 복사 (12바이트 Vendor ID)
    mov [edi], ebx          ; Offset 0~3
    mov [edi+4], edx        ; Offset 4~7
    mov [edi+8], ecx        ; Offset 8~11 (수정: 더 이상 포인터로 덮어써지지 않음)
    mov BYTE PTR [edi+12], 0 ; Null Terminator

    mov eax, 1
    jmp vendor_exit

vendor_fail:
    xor eax, eax

vendor_exit:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
cpu_vendor ENDP


;***************************************************************************
; @brief   CPU 브랜드 이름 파트 0을 취득합니다 (x86 32비트).
; @param   없음
; @return  EDX:EAX (64비트) - CPU Brand String의 앞선 8바이트 데이터
; @detail  CPUID Extended Leaf 80000002h를 호출하여 EAX, EBX 값을 64비트 리턴값(EDX:EAX)으로 결합합니다.
;***************************************************************************
cpu_brand_part0 PROC
    push ebp
    mov ebp, esp
    push ebx

    mov eax, 80000002h      ; Extended Leaf 80000002h
    cpuid

    ; cdecl 64비트 반환 규약: 하위 32비트는 EAX, 상위 32비트는 EDX에 할당
    ; EAX = EAX 레지스터 결과 (하위 4바이트)
    mov edx, ebx            ; EDX = EBX 레지스터 결과 (상위 4바이트)

    pop ebx
    pop ebp
    ret
cpu_brand_part0 ENDP


;***************************************************************************
; @brief   현재 코어의 하이브리드 아키텍처 유형을 판별합니다 (x86 32비트).
; @param   없음
; @return  EAX - 0x20: E-Core, 0x40: P-Core, 0: 일반 CPU
; @detail  CPUID Leaf 1Ah의 EAX[31:24] 비트를 추출합니다.
;***************************************************************************
cpu_core_type PROC
    push ebp
    mov ebp, esp
    push ebx

    mov eax, 1Ah            ; Hybrid Information Leaf
    xor ecx, ecx            ; Sub-leaf 0
    cpuid

    shr eax, 24             ; EAX[31:24] 비트 추출
    and eax, 0FFh

    pop ebx
    pop ebp
    ret
cpu_core_type ENDP


;***************************************************************************
; @brief   CPU 실시간 타임스탬프 카운터(TSC)를 읽습니다 (x86 32비트).
; @param   없음
; @return  EDX:EAX (64비트) - CPU 누적 클록 사이클 수
; @detail  RDTSC 지시어를 통해 64비트 카운터 값을 EDX:EAX 쌍으로 반환합니다.
;***************************************************************************
cpu_read_tsc PROC
    rdtsc                   ; EDX:EAX에 64비트 TSC 값 저장됨
    ret
cpu_read_tsc ENDP


;***************************************************************************
; @brief   요청된 레벨의 캐시 크기를 KB 단위로 계산합니다 (x86 32비트).
; @param   [ESP+4] - cache_level (1: L1, 2: L2, 3: L3)
; @return  EAX - 캐시 크기 (KB 단위)
; @detail  CPUID Leaf 4 데이터를 탐색하여 캐시 크기를 계산합니다.
;***************************************************************************
cpu_cache_size_kb PROC
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov esi, [ebp+8]        ; cache_level (목표 레벨)
    ; [수정] 기존에는 ECX를 "다음에 조회할 서브리프 번호"와 "CPUID가 반환한
    ; Sets-1 값"이라는 두 가지 용도로 동시에 사용했습니다. 첫 CPUID 호출 순간
    ; ECX가 서브리프 카운터가 아니게 되어 버려, 이후 INC ECX가 엉뚱한 값을
    ; 서브리프로 사용하는 버그가 있었습니다. CPUID로 절대 훼손되지 않는 EDI를
    ; 서브리프 카운터 전용으로 분리합니다. 또한 동일 입력으로 CPUID를 두 번
    ; 호출하던 중복도 제거해 한 서브리프당 한 번만 호출하도록 정리했습니다.
    xor edi, edi             ; 서브리프 인덱스 (CPUID로 훼손되지 않음)

find_cache_loop:
    mov eax, 4
    mov ecx, edi              ; 이번 서브리프 번호를 CPUID 입력으로 사용
    cpuid                     ; eax/ebx/ecx/edx = 이 서브리프의 Leaf4 결과

    mov edx, eax
    and edx, 01Fh             ; Cache Type (bits 4:0). 0이면 더 이상 캐시 없음
    jz cache_not_found

    mov edx, eax
    shr edx, 5
    and edx, 7                ; Cache Level (bits 7:5)
    cmp edx, esi
    jne next_subleaf

    ; 목표 레벨과 일치: EBX(Ways/Partitions/LineSize), ECX(Sets-1)로 크기 계산
    ; (이 시점의 EBX/ECX는 방금 실행한 CPUID의 결과 그대로이며 훼손되지 않음)
    mov eax, ebx
    shr eax, 22               ; Ways - 1
    inc eax

    mov edx, ebx
    shr edx, 12
    and edx, 3FFh             ; Physical Partitions - 1
    inc edx
    imul eax, edx

    mov edx, ebx
    and edx, 0FFFh            ; Line Size - 1
    inc edx
    imul eax, edx

    inc ecx                   ; Sets - 1 (이번 서브리프의 진짜 CPUID 출력값)
    imul eax, ecx

    shr eax, 10                ; Byte -> KB 변환 (/1024)
    jmp exit_proc

next_subleaf:
    inc edi                    ; 다음 서브리프로 이동 (ECX와 무관하게 안전)
    jmp find_cache_loop

cache_not_found:
    xor eax, eax

exit_proc:
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
cpu_cache_size_kb ENDP


cpu_brand_part1 PROC
    push ebp
    mov ebp, esp
    push ebx
    mov eax, 80000002h
    cpuid
    mov eax, ecx
    pop ebx
    pop ebp
    ret
cpu_brand_part1 ENDP

cpu_brand_part2 PROC
    push ebp
    mov ebp, esp
    push ebx
    mov eax, 80000003h
    cpuid
    mov edx, ebx
    pop ebx
    pop ebp
    ret
cpu_brand_part2 ENDP

cpu_brand_part3 PROC
    push ebp
    mov ebp, esp
    push ebx
    mov eax, 80000003h
    cpuid
    mov eax, ecx
    pop ebx
    pop ebp
    ret
cpu_brand_part3 ENDP

cpu_brand_part4 PROC
    push ebp
    mov ebp, esp
    push ebx
    mov eax, 80000004h
    cpuid
    mov edx, ebx
    pop ebx
    pop ebp
    ret
cpu_brand_part4 ENDP

cpu_brand_part5 PROC
    push ebp
    mov ebp, esp
    push ebx
    mov eax, 80000004h
    cpuid
    mov eax, ecx
    pop ebx
    pop ebp
    ret
cpu_brand_part5 ENDP

;***************************************************************************
; @brief   CPU 브랜드 이름 전체(최대 48자)를 취득합니다 (x86 32비트).
; @param   [ESP+4] - brandname 버퍼 포인터 (최소 49바이트 이상)
; @return  EAX - 1: 성공, 0: 실패
;***************************************************************************
cpu_brand PROC
    push ebp
    mov ebp, esp
    push ebx
    push edi

    mov edi, [ebp+8]
    test edi, edi
    jz brand_fail_32

    ; Extended Leaf 80000002h
    mov eax, 80000002h
    cpuid
    mov DWORD PTR [edi], eax
    mov DWORD PTR [edi+4], ebx
    mov DWORD PTR [edi+8], ecx
    mov DWORD PTR [edi+12], edx

    ; Extended Leaf 80000003h
    mov eax, 80000003h
    cpuid
    mov DWORD PTR [edi+16], eax
    mov DWORD PTR [edi+20], ebx
    mov DWORD PTR [edi+24], ecx
    mov DWORD PTR [edi+28], edx

    ; Extended Leaf 80000004h
    mov eax, 80000004h
    cpuid
    mov DWORD PTR [edi+32], eax
    mov DWORD PTR [edi+36], ebx
    mov DWORD PTR [edi+40], ecx
    mov DWORD PTR [edi+44], edx

    mov BYTE PTR [edi+48], 0  ; Null termination

    mov eax, 1
    pop edi
    pop ebx
    pop ebp
    ret

brand_fail_32:
    xor eax, eax
    pop edi
    pop ebx
    pop ebp
    ret
cpu_brand ENDP

END
