#include <shared.h>
#include <cpu/acpi.h>
#include <cpu/gdt.h>
#include <cpu/idt.h>
#include <cpu/irq.h>
#include <cpu/isr.h>
#include <gfx/console.h>
#include <gfx/gfx.h>
#include <kmain.h>
#include <memory/paging.h>
#include <memory/pm.h>
#include <memory/vm.h>
#include <process/scheduler.h>
#include <process/thread.h>
#include <stddef.h>
#include <stdint.h>



void KERNEL_CALL thread1(void* ta){
	console_log("Thread1!\n");
}



void KERNEL_CALL thread2(void* ta){
	console_log("Thread2!\n");
}



void KERNEL_CALL kmain(KernelArgs* ka){
	paging_init(ka);
	vm_init(ka);
	pm_init(ka);
	vm_after_pm_init(ka);
	gfx_init(ka);
	console_init(ka);
	console_log("Starting System...\n");
	console_log("Memory Map (%llu):\n",ka->mmap_l);
	for (uint64_t i=0;i<ka->mmap_l;i++){
		console_log("  %llx - +%llx",ka->mmap[i].b&0xfffffffffffffffe,ka->mmap[i].l);
		if (ka->mmap[i].b&1){
			console_log(" (ACPI Tables)");
		}
		console_log("\n");
	}
	console_log("Setting Up GDT...\n");
	asm_setup_gdt();
	console_log("Setting Up IDT...\n");
	setup_idt(ka);
	console_log("Setting Up Default ISRs...\n");
	setup_isr();
	console_log("Setting Up Default IRQs...\n");
	setup_irq();
	console_log("Enabling IDT...\n");
	enable_idt();
	console_log("Setting Up ACPI...\n");
	acpi_init(ka);
	console_log("Setting Up Scheduler...\n");
	scheduler_init();
	console_log("Registering Kernel Threads...\n");
	create_thread(thread1,THREAD_PRIORITY_NORMAL,NULL);
	create_thread(thread2,THREAD_PRIORITY_NORMAL,NULL);
	console_ok("Starting Scheduler...\n");
	scheduler_start();
}