TITLE MemDiskDetail64.asm

EXTERN CreateFileA : PROC
EXTERN DeviceIoControl : PROC
EXTERN CloseHandle : PROC
EXTERN get_smbios_string_64 : PROC

.code

;***************************************************************************
; @brief   디스크 물리 상세 정보(모델명/시리얼/인터페이스) 취득 (x64)
; @param   RCX - drive_index (0: 첫번째 디스크, 1: 두번째 ...)
; @param   RDX - out_buffer  (STORAGE_DEVICE_DESCRIPTOR 파싱 데이터 수신 버퍼)
; @param   R8  - buffer_size
; @return  RAX - 1: 성공, 0: 실패
;***************************************************************************
get_disk_detail_info_64 PROC
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    push r15

    sub rsp, 104                    ; Shadow space + Local Variables

    mov r12, rdx                    ; out_buffer
    mov r13d, r8d                   ; buffer_size

    ; 1. DevicePath 생성 ("\\.\PhysicalDrive0")
    mov DWORD PTR [rbp-48], 5C2E5C5Ch ; "\\.\ " (리틀엔디안 보정)
    mov DWORD PTR [rbp-44], 73796850h ; "Phys"
    mov DWORD PTR [rbp-40], 6C616369h ; "ical"
    mov DWORD PTR [rbp-36], 76697244h ; "Driv" (리틀엔디안 보정)
    mov WORD PTR  [rbp-32], 3065h     ; "e0"
    mov BYTE PTR  [rbp-30], 0         ; Null

    ; drive_index 반영 ('0' + index)
    mov al, cl
    add al, '0'
    mov BYTE PTR [rbp-31], al

    ; 2. CreateFileA 호출 (GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE)
    lea rcx, [rbp-48]               ; lpFileName
    mov edx, 0C0000000h             ; dwDesiredAccess
    mov r8d, 3                      ; dwShareMode (READ | WRITE)
    xor r9, r9                      ; lpSecurityAttributes
    mov QWORD PTR [rsp+32], 3       ; dwCreationDisposition (OPEN_EXISTING)
    mov QWORD PTR [rsp+40], 0       ; dwFlagsAndAttributes
    mov QWORD PTR [rsp+48], 0       ; hTemplateFile
    call CreateFileA

    cmp rax, -1                     ; INVALID_HANDLE_VALUE
    je fn_disk_detail_fail
    mov r14, rax                    ; r14 = Handle

    ; 3. STORAGE_PROPERTY_QUERY 구조체 준비 (12바이트 전부 0으로 초기화, StorageDeviceProperty = 0)
    mov DWORD PTR [rbp-80], 0       ; PropertyId
    mov DWORD PTR [rbp-76], 0       ; QueryType
    mov DWORD PTR [rbp-72], 0       ; Reserved / AdditionalParameters 패딩

    ; 4. DeviceIoControl 호출 (IOCTL_STORAGE_QUERY_PROPERTY = 0x2D1400)
    mov rcx, r14                    ; hDevice
    mov edx, 02D1400h               ; dwIoControlCode
    lea r8, [rbp-80]                ; lpInBuffer
    mov r9d, 12                     ; nInBufferSize
    mov QWORD PTR [rsp+32], r12     ; lpOutBuffer
    mov QWORD PTR [rsp+40], r13     ; nOutBufferSize
    lea rax, [rbp-88]
    mov QWORD PTR [rsp+48], rax     ; lpBytesReturned
    mov QWORD PTR [rsp+56], 0       ; lpOverlapped
    call DeviceIoControl

    mov r15, rax                    ; 결과 저장

    ; 5. CloseHandle
    mov rcx, r14
    call CloseHandle

    test r15, r15
    jz fn_disk_detail_fail

    mov rax, 1
    jmp fn_disk_detail_exit

fn_disk_detail_fail:
    xor rax, rax

fn_disk_detail_exit:
    add rsp, 104
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
get_disk_detail_info_64 ENDP


