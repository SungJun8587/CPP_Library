_TEXT	SEGMENT


cpu_id_supported PROC
	
	push rbx         ; save rbx for the caller
	pushfq           ; push rflags on the stack
	pop rax          ; pop them into rax
	mov rbx, rax     ; save to rbx for restoring afterwards
	xor rax, 200000h ; toggle bit 21
	push rax         ; push the toggled rflags
	popfq            ; pop them back into rflags
	pushfq           ; push rflags
	pop rax          ; pop them back into rax
	cmp rax, rbx     ; see if bit 21 was reset
	jz not_supported
	
	mov eax, 1
	jmp exit
	
not_supported:
	xor eax, eax

exit:
	pop rbx
	ret
cpu_id_supported ENDP


cpu_id PROC
    call cpu_id_supported   ; CPUID 지원 여부 확인
    test eax, eax           ; EAX == 0이면 지원 안 함
    jz exit

	; RCX가 가리키는 메모리 주소의 유효성을 확인
    test rcx, rcx           ; EAX의 메모리 주소 NULL 체크
    jz exit                 ; NULL이면 종료
    test rdx, rdx           ; EBX의 메모리 주소 NULL 체크
    jz exit                 ; NULL이면 종료
    test r8, r8             ; ECX의 메모리 주소 NULL 체크
    jz exit                 ; NULL이면 종료
    test r9, r9             ; EDX의 메모리 주소 NULL 체크
    jz exit                 ; NULL이면 종료

    ; RCX가 가리키는 메모리 주소의 유효성을 확인
    mov r15, rcx           ; 첫 번째 인자 (CPUID 요청 및 결과를 저장할 메모리 주소)
    test r15, r15          ; NULL 체크
    jz exit                ; NULL이면 종료

    ; CPUID 실행
    mov eax, [r15]         ; EAX = CPUID 요청값 (예: 0x80000001)
    xor ebx, ebx           ; EBX 초기화
	xor ecx, ecx           ; ECX 초기화
	xor edx, edx           ; EDX 초기화

    cpuid                  ; CPUID 명령어 실행

    ; 결과를 메모리에 저장
    mov [r15], eax				; EAX 결과 저장 (두 번째 인자)
    mov [r15 + 4], ebx          ; EBX 결과 저장 (세 번째 인자)
    mov [r15 + 8], ecx          ; ECX 결과 저장 (네 번째 인자)
    mov [r15 + 12], edx			; EDX 결과 저장 (네 번째 인자)

exit:
    ret
cpu_id ENDP


cpu_vendor PROC

	call cpu_id_supported
	cmp eax, 0
	je exit
	
	mov r14, rcx
	mov r15, rdx

	mov eax, 0
	cpuid

	mov DWORD PTR [r14], eax
	mov DWORD PTR [r15], ebx
	mov DWORD PTR [r15+4], edx
	mov DWORD PTR [r15+8], ecx

exit:
	ret
cpu_vendor ENDP


cpu_brand_part0 PROC
	
	push rbx

	call cpu_id_supported
	cmp eax, 0
	je exit

	mov eax, 80000000h
	cpuid
	cmp eax, 80000004h
	jnge not_supported

	mov eax, 80000002h
	cpuid

	xchg rbx, rax
	shl rax, 32
	shl rbx, 32
	shr rbx, 32
	or  rax, rbx

	jmp exit

not_supported:
	xor rax, rax
	
exit:
	pop rbx
	ret

cpu_brand_part0 ENDP


cpu_brand_part1 PROC
	
	push rbx

	call cpu_id_supported
	cmp eax, 0
	je exit

	mov eax, 80000000h
	cpuid
	cmp eax, 80000004h
	jnge not_supported

	mov eax, 80000002h
	cpuid

	mov rax, rdx
	shl rax, 32
	shl rcx, 32
	shr rcx, 32
	or  rax, rcx

	jmp exit
	

not_supported:
	xor eax, eax
	
exit:
	pop rbx
	ret

cpu_brand_part1 ENDP


cpu_brand_part2 PROC
	
	push rbx

	call cpu_id_supported
	cmp eax, 0
	je exit

	mov eax, 80000000h
	cpuid
	cmp eax, 80000004h
	jnge not_supported

	mov eax, 80000003h
	cpuid

	xchg rbx, rax
	shl rax, 32
	shl rbx, 32
	shr rbx, 32
	or  rax, rbx

	jmp exit
	

not_supported:
	xor eax, eax
	
exit:
	pop rbx
	ret

cpu_brand_part2 ENDP


cpu_brand_part3 PROC
	
	push rbx

	call cpu_id_supported
	cmp eax, 0
	je exit

	mov eax, 80000000h
	cpuid
	cmp eax, 80000004h
	jnge not_supported

	mov eax, 80000003h
	cpuid

	mov rax, rdx
	shl rax, 32
	shl rcx, 32
	shr rcx, 32
	or  rax, rcx

	jmp exit
	

