// halt.c
//

#include "halt.h"
#include "trap.h"
#include "console.h"

#include <stdint.h>

// The halt_success and halt_failure functions use the virt test device to
// terminate. Will not work on real hardware.

void halt_success(void) {
    *(int*)0x100000 = 0x5555; // success
    for (;;) continue; // just in case
}

void halt_failure(void) {
    *(int*)0x100000 = 0x3333; // failure
    for (;;) continue; // just in case
}

void panic(const char * msg) {
    if (msg != NULL)
        console_puts(msg);
    
    halt_failure();
}

// except_names() is called from trap.s by _trap_entry

static const char * const except_names[] = {
    [0] = "Misaligned instruction address",
    [1] = "Instruction access fault",
    [2] = "Illegal instruction",
    [3] = "Breakpoint",
    [4] = "Misaligned load address",
    [5] = "Load access fault",
    [6] = "Misaligned store address",
    [7] = "Store access fault"
};


void except_handler(int code, struct trap_frame * tfr) {
    const char * name = NULL;

    if (0 <= code && code < sizeof(except_names)/sizeof(except_names[0]))
        name = except_names[code];
    
    if (name == NULL)
        kprintf("Fault %d at %p\n", code, (void*)tfr->mepc);
    else
        kprintf("%s at %p\n", name, (void*)tfr->mepc);
    
    panic(NULL);
}