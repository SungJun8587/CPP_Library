.MODEL FLAT, C
.STACK 4096

; Windows API 외장 함수 선언 (x86 stdcall 바인딩)
EXTERN CreateFileA@28 : PROC
EXTERN DeviceIoControl@32 : PROC
EXTERN CloseHandle@4 : PROC
EXTERN get_smbios_string_32 : PROC

.CODE

;***************************************************************************
; @brief   디스크 물리 상세 정보(모델명/시리얼/인터페이스) 취득 (x86 32비트)
; @param   [ESP+4] - drive_index (0: 첫번째 디스크, 1: 두번째 ...)
; @param   [ESP+8] - out_buffer  (STORAGE_DEVICE_DESCRIPTOR 파싱 데이터 수신 버퍼)
; @param   [ESP+12]- buffer_size
; @return  EAX - 1: 성공, 0: 실패
;***************************************************************************
get_disk_detail_info_32 PROC
    push ebp
    mov ebp, esp
    sub esp, 40                     ; 지역 변수 공간
    push ebx
    push esi
    push edi

    ; [ebp-40..ebp-29] : STORAGE_PROPERTY_QUERY (12바이트, PropertyId/QueryType/Reserved)
    ; [ebp-28]         : BytesReturned
    ; [ebp-24]         : CreateFile 핸들
    ; [ebp-20..ebp-2]  : 드라이브 경로 버퍼 ("\\.\PhysicalDrive0")
    ; 위 4개 영역은 서로 겹치지 않도록 재배치함 (기존 코드는 Query 구조체와 Handle 변수가 겹치는 버그가 있었음)

    ; 1. DevicePath 생성 ("\\.\PhysicalDrive0")
    mov DWORD PTR [ebp-20], 5C2E5C5Ch ; "\\.\ " (리틀엔디안 보정)
    mov DWORD PTR [ebp-16], 73796850h ; "Phys"
    mov DWORD PTR [ebp-12], 6C616369h ; "ical"
    mov DWORD PTR [ebp-8],  76697244h ; "Driv" (리틀엔디안 보정)
    mov WORD PTR  [ebp-4],  3065h     ; "e0"
    mov BYTE PTR  [ebp-2],  0         ; Null

    ; drive_index 반영 ('0' + index)
    mov al, BYTE PTR [ebp+8]
    add al, '0'
    mov BYTE PTR [ebp-3], al

    ; 2. CreateFileA 호출
    push 0                          ; hTemplateFile = NULL
    push 0                          ; dwFlagsAndAttributes = 0
    push 3                          ; dwCreationDisposition = OPEN_EXISTING (3)
    push 0                          ; lpSecurityAttributes = NULL
    push 3                          ; dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE (3)
    push 0C0000000h                 ; dwDesiredAccess = GENERIC_READ | GENERIC_WRITE
    lea eax, [ebp-20]
    push eax                        ; lpFileName
    call CreateFileA@28

    cmp eax, -1                     ; INVALID_HANDLE_VALUE
    je fn_disk_detail_fail
    mov [ebp-24], eax               ; Handle 저장

    ; 3. STORAGE_PROPERTY_QUERY 구조체 준비 (12바이트 전부 0으로 초기화)
    mov DWORD PTR [ebp-40], 0       ; PropertyId = StorageDeviceProperty (0)
    mov DWORD PTR [ebp-36], 0       ; QueryType = PropertyStandardQuery (0)
    mov DWORD PTR [ebp-32], 0       ; Reserved / AdditionalParameters 패딩

    ; 4. DeviceIoControl 호출 (IOCTL_STORAGE_QUERY_PROPERTY = 0x2D1400)
    push 0                          ; lpOverlapped
    lea eax, [ebp-28]
    push eax                        ; lpBytesReturned
    push DWORD PTR [ebp+16]         ; nOutBufferSize
    push DWORD PTR [ebp+12]         ; lpOutBuffer
    push 12                         ; nInBufferSize (sizeof(STORAGE_PROPERTY_QUERY))
    lea eax, [ebp-40]
    push eax                        ; lpInBuffer
    push 02D1400h                   ; dwIoControlCode
    push DWORD PTR [ebp-24]         ; hDevice
    call DeviceIoControl@32

    mov ebx, eax                    ; 결과 저장

    ; 5. CloseHandle
    push DWORD PTR [ebp-24]
    call CloseHandle@4

    test ebx, ebx
    jz fn_disk_detail_fail

    mov eax, 1
    jmp fn_disk_detail_exit

