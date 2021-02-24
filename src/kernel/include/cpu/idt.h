#ifndef __KERNEL_CPU_IDT_H__
#define __KERNEL_CPU_IDT_H__ 1
#include <shared.h>
#include <kmain.h>
#include <stdint.h>



typedef struct __REGISTERS{
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t rbp;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
	uint64_t t;
	uint64_t ec;
	uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
} __attribute__((packed)) registers_t;
typedef struct __IDT_TABLE{
	uint16_t l;
	uint64_t b;
} __attribute__((packed)) idt_table_t;
typedef struct __IDT_ENTRY{
	uint16_t off16;
	uint16_t s;
	uint8_t z8;
	uint8_t f;
	uint16_t off32;
	uint32_t off64;
	uint32_t z32;
} __attribute__((packed)) idt_entry_t;



extern void asm_setup_idt(void);



void KERNEL_CALL setup_idt(KernelArgs* ka);



void KERNEL_CALL set_idt_entry(uint8_t i,void* a,uint16_t s,uint8_t f);



#endif
