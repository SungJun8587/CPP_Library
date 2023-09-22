TITLE 'SEE HEADER FILE FOR DECLARATIONS'
.686P
.model FLAT

PUBLIC	_cpu_id_supported@0
PUBLIC	_cpu_id@16
PUBLIC  _cpu_vendor@8
PUBLIC	_cpu_brand@4
PUBLIC	_cpu_avx@0
PUBLIC	_cpu_avx2@0
PUBLIC	_cpu_mmx@0
PUBLIC	_cpu_sse@0
PUBLIC	_cpu_sse2@0
PUBLIC	_cpu_sse3@0
PUBLIC	_cpu_ssse3@0
PUBLIC	_cpu_sse41@0
PUBLIC	_cpu_sse42@0
PUBLIC	_cpu_logical_processor_count@0
PUBLIC	_cpu_hyperthreading@0

_TEXT	SEGMENT

_cpu_id_supported@0 PROC NEAR
	
	push ebx         ; save ebx for the caller
	pushfd           ; push eflags on the stack
	pop eax          ; pop them into eax
	mov ebx, eax     ; save to ebx for restoring afterwards
	xor eax, 200000h ; toggle bit 21
	push eax         ; push the toggled eflags
	popfd            ; pop them back into eflags
	pushfd           ; push eflags
	pop eax          ; pop them back into eax
	cmp eax, ebx     ; see if bit 21 was reset
	jz not_supported
	
	mov eax, 1
	jmp exit
	
not_supported:
	xor eax, eax

exit:
	pop ebx
	ret 0
_cpu_id_supported@0 ENDP


_cpu_id@16 PROC NEAR

	call _cpu_id_supported@0
	cmp eax, 0
	je exit

	push ebx
	push esi

	mov esi, [esp+(1+2)*4] ; parameter: eax
	mov eax, [esi]
	
	mov esi, [esp+(2+2)*4] ; parameter: ebx
	mov ebx, [esi]
	
	mov esi, [esp+(3+2)*4] ; parameter: ecx
	mov ecx, [esi]
	
	mov esi, [esp+(4+2)*4] ; parameter: edx
	mov edx, [esi]

	cpuid

	mov [esi], edx
	
	mov esi, [esp+(3+2)*4] ; parameter: ecx
	mov [esi], ecx
	
	mov esi, [esp+(2+2)*4] ; parameter: ebx
	mov [esi], ebx
	
	mov esi, [esp+(1+2)*4] ; parameter: eax
	mov [esi], eax

	pop esi
	pop ebx

exit:
	ret 16
_cpu_id@16 ENDP


_cpu_vendor@8 PROC NEAR
	
	push ebx
	push esi

	call _cpu_id_supported@0
	cmp eax, 0
	je exit

	mov eax, 0
	cpuid

	mov esi, [esp+(1+2)*4] ; parameter: eax
	mov [esi], eax 

	mov esi, [esp+(2+2)*4] ; parameter: vendorname
	mov [esi], ebx
	mov [esi+4], edx
	mov [esi+8], ecx

	jmp exit

not_supported:
	xor eax, eax
	
exit:
	pop esi
	pop ebx
	ret 8
_cpu_vendor@8 ENDP


_cpu_brand@4 PROC NEAR
	
	push ebx
	push esi

	call _cpu_id_supported@0
	cmp eax, 0
	je exit

	mov eax, 80000000h
	cpuid
	cmp eax, 80000004h
	jnge not_supported

	mov esi, [esp+(1+2)*4]

	mov eax, 80000002h
	cpuid
	mov [esi], eax
	mov [esi+4], ebx
	mov [esi+8], ecx
	mov [esi+12], edx

	mov eax, 80000003h
	cpuid
	mov [esi+16], eax
	mov [esi+20], ebx
	mov [esi+24], ecx
	mov [esi+28], edx

	mov eax, 80000004h
	cpuid
	mov [esi+32], eax
	mov [esi+36], ebx
	mov [esi+40], ecx
	mov [esi+44], edx

	mov eax, 1
	jmp exit
	

