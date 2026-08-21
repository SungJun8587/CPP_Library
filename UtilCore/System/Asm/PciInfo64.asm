TITLE PciInfo64.asm

.code

;***************************************************************************
; @brief   PCI Vendor ID를 확인하여 제조사 식별 문자열을 작성합니다 (x64).
; @param   RCX - vendor_id
; @param   RDX - buffer*
; @param   R8D - buffer_size
; @return  없음
; @detail  Direct Memory Access로 아스키 파싱 데이터를 복사합니다.
;***************************************************************************
pci_parse_vendor_name PROC
    test rdx, rdx
    jz exit_null
    test r8d, r8d
    jz exit_null

    cmp cx, 10DEh          ; NVIDIA
    je is_nvidia
    cmp cx, 1002h          ; AMD
    je is_amd
    cmp cx, 8086h          ; Intel
    je is_intel
    cmp cx, 144Dh          ; Samsung
    je is_samsung
    jmp is_unknown

is_nvidia:
    ; [수정] 기존 상수(4149564Eh, 00004144h)는 리틀엔디안으로 풀면
    ; "NVIA"+"DA\0\0" = "NVIADA"가 되어 의도한 "NVIDIA"와 철자가 달랐습니다.
    mov eax, 4449564Eh     ; "NVID"
    mov DWORD PTR [rdx], eax
    mov eax, 00004149h     ; "IA\0\0"
    mov DWORD PTR [rdx+4], eax
    ret

is_amd:
    mov eax, 20444D41h     ; "AMD "
    mov DWORD PTR [rdx], eax
    mov eax, 00000000h
    mov DWORD PTR [rdx+4], eax
    ret

is_intel:
    mov eax, 65746E49h     ; "Inte"
    mov DWORD PTR [rdx], eax
    mov eax, 0000006Ch     ; "l\0\0\0"
    mov DWORD PTR [rdx+4], eax
    ret

is_samsung:
    ; [수정] 두 번째 DWORD(676E7575h)는 리틀엔디안으로 풀면 "uung"이 되어
    ; 주석("ung\0")과 실제 값이 달랐고, 결과적으로 "Samsuung"이 되어 널
    ; 종료자도 빠져 있었습니다.
    mov eax, 736D6153h     ; "Sams"
    mov DWORD PTR [rdx], eax
    mov eax, 00676E75h     ; "ung\0"
    mov DWORD PTR [rdx+4], eax
    ret

is_unknown:
    mov eax, 6E6B6E55h     ; "Unkn"
    mov DWORD PTR [rdx], eax
    mov eax, 006E776Fh     ; "own\0"
    mov DWORD PTR [rdx+4], eax

exit_null:
    ret
pci_parse_vendor_name ENDP


;***************************************************************************
; @brief   PCI Class 코드 세트를 통해 디바이스 유형을 분류합니다 (x64).
; @param   RCX - base_class
; @param   RDX - sub_class
; @param   R8  - prog_if
; @return  EAX - 0: Unknown, 1: GPU, 2: NVMe
; @detail  Class 코드 및 프로그래밍 인터페이스를 비교 판별합니다.
;***************************************************************************
pci_classify_device PROC
    cmp cl, 03h            ; Display Controller (GPU)
    je is_gpu

    cmp cl, 01h            ; Mass Storage
    jne is_other
    cmp dl, 08h            ; Non-Volatile Memory
    jne is_other
    cmp r8b, 02h           ; NVMe Interface
    je is_nvme

is_other:
    xor eax, eax
    ret

is_gpu:
    mov eax, 1
    ret

is_nvme:
    mov eax, 2
    ret
pci_classify_device ENDP

END