fn_disk_detail_fail:
    xor eax, eax

fn_disk_detail_exit:
    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret
get_disk_detail_info_32 ENDP


;***************************************************************************
; @brief   디스크 전체 용량(바이트) 취득 (x86 32비트, IOCTL_DISK_GET_LENGTH_INFO)
; @param   [ESP+4] - drive_index
; @param   [ESP+8] - out_uint64 (8바이트 unsigned __int64* 수신 버퍼)
; @return  EAX - 1: 성공, 0: 실패
;***************************************************************************
get_disk_total_bytes_32 PROC
    push ebp
    mov ebp, esp
    sub esp, 40
    push ebx
    push esi
    push edi

    ; [ebp-36..ebp-29] : LARGE_INTEGER 수신 버퍼 (8바이트)
    ; [ebp-28]         : BytesReturned
    ; [ebp-24]         : Handle
    ; [ebp-20..ebp-2]  : 드라이브 경로 버퍼

    mov DWORD PTR [ebp-20], 5C2E5C5Ch
    mov DWORD PTR [ebp-16], 73796850h
    mov DWORD PTR [ebp-12], 6C616369h
    mov DWORD PTR [ebp-8],  76697244h
    mov WORD PTR  [ebp-4],  3065h
    mov BYTE PTR  [ebp-2],  0

    mov al, BYTE PTR [ebp+8]
    add al, '0'
    mov BYTE PTR [ebp-3], al

    push 0
    push 0
    push 3
    push 0
    push 3
    push 0C0000000h
    lea eax, [ebp-20]
    push eax
    call CreateFileA@28

    cmp eax, -1
    je fn_total_bytes_fail
    mov [ebp-24], eax

    push 0                          ; lpOverlapped
    lea eax, [ebp-28]
    push eax                        ; lpBytesReturned
    push 8                          ; nOutBufferSize (sizeof(LARGE_INTEGER))
    lea eax, [ebp-36]
    push eax                        ; lpOutBuffer
    push 0                          ; nInBufferSize
    push 0                          ; lpInBuffer
    push 07405Ch                    ; IOCTL_DISK_GET_LENGTH_INFO
    push DWORD PTR [ebp-24]         ; hDevice
    call DeviceIoControl@32

    mov ebx, eax

    push DWORD PTR [ebp-24]
    call CloseHandle@4

    test ebx, ebx
    jz fn_total_bytes_fail

    mov edx, DWORD PTR [ebp+12]     ; out_uint64 ptr
    mov eax, DWORD PTR [ebp-36]     ; low dword
    mov DWORD PTR [edx], eax
    mov eax, DWORD PTR [ebp-32]     ; high dword
    mov DWORD PTR [edx+4], eax

    mov eax, 1
    jmp fn_total_bytes_exit

fn_total_bytes_fail:
    xor eax, eax

fn_total_bytes_exit:
    pop edi
    pop esi
    pop ebx
    mov esp, ebp
    pop ebp
    ret
get_disk_total_bytes_32 ENDP


;***************************************************************************
; Public C-Interface 파서 바인딩 (RAM SMBIOS Type 17 정보)
;***************************************************************************

; RAM 슬롯 메인 제조사 추출 (Type 17, Offset 0x17)
get_ram_manufacturer PROC
    push ebp
    mov ebp, esp
    push DWORD PTR [ebp+12]         ; buffer_size
    push DWORD PTR [ebp+8]          ; buffer
    push 23                         ; Offset 0x17 (Manufacturer)
    push 17                         ; Type 17 (Memory Device)
    call get_smbios_string_32
    add esp, 16
    pop ebp
    ret
get_ram_manufacturer ENDP

; RAM 슬롯 위치 명칭 추출 (Type 17, Offset 0x10)
get_ram_locator PROC
    push ebp
    mov ebp, esp
    push DWORD PTR [ebp+12]         ; buffer_size
    push DWORD PTR [ebp+8]          ; buffer
    push 16                         ; Offset 0x10 (Device Locator)
    push 17                         ; Type 17
    call get_smbios_string_32
    add esp, 16
    pop ebp
    ret
get_ram_locator ENDP

END
