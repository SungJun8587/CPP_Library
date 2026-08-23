.MODEL FLAT, C
TITLE PciInfo32.asm
; PCI Vendor ID -> 제조사명 조회 및 Class Code 기반 장치 분류 (x86 32비트).

.data

pci_vendor_ids WORD 8086h, 8087h, 1022h, 1002h, 10DEh, 10ECh, 14E4h, 168Ch, 1B21h, 1106h
               WORD 1039h, 144Dh, 1344h, 1C5Ch, 1B4Bh, 15ADh, 80EEh, 1414h, 1AB8h, 1179h
PCI_VENDOR_COUNT = ($ - pci_vendor_ids) / 2

str_intel      BYTE "Intel", 0
str_amd        BYTE "AMD", 0
str_amdati     BYTE "AMD/ATI", 0
str_nvidia     BYTE "NVIDIA", 0
str_realtek    BYTE "Realtek", 0
str_broadcom   BYTE "Broadcom", 0
str_qcomath    BYTE "Qualcomm Atheros", 0
str_asmedia    BYTE "ASMedia", 0
str_via        BYTE "VIA Technologies", 0
str_sis        BYTE "SiS", 0
str_samsung    BYTE "Samsung", 0
str_micron     BYTE "Micron", 0
str_skhynix    BYTE "SK hynix", 0
str_marvell    BYTE "Marvell", 0
str_vmware     BYTE "VMware", 0
str_vbox       BYTE "Oracle VirtualBox", 0
str_hyperv     BYTE "Microsoft (Hyper-V)", 0
str_parallels  BYTE "Parallels", 0
str_toshiba    BYTE "Toshiba", 0
str_unknown    BYTE "Unknown", 0

pci_vendor_names DWORD OFFSET str_intel, OFFSET str_intel, OFFSET str_amd, OFFSET str_amdati, OFFSET str_nvidia
                 DWORD OFFSET str_realtek, OFFSET str_broadcom, OFFSET str_qcomath, OFFSET str_asmedia, OFFSET str_via
                 DWORD OFFSET str_sis, OFFSET str_samsung, OFFSET str_micron, OFFSET str_skhynix, OFFSET str_marvell
                 DWORD OFFSET str_vmware, OFFSET str_vbox, OFFSET str_hyperv, OFFSET str_parallels, OFFSET str_toshiba

.code

;***************************************************************************
; @brief   PCI Vendor ID를 확인하여 제조사 식별 문자열을 작성합니다 (x86 32비트).
; @param   [ESP+4] - vendor_id (하위 16비트만 사용)
; @param   [ESP+8] - buffer*
; @param   [ESP+12]- buffer_size
; @return  없음
; @detail  pci_vendor_ids/pci_vendor_names 병렬 테이블을 선형 검색하고, buffer_size를
;          실제로 지키며 복사(초과분은 잘라내고 항상 널 종료)합니다. 매칭 실패 시 "Unknown".
;***************************************************************************
pci_parse_vendor_name PROC
    push ebp
    mov ebp, esp
    push esi
    push edi
    push ebx

    mov cx, [ebp+8]          ; vendor_id
    mov edi, [ebp+12]        ; buffer*
    mov edx, [ebp+16]        ; buffer_size

    test edi, edi
    jz pv_exit
    test edx, edx
    jz pv_exit

    xor esi, esi              ; 검색 인덱스
pv_loop:
    cmp esi, PCI_VENDOR_COUNT
    jae pv_notfound

    movzx eax, WORD PTR [pci_vendor_ids + esi*2]
    cmp ax, cx
    je pv_found

    inc esi
    jmp pv_loop

pv_found:
    mov eax, [pci_vendor_names + esi*4]
    jmp pv_copy

pv_notfound:
    lea eax, [str_unknown]

pv_copy:
    xor ecx, ecx               ; 복사한 바이트 수
pv_copy_loop:
    cmp ecx, edx
    jae pv_copy_done
    mov bl, BYTE PTR [eax+ecx]
    test bl, bl
    jz pv_copy_done
    mov BYTE PTR [edi+ecx], bl
    inc ecx
    jmp pv_copy_loop
pv_copy_done:
    cmp ecx, edx
    jb pv_null_ok
    dec ecx                    ; buffer_size와 같으면 마지막 한 바이트를 null용으로 확보
pv_null_ok:
    mov BYTE PTR [edi+ecx], 0

pv_exit:
    pop ebx
    pop edi
    pop esi
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
