// main.c - Main function: runs shell to load executable
//

#ifdef MAIN_TRACE
#define TRACE
#endif

#ifdef MAIN_DEBUG
#define DEBUG
#endif

#define INIT_PROC "run_me" // name of init process executable

#include "console.h"
#include "thread.h"
#include "device.h"
#include "uart.h"
#include "timer.h"
#include "intr.h"
#include "memory.h"
#include "heap.h"
#include "virtio.h"
#include "halt.h"
#include "elf.h"
#include "fs.h"
#include "string.h"
#include "process.h"
#include "config.h"


void hello(void) {
    kprintf("Hello, World!\n");
}

void main(void) {

    console_init();
    memory_init();
    void (*ptr)(void);
    ptr = hello;
    kprintf("here we have a hello program at %p, it says: ", ptr);
    ptr();
    kprintf("now we makes it unexcutable! Run it again: ");
    memory_set_page_flags(ptr, PTE_R);
    ptr();

}
