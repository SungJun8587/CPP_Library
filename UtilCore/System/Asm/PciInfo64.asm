TITLE PciInfo64.asm
; PCI Vendor ID -> 제조사명 조회 및 Class Code 기반 장치 분류 (x64).
;
;   pci_parse_vendor_name(vendor_id, buffer, buffer_size) -> 없음
;   pci_classify_device(base_class, sub_class, prog_if) -> 0:Unknown 1:GPU 2:NVMe

.data

; pci_vendor_ids[i]에 대응하는 이름 포인터가 pci_vendor_names[i] - 두 배열은 반드시 같은 순서를 유지.
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

; pci_vendor_ids와 동일한 순서 (8086h, 8087h 둘 다 Intel)
pci_vendor_names QWORD OFFSET str_intel, OFFSET str_intel, OFFSET str_amd, OFFSET str_amdati, OFFSET str_nvidia
                 QWORD OFFSET str_realtek, OFFSET str_broadcom, OFFSET str_qcomath, OFFSET str_asmedia, OFFSET str_via
                 QWORD OFFSET str_sis, OFFSET str_samsung, OFFSET str_micron, OFFSET str_skhynix, OFFSET str_marvell
                 QWORD OFFSET str_vmware, OFFSET str_vbox, OFFSET str_hyperv, OFFSET str_parallels, OFFSET str_toshiba

.code

;***************************************************************************
; @brief   PCI Vendor ID를 확인하여 제조사 식별 문자열을 작성합니다 (x64).
; @param   RCX - vendor_id (하위 16비트만 사용)
; @param   RDX - buffer*
; @param   R8D - buffer_size
; @return  없음
; @detail  pci_vendor_ids/pci_vendor_names 병렬 테이블을 선형 검색하고, buffer_size를
;          실제로 지키며 복사(초과분은 잘라내고 항상 널 종료)합니다. 매칭 실패 시 "Unknown".
;***************************************************************************
pci_parse_vendor_name PROC
    test rdx, rdx
    jz pv_exit
    test r8d, r8d
    jz pv_exit

    xor r9, r9                       ; 검색 인덱스
    lea r10, [pci_vendor_ids]
    lea r11, [pci_vendor_names]

pv_loop:
    cmp r9, PCI_VENDOR_COUNT
    jae pv_notfound

    movzx eax, WORD PTR [r10 + r9*2]
    cmp ax, cx
    je pv_found

    inc r9
    jmp pv_loop

pv_found:
    mov rax, [r11 + r9*8]             ; 매칭된 문자열 포인터
    jmp pv_copy

pv_notfound:
    lea rax, [str_unknown]

pv_copy:
    xor ecx, ecx                      ; 복사한 바이트 수
pv_copy_loop:
    cmp ecx, r8d
    jae pv_copy_done
    mov r9b, BYTE PTR [rax + rcx]
    test r9b, r9b
    jz pv_copy_done
    mov BYTE PTR [rdx + rcx], r9b
    inc ecx
    jmp pv_copy_loop
pv_copy_done:
    cmp ecx, r8d
    jb pv_null_ok
    dec ecx                           ; buffer_size와 같으면 마지막 한 바이트를 null용으로 확보
pv_null_ok:
    mov BYTE PTR [rdx + rcx], 0

pv_exit:
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
