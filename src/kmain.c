#include <kmain.h>
#include <types.h>
#include <gfx.h>
#include <heap.h>
#include <driver/console.h>
#include <cpu/gdt.h>
#include <cpu/idt.h>
#include <cpu/isr.h>
#include <cpu/irq.h>



void __attribute__((ms_abi)) kmain(KernelArgs ka){
	gfx_init(ka);
	console_init(ka);
	console_log("Starting System...");
	console_log("Memory Map:");
	for (uint64_t i=0;i<ka.mmap_l;i++){
		console_continue("  d");
	}
	console_log("Initialising Heap...");
	init_heap();
	console_log("Setting Up GDT...");
	asm_setup_gdt();
	console_log("Setting Up IDT...");
	setup_idt();
	console_log("Setting Up Default ISRs...");
	setup_isr();
	console_log("Setting Up Default IRQs...");
	setup_irq();
	console_log("Setting Up IRQ Handlers...");
	// setup_keyboard();
	// setup_timer();
	console_log("Enabling IDT...");
	enable_idt();
	console_ok("Reached the End!");
	for (;;){
		__asm__ volatile("cli");
	}
}