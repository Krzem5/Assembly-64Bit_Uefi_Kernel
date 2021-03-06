#ifndef __CPU_CPU_INFO_H__
#define __CPU_CPU_INFO_H__ 1
#include <shared.h>
#include <stdint.h>



#define CPU_INFO_FEATURE_FPU 1ull
#define CPU_INFO_FEATURE_VME 2ull
#define CPU_INFO_FEATURE_DE 4ull
#define CPU_INFO_FEATURE_PSE 8ull
#define CPU_INFO_FEATURE_TSC 16ull
#define CPU_INFO_FEATURE_MSR 32ull
#define CPU_INFO_FEATURE_PAE 64ull
#define CPU_INFO_FEATURE_MCE 128ull
#define CPU_INFO_FEATURE_CX8 256ull
#define CPU_INFO_FEATURE_APIC 512ull
#define CPU_INFO_FEATURE_SEP 2048ull
#define CPU_INFO_FEATURE_MTRR 4096ull
#define CPU_INFO_FEATURE_PGE 8192ull
#define CPU_INFO_FEATURE_MCA 16384ull
#define CPU_INFO_FEATURE_CMOV 32768ull
#define CPU_INFO_FEATURE_PAT 65536ull
#define CPU_INFO_FEATURE_PSE36 131072ull
#define CPU_INFO_FEATURE_PSN 262144ull
#define CPU_INFO_FEATURE_CLF 524288ull
#define CPU_INFO_FEATURE_DTES 2097152ull
#define CPU_INFO_FEATURE_ACPI 4194304ull
#define CPU_INFO_FEATURE_MMX 8388608ull
#define CPU_INFO_FEATURE_FXSR 16777216ull
#define CPU_INFO_FEATURE_SSE 33554432ull
#define CPU_INFO_FEATURE_SSE2 67108864ull
#define CPU_INFO_FEATURE_SS 134217728ull
#define CPU_INFO_FEATURE_HTT 268435456ull
#define CPU_INFO_FEATURE_TM1 536870912ull
#define CPU_INFO_FEATURE_IA64 1073741824ull
#define CPU_INFO_FEATURE_PBE 2147483648ull
#define CPU_INFO_FEATURE_SSE3 4294967296ull
#define CPU_INFO_FEATURE_PCLMUL 4294967296ull
#define CPU_INFO_FEATURE_DTES64 17179869184ull
#define CPU_INFO_FEATURE_MONITOR 34359738368ull
#define CPU_INFO_FEATURE_DS_CPL 68719476736ull
#define CPU_INFO_FEATURE_VMX 137438953472ull
#define CPU_INFO_FEATURE_SMX 274877906944ull
#define CPU_INFO_FEATURE_EST 549755813888ull
#define CPU_INFO_FEATURE_TM2 1099511627776ull
#define CPU_INFO_FEATURE_SSSE3 2199023255552ull
#define CPU_INFO_FEATURE_CID 4398046511104ull
#define CPU_INFO_FEATURE_FMA 17592186044416ull
#define CPU_INFO_FEATURE_CX16 35184372088832ull
#define CPU_INFO_FEATURE_ETPRD 70368744177664ull
#define CPU_INFO_FEATURE_PDCM 140737488355328ull
#define CPU_INFO_FEATURE_PCIDE 562949953421312ull
#define CPU_INFO_FEATURE_DCA 1125899906842624ull
#define CPU_INFO_FEATURE_SSE4_1 2251799813685248ull
#define CPU_INFO_FEATURE_SSE4_2 4503599627370496ull
#define CPU_INFO_FEATURE_x2APIC 9007199254740992ull
#define CPU_INFO_FEATURE_MOVBE 18014398509481984ull
#define CPU_INFO_FEATURE_POPCNT 36028797018963968ull
#define CPU_INFO_FEATURE_AES 144115188075855872ull
#define CPU_INFO_FEATURE_XSAVE 288230376151711744ull
#define CPU_INFO_FEATURE_OSXSAVE 576460752303423488ull
#define CPU_INFO_FEATURE_AVX 1152921504606846976ull



typedef struct __CPUID_INFO{
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
} cpuid_t;



typedef struct __CPU_INFO{
	uint32_t mx;
	char id[13];
	uint64_t f;
	char nm[48];
} cpu_info_t;



extern cpu_info_t cpu_info_data;



void KERNEL_CALL cpu_info_init(void);



extern void KERNEL_CALL asm_cpuid(uint32_t t,cpuid_t* o);



#endif
