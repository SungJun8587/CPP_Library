.MODEL FLAT, C
.CODE

;***************************************************************************
; @brief   PCI Vendor ID를 확인하여 제조사 식별 문자열을 작성합니다 (x86 32비트).
; @param   [ESP+4] - vendor_id
; @param   [ESP+8] - buffer*
; @param   [ESP+12]- buffer_size
; @return  없음
; @detail  32비트 메모리 주소 매핑을 활용해 텍스트 버퍼를 채웁니다.
;***************************************************************************
pci_parse_vendor_name PROC
    push ebp
    mov ebp, esp
    push edi

    mov cx, [ebp+8]         ; vendor_id
    mov edi, [ebp+12]       ; buffer*
    
    test edi, edi
    jz exit_null

    cmp cx, 10DEh
    je is_nvidia
    cmp cx, 1002h
    je is_amd
    cmp cx, 8086h
    je is_intel
    cmp cx, 144Dh
    je is_samsung
    jmp is_unknown

is_nvidia:
    ; [수정] 기존 상수(4149564Eh, 00004144h)를 리틀엔디안으로 풀어보면
    ; "NVIA"+"DA\0\0" = "NVIADA"가 되어 의도한 "NVIDIA"와 철자가 달랐습니다.
    ; "NVIDIA\0\0"(N V I D I A \0 \0)에 맞는 값으로 바로잡습니다.
    mov DWORD PTR [edi], 4449564Eh    ; "NVID"
    mov DWORD PTR [edi+4], 00004149h  ; "IA\0\0"
    jmp exit_null

is_amd:
    mov DWORD PTR [edi], 20444D41h    ; "AMD "
    mov DWORD PTR [edi+4], 00000000h
    jmp exit_null

is_intel:
    mov DWORD PTR [edi], 65746E49h    ; "Inte"
    mov DWORD PTR [edi+4], 0000006Ch  ; "l\0\0\0"
    jmp exit_null

is_samsung:
    ; [수정] 두 번째 DWORD(676E7575h)를 리틀엔디안으로 풀면 "uung"이 되어
    ; 주석("ung\0")과 실제 값이 달랐고, 결과적으로 "Samsuung"이 되어 널
    ; 종료자도 빠져 있었습니다. "Samsung\0"에 맞는 값으로 바로잡습니다.
    mov DWORD PTR [edi], 736D6153h    ; "Sams"
    mov DWORD PTR [edi+4], 00676E75h  ; "ung\0"
    jmp exit_null

is_unknown:
    mov DWORD PTR [edi], 6E6B6E55h    ; "Unkn"
    mov DWORD PTR [edi+4], 006E776Fh  ; "own\0"

exit_null:
    pop edi
    pop ebp
    ret
pci_parse_vendor_name ENDP


;***************************************************************************
; @brief   PCI Class 코드를 통해 디바이스 유형을 분류합니다 (x86 32비트).
; @param   [ESP+4] - base_class
; @param   [ESP+8] - sub_class
; @param   [ESP+12]- prog_if
; @return  EAX - 0: Unknown, 1: GPU, 2: NVMe
; @detail  cdecl 스택에서 8비트 Class 코드 값을 비교 판단합니다.
;***************************************************************************
pci_classify_device PROC
    push ebp
    mov ebp, esp

    mov cl, [ebp+8]         ; base_class
    mov dl, [ebp+12]        ; sub_class
    mov al, [ebp+16]        ; prog_if

    cmp cl, 03h             ; Display Controller
    je is_gpu

    cmp cl, 01h             ; Mass Storage
    jne is_other
    cmp dl, 08h             ; NVM
    jne is_other
    cmp al, 02h             ; NVMe
    je is_nvme

is_other:
    xor eax, eax
    jmp exit_class

is_gpu:
    mov eax, 1
    jmp exit_class

is_nvme:
    mov eax, 2

exit_class:
    pop ebp
    ret
pci_classify_device ENDP

END