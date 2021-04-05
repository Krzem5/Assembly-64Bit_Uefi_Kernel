bits 64
global asm_setup_idt
extern idt



section .unmap
asm_setup_idt:
	push rdi
	xor rax, rax
	mov rcx, (__C_IDT_ENTRY_STRUCT_SIZE__ * __C_TOTAL_INTERRUPT_NUMBER__ / __C_SIZEOF_UINT64_T__)
	mov rdi, qword [idt+__C_IDT_TABLE_STRUCT_B_OFFSET__]
	rep stosq
	lidt [idt]
	pop rdi
	ret
