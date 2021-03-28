#ifndef __KERNEL_PROCESS_THREAD_H__
#define __KERNEL_PROCESS_THREAD_H__ 1
#include <shared.h>
#include <process/process.h>
#include <stdint.h>



typedef uint32_t tid_t;
typedef struct __THREAD_DATA{
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t rsp;
	uint64_t rbp;
	uint64_t rip;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
	uint64_t rflags;
	uint64_t cs;
	uint64_t ss;
} thread_data_t;
typedef struct __THREAD{
	uint8_t f;
	tid_t id;
	thread_data_t dt;
	process_t* p;
} thread_t;
typedef void (KERNEL_CALL *thread_start_t)(void* arg);



extern void KERNEL_CALL asm_clear_thread_data(thread_data_t* dt);



void KERNEL_CALL KERNEL_UNMAP_AFTER_LOAD thread_init(void);



thread_t* KERNEL_CALL create_thread(process_t* p,thread_start_t e,void* a);



#endif
