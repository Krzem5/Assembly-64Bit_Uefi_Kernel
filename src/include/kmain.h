#ifndef __KERNEL_KMAIN_H__
#define __KERNEL_KMAIN_H__ 1
#ifndef KERNEL_ARGS_STRUCT_ONLY
#include <libc/stdint.h>
#endif



typedef struct __KERNEL_ARGS_MEM_ENTRY{
	uint64_t b;
	uint64_t l;
} KernelArgsMemEntry;
typedef struct __KERNEL_ARGS{
	uint32_t* vmem;
	uint64_t vmem_l;
	uint64_t vmem_w;
	uint64_t vmem_h;
	void* acpi;
	void* idt;
	uint64_t* pml4;
	uint64_t t_pg;
	uint64_t u_pg;
	uint64_t k_pg;
	uint64_t mmap_l;
	KernelArgsMemEntry mmap[];
} KernelArgs;



#ifdef KERNEL_ARGS_STRUCT_ONLY
typedef void __attribute__((ms_abi)) (*kmain)(KernelArgs* ka);
#else
void __attribute__((ms_abi)) kmain(KernelArgs* ka);
#endif



#endif
