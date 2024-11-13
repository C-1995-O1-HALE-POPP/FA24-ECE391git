// main.c - Main function
//

#include "console.h"
#include "serial.h"
#include "intr.h"
#include "thread.h"
#include "timer.h"
#include "heap.h"

extern void trek_main(int comno);
extern void rule30_main(int comno);

static void trek_start(void * aux);
static void rule30_start(void * aux);

extern char _kimg_end[]; // end of kernel image (defined in kernel.ld)

#ifndef RAM_SIZE
#ifdef RAM_SIZE_MB
#define RAM_SIZE (RAM_SIZE_MB*1024*1024)
#else
#define RAM_SIZE (8*1024*1024)
#endif
#endif

#ifndef RAM_START
#define RAM_START 0x80000000UL
#endif

void main(void) {
	int trek_tid;

	console_init();
	intr_init();
	thread_init();
	timer_init();

	heap_init(_kimg_end, (void*)RAM_START+RAM_SIZE);

	com_init_async(1);
	com_init_async(2);

	intr_enable();
	timer_start();

	// Spawn a thread for trek and rule30
	
	trek_tid = thread_spawn("trek", trek_start, NULL);
	thread_spawn("rule30", rule30_start, NULL);

	// Wait for trek to exit

	thread_join(trek_tid);
}

void trek_start(void * aux __attribute__ ((unused))) {
	trek_main(1); // COM1
}

void rule30_start(void * aux __attribute__ ((unused))) {
	rule30_main(2); // COM2
}