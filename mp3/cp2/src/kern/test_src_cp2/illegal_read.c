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

void main(void) {

    console_init();
    memory_init();
    char* hello = memory_alloc_and_map_page(0xc0001000, PTE_R | PTE_W);
    memcpy(hello, "Hello, World!\n",14);
    kprintf("here we have a string at %p, it says: %s", hello, hello);
    memory_set_page_flags(hello, 0);
    kprintf("now we makes it unreadable! it will say: %s", hello);

}