not_supported:
	xor eax, eax
	
exit:
	pop esi
	pop ebx
	ret 4
_cpu_brand@4 ENDP


_cpu_avx@0 PROC NEAR

	call _cpu_id_supported@0
	cmp eax, 0
	je exit
	
	push ebx
	
	mov eax, 1
	cpuid
	shr ecx, 28
	and ecx, 1
	mov eax, ecx

	pop ebx

exit:
	ret 0
_cpu_avx@0 ENDP


_cpu_avx2@0 PROC NEAR

	call _cpu_id_supported@0
	cmp eax, 0
	je exit
	
	push ebx
	
	mov eax, 7
	xor ecx, ecx
	cpuid
	shr ebx, 5
	and ebx, 1
	mov eax, ebx

	pop ebx

exit:
	ret 0
_cpu_avx2@0 ENDP


_cpu_mmx@0 PROC NEAR

	call _cpu_id_supported@0
	cmp eax, 0
	je exit
	
	push ebx
	
	mov eax, 1
	cpuid
	shr edx, 23
	and edx, 1
	mov eax, edx

	pop ebx

exit:
	ret 0
_cpu_mmx@0 ENDP


_cpu_sse@0 PROC NEAR

	call _cpu_id_supported@0
	cmp eax, 0
	je exit
	
	push ebx
	
	mov eax, 1
	cpuid
	shr edx, 25
	and edx, 1
	mov eax, edx

	pop ebx

exit:
	ret 0
_cpu_sse@0 ENDP


_cpu_sse2@0 PROC NEAR

	call _cpu_id_supported@0
	cmp eax, 0
	je exit

	push ebx

	mov eax, 1
	cpuid
	shr edx, 26
	and edx, 1
	mov eax, edx

	pop ebx

exit:
	ret 0
_cpu_sse2@0 ENDP


_cpu_sse3@0 PROC NEAR

	call _cpu_id_supported@0
	cmp eax, 0
	je exit
	
	push ebx

	mov eax, 1
	cpuid
	and ecx, 1
	mov eax, ecx

	pop ebx

exit:
	ret 0
_cpu_sse3@0 ENDP


_cpu_ssse3@0 PROC NEAR

	call _cpu_id_supported@0
	cmp eax, 0
	je exit
	
	push ebx

	mov eax, 1
	cpuid
	shr ecx, 9
	and ecx, 1
	mov eax, ecx

	pop ebx

exit:
	ret 0
_cpu_ssse3@0 ENDP


_cpu_sse41@0 PROC NEAR

	call _cpu_id_supported@0
	cmp eax, 0
	je exit
	
	push ebx

	mov eax, 1
	cpuid
	shr ecx, 19
	and ecx, 1
	mov eax, ecx

	pop ebx

exit:
	ret 0
_cpu_sse41@0 ENDP


_cpu_sse42@0 PROC NEAR

	call _cpu_id_supported@0
	cmp eax, 0
	je exit
	
	push ebx

	mov eax, 1
	cpuid
	shr ecx, 20
	and ecx, 1
	mov eax, ecx

	pop ebx

exit:
	ret 0
_cpu_sse42@0 ENDP


_cpu_logical_processor_count@0 PROC NEAR

	call _cpu_id_supported@0
	cmp eax, 0
	je exit
	
	call _cpu_hyperthreading@0
	cmp eax, 0
	je exit
	
	push ebx

	mov eax, 1
	cpuid
	shr ebx, 16
	and ebx, 0FFh
	mov eax, ebx

	pop ebx

exit:
	ret 0
_cpu_logical_processor_count@0 ENDP


_cpu_hyperthreading@0 PROC NEAR

	call _cpu_id_supported@0
	cmp eax, 0
	je exit
	
	push ebx

	mov eax, 1
	cpuid
	shr edx, 28 ; bit 28
	and edx, 1
	mov eax, edx

	pop ebx

exit:
	ret 0
_cpu_hyperthreading@0 ENDP


_TEXT	ENDS
END