not_supported:
	xor eax, eax
	
exit:
	pop rbx
	ret

cpu_brand_part3 ENDP


cpu_brand_part4 PROC
	
	push rbx

	call cpu_id_supported
	cmp eax, 0
	je exit

	mov eax, 80000000h
	cpuid
	cmp eax, 80000004h
	jnge not_supported

	mov eax, 80000004h
	cpuid

	xchg rbx, rax
	shl rax, 32
	shl rbx, 32
	shr rbx, 32
	or  rax, rbx

	jmp exit
	

not_supported:
	xor eax, eax
	
exit:
	pop rbx
	ret

cpu_brand_part4 ENDP


cpu_brand_part5 PROC
	
	push rbx

	call cpu_id_supported
	cmp eax, 0
	je exit

	mov eax, 80000000h
	cpuid
	cmp eax, 80000004h
	jnge not_supported

	mov eax, 80000004h
	cpuid

	mov rax, rdx
	shl rax, 32
	shl rcx, 32
	shr rcx, 32
	or  rax, rcx

	jmp exit
	

not_supported:
	xor eax, eax
	
exit:
	pop rbx
	ret

cpu_brand_part5 ENDP


cpu_avx PROC

	call cpu_id_supported
	cmp eax, 0
	je exit
	
	push rbx
	
	mov eax, 1
	cpuid
	shr ecx, 28
	and ecx, 1
	mov eax, ecx

	pop rbx

exit:
	ret
cpu_avx ENDP


cpu_avx2 PROC

	call cpu_id_supported
	cmp eax, 0
	je exit
	
	push rbx

	xor rbx, rbx
	
	mov eax, 7
	cpuid
	shr ebx, 5
	and ebx, 1
	mov eax, ebx

	pop rbx

exit:
	ret
cpu_avx2 ENDP


cpu_mmx PROC

	call cpu_id_supported
	cmp eax, 0
	je exit
	
	push rbx
	
	mov eax, 1
	cpuid
	shr edx, 23
	and edx, 1
	mov eax, edx

	pop rbx

exit:
	ret
cpu_mmx ENDP


cpu_sse PROC

	call cpu_id_supported
	cmp eax, 0
	je exit
	
	push rbx
	
	mov eax, 1
	cpuid
	shr edx, 25
	and edx, 1
	mov eax, edx

	pop rbx

exit:
	ret
cpu_sse ENDP


cpu_sse2 PROC

	call cpu_id_supported
	cmp eax, 0
	je exit

	push rbx

	mov eax, 1
	cpuid
	shr edx, 26
	and edx, 1
	mov eax, edx

	pop rbx

exit:
	ret
cpu_sse2 ENDP


cpu_sse3 PROC

	call cpu_id_supported
	cmp eax, 0
	je exit
	
	push rbx

	mov eax, 1
	cpuid
	and ecx, 1
	mov eax, ecx

	pop rbx

exit:
	ret
cpu_sse3 ENDP


cpu_ssse3 PROC

	call cpu_id_supported
	cmp eax, 0
	je exit
	
	push rbx

	mov eax, 1
	cpuid
	shr ecx, 9
	and ecx, 1
	mov eax, ecx

	pop rbx

exit:
	ret
cpu_ssse3 ENDP


cpu_sse41 PROC

	call cpu_id_supported
	cmp eax, 0
	je exit
	
	push rbx

	mov eax, 1
	cpuid
	shr ecx, 19
	and ecx, 1
	mov eax, ecx

	pop rbx

exit:
	ret
cpu_sse41 ENDP


cpu_sse42 PROC

	call cpu_id_supported
	cmp eax, 0
	je exit
	
	push rbx

	mov eax, 1
	cpuid
	shr ecx, 20
	and ecx, 1
	mov eax, ecx

	pop rbx

exit:
	ret
cpu_sse42 ENDP


cpu_logical_processor_count PROC

	call cpu_id_supported
	cmp eax, 0
	je exit

	call cpu_hyperthreading
	cmp eax, 0
	je exit

	push rbx

	mov eax, 1
	cpuid
	and ebx, 00FF0000h
	mov eax, ebx
	shr eax, 16

	pop rbx

exit:
	ret
cpu_logical_processor_count ENDP


cpu_hyperthreading PROC

	call cpu_id_supported
	cmp eax, 0
	je exit
	
	push rbx
	
	mov eax, 1
	cpuid
	shr edx, 28
	and edx, 1
	mov eax, edx

	pop rbx

exit:
	ret
cpu_hyperthreading ENDP


_TEXT	ENDS
END