;***************************************************************************
; @brief   디스크 전체 용량(바이트) 취득 (x64, IOCTL_DISK_GET_LENGTH_INFO)
; @param   RCX - drive_index
; @param   RDX - out_uint64 (unsigned __int64* 수신 버퍼)
; @return  RAX - 1: 성공, 0: 실패
;***************************************************************************
get_disk_total_bytes_64 PROC
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 96

    ; rsp = rbp-128 이므로 스택 인자 슬롯([rsp+32]..[rsp+56])은 rbp-96..rbp-65 영역을 차지함.
    ; 지역 변수는 전부 rbp-64 이상(즉 rbp-64..rbp-1)에 배치해 인자 슬롯과 겹치지 않도록 함.
    ; [rbp-64..rbp-46] : 드라이브 경로 버퍼
    ; [rbp-40]         : Handle (8바이트)
    ; [rbp-24]         : LARGE_INTEGER 수신 버퍼 (8바이트)
    ; [rbp-8]          : BytesReturned (8바이트)

    mov r12, rdx                     ; out_buffer ptr 보존

    mov DWORD PTR [rbp-64], 5C2E5C5Ch
    mov DWORD PTR [rbp-60], 73796850h
    mov DWORD PTR [rbp-56], 6C616369h
    mov DWORD PTR [rbp-52], 76697244h
    mov WORD PTR  [rbp-48], 3065h
    mov BYTE PTR  [rbp-46], 0

    mov al, cl
    add al, '0'
    mov BYTE PTR [rbp-47], al

    lea rcx, [rbp-64]                ; lpFileName
    mov edx, 0C0000000h              ; dwDesiredAccess
    mov r8d, 3                       ; dwShareMode
    xor r9, r9                       ; lpSecurityAttributes
    mov QWORD PTR [rsp+32], 3        ; dwCreationDisposition
    mov QWORD PTR [rsp+40], 0        ; dwFlagsAndAttributes
    mov QWORD PTR [rsp+48], 0        ; hTemplateFile
    call CreateFileA

    cmp rax, -1
    je fn_total_bytes_fail
    mov QWORD PTR [rbp-40], rax      ; Handle 저장

    mov rcx, QWORD PTR [rbp-40]      ; hDevice
    mov edx, 07405Ch                 ; IOCTL_DISK_GET_LENGTH_INFO
    xor r8, r8                       ; lpInBuffer = NULL
    xor r9d, r9d                     ; nInBufferSize = 0
    lea rax, [rbp-24]
    mov QWORD PTR [rsp+32], rax      ; lpOutBuffer
    mov QWORD PTR [rsp+40], 8        ; nOutBufferSize (sizeof(LARGE_INTEGER))
    lea rax, [rbp-8]
    mov QWORD PTR [rsp+48], rax      ; lpBytesReturned
    mov QWORD PTR [rsp+56], 0        ; lpOverlapped
    call DeviceIoControl

    mov r13, rax

    mov rcx, QWORD PTR [rbp-40]
    call CloseHandle

    test r13, r13
    jz fn_total_bytes_fail

    mov rax, QWORD PTR [rbp-24]
    mov QWORD PTR [r12], rax

    mov rax, 1
    jmp fn_total_bytes_exit

fn_total_bytes_fail:
    xor rax, rax

fn_total_bytes_exit:
    add rsp, 96
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
get_disk_total_bytes_64 ENDP


; C-Interface 파서 바인딩 (SMBIOS Type 17 RAM 세부 정보용)
; RAM 슬롯 메인 제조사 추출 (Type 17, Offset 0x17)
get_ram_manufacturer PROC
    sub rsp, 40
    mov r8, rcx
    mov r9d, edx
    mov ecx, 17             ; Type 17 (Memory Device)
    mov edx, 23             ; Offset 0x17 (Manufacturer)
    call get_smbios_string_64
    add rsp, 40
    ret
get_ram_manufacturer ENDP

; RAM 슬롯 위치 명칭 추출 (Type 17, Offset 0x10)
get_ram_locator PROC
    sub rsp, 40
    mov r8, rcx
    mov r9d, edx
    mov ecx, 17             ; Type 17
    mov edx, 16             ; Offset 0x10 (Device Locator)
    call get_smbios_string_64
    add rsp, 40
    ret
get_ram_locator ENDP

END
