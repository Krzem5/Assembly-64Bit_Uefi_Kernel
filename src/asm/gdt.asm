bits 64
global asm_setup_gdt



asm_setup_gdt:
	lgdt [gdt64_pointer]
	mov ax, gdt64_data
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	push byte gdt64_code
	push ._new_cs
	retfq
._new_cs:
	ret



section .data
	gdt64_start:
		dq 0x000100000000ffff
	gdt64_code: equ $-gdt64_start
		dq 0x00af9a0000000000
	gdt64_data: equ $-gdt64_start
		dq 0x0000920000000000
	gdt64_pointer:
		dw $-gdt64_start-1
		dq gdt64_start
