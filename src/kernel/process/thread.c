#include <shared.h>
#include <cpu/fatal_error.h>
#include <gfx/console.h>
#include <memory/vm.h>
#include <process/process.h>
#include <process/thread.h>



#define PAGE_4KB_POWER_OF_2 12
#define THREAD_4KB_PAGES_COUNT 64
#define MAX_THREAD_ID ((THREAD_4KB_PAGES_COUNT<<PAGE_4KB_POWER_OF_2)/sizeof(thread_t)-1)
#define THREAD_GET_PRESENT(p) ((p)->f&1)
#define THREAD_SET_PRESENT 1



thread_t* tl;
uint32_t _nti=0;



void KERNEL_CALL thread_init(void){
	tl=(thread_t*)(void*)vm_commit(THREAD_4KB_PAGES_COUNT);
	for (uint32_t i=0;i<MAX_THREAD_ID+1;i++){
		(tl+i)->f=0;
		(tl+i)->id=i;
	}
	console_log("Max Thread ID: %llu\n",MAX_THREAD_ID);
}



thread_t* KERNEL_CALL create_thread(process_t* p,thread_start_t a,void* arg){
	thread_t* o=tl+_nti;
	o->f|=THREAD_SET_PRESENT;
	o->p=p;
	o->dt.rcx=(uint64_t)arg;
	o->dt.rip=(uint64_t)a;
	_nti++;
	while (THREAD_GET_PRESENT(tl+_nti)){
		_nti++;
		if (_nti>MAX_THREAD_ID){
			fatal_error("Final Thread Allocated!\n");
		}
	}
	return o;
}
