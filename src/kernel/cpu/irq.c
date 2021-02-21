#include <shared.h>
#include <cpu/idt.h>
#include <cpu/irq.h>
#include <cpu/ports.h>
#include <gfx/console.h>
#include <stddef.h>
#include <stdint.h>



irq_handler_t irq_hl[16]={NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};



void KERNEL_CALL _handle_irq(registers_t* r){
	console_warn("IRQ:\n  rax    = %#.18llx\n  rbx    = %#.18llx\n  rcx    = %#.18llx\n  rdx    = %#.18llx\n  rsi    = %#.18llx\n  rdi    = %#.18llx\n  rbp    = %#.18llx\n  r8     = %#.18llx\n  r9     = %#.18llx\n  r10    = %#.18llx\n  r11    = %#.18llx\n  r12    = %#.18llx\n  r13    = %#.18llx\n  r14    = %#.18llx\n  r15    = %#.18llx\n  t      = %#.4llx\n  ec     = %#.4llx\n  rip    = %#.18llx\n  cs     = %#.6llx\n  rflags = %#.8llx\n  rsp    = %#.18llx\n  ss     = %#.6llx\n",r->rax,r->rbx,r->rcx,r->rdx,r->rsi,r->rdi,r->rbp,r->r8,r->r9,r->r10,r->r11,r->r12,r->r13,r->r14,r->r15,r->t,r->ec,r->rip,r->cs,r->rflags,r->rsp,r->ss);
	if (irq_hl[r->t-0x20]){
		irq_hl[r->t-0x20](r);
	}
	if (r->t>=0x28){
		port_out(0xa0,0x20);
	}
	port_out(0x20,0x20);
}



void KERNEL_CALL setup_irq(void){
	port_out(0x20,0x11);
	port_out(0xa0,0x11);
	port_out(0x21,0x20);
	port_out(0xa1,0x28);
	port_out(0x21,0x04);
	port_out(0xa1,0x02);
	port_out(0x21,0x01);
	port_out(0xa1,0x01);
	port_out(0x21,0x0);
	port_out(0xa1,0x0);
	set_idt_entry(32,_asm_irq0,0x08,0x8e);
	set_idt_entry(33,_asm_irq1,0x08,0x8e);
	set_idt_entry(34,_asm_irq2,0x08,0x8e);
	set_idt_entry(35,_asm_irq3,0x08,0x8e);
	set_idt_entry(36,_asm_irq4,0x08,0x8e);
	set_idt_entry(37,_asm_irq5,0x08,0x8e);
	set_idt_entry(38,_asm_irq6,0x08,0x8e);
	set_idt_entry(39,_asm_irq7,0x08,0x8e);
	set_idt_entry(40,_asm_irq8,0x08,0x8e);
	set_idt_entry(41,_asm_irq9,0x08,0x8e);
	set_idt_entry(42,_asm_irq10,0x08,0x8e);
	set_idt_entry(43,_asm_irq11,0x08,0x8e);
	set_idt_entry(44,_asm_irq12,0x08,0x8e);
	set_idt_entry(45,_asm_irq13,0x08,0x8e);
	set_idt_entry(46,_asm_irq14,0x08,0x8e);
	set_idt_entry(47,_asm_irq15,0x08,0x8e);
}



void KERNEL_CALL regiser_irq_handler(uint8_t i,irq_handler_t h){
	irq_hl[i]=h;
}



void KERNEL_CALL unregiser_irq_handler(uint8_t i){
	irq_hl[i]=NULL;
}
