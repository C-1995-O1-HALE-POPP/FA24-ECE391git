// excp.c - Exception handlers
//

#include "trap.h"
#include "csr.h"
#include "halt.h"
#include "memory.h"
#include "process.h"

#include <stddef.h>



// INTERNAL FUNCTION DECLARATIONS
//

static void __attribute__ ((noreturn)) default_excp_handler (
    unsigned int code, const struct trap_frame * tfr);

// IMPORTED FUNCTION DECLARATIONS
//

extern void syscall_handler(struct trap_frame * tfr); // syscall.c

// INTERNAL GLOBAL VARIABLES
//

static const char * const excp_names[] = {
	[RISCV_SCAUSE_INSTR_ADDR_MISALIGNED] = "Misaligned instruction address",
	[RISCV_SCAUSE_INSTR_ACCESS_FAULT] = "Instruction access fault",
	[RISCV_SCAUSE_ILLEGAL_INSTR] = "Illegal instruction",
	[RISCV_SCAUSE_BREAKPOINT] = "Breakpoint",
	[RISCV_SCAUSE_LOAD_ADDR_MISALIGNED] = "Misaligned load address",
	[RISCV_SCAUSE_LOAD_ACCESS_FAULT] = "Load access fault",
	[RISCV_SCAUSE_STORE_ADDR_MISALIGNED] = "Misaligned store address",
	[RISCV_SCAUSE_STORE_ACCESS_FAULT] = "Store access fault",
    [RISCV_SCAUSE_ECALL_FROM_UMODE] = "Environment call from U mode",
    [RISCV_SCAUSE_ECALL_FROM_SMODE] = "Environment call from S mode",
    [RISCV_SCAUSE_INSTR_PAGE_FAULT] = "Instruction page fault",
    [RISCV_SCAUSE_LOAD_PAGE_FAULT] = "Load page fault",
    [RISCV_SCAUSE_STORE_PAGE_FAULT] = "Store page fault"
};

// EXPORTED FUNCTION DEFINITIONS
//

void smode_excp_handler(unsigned int code, struct trap_frame * tfr) {
    trace("smode_excp_handler(%d, %p)", code, tfr);
	default_excp_handler(code, tfr);
}

/*
 * umode_excp_handler - User Mode Exception Handler
 * 
 * This function handles exceptions that occur in User Mode (U-mode). It performs appropriate actions based on the exception type.
 * The types of exceptions include system calls, page faults, memory access errors, illegal instructions, etc.
 * 
 * Input:
 *   code - The exception code
 *   tfr  - A pointer to the `trap_frame` structure
 * Output:
 *   None
 * 
 * Side effects:
 *   The function performs different actions based on the exception type
 * 
 * The exception codes (code)  include:
 *   - RISCV_SCAUSE_ECALL_FROM_UMODE: User-mode system call
 *   - RISCV_SCAUSE_ECALL_FROM_SMODE: Environment call from supervisor mode (S-mode)
 *   - RISCV_SCAUSE_LOAD_PAGE_FAULT: Load page fault
 *   - RISCV_SCAUSE_STORE_PAGE_FAULT: Store page fault
 *   - RISCV_SCAUSE_INSTR_PAGE_FAULT: Instruction page fault
 *   - RISCV_SCAUSE_INSTR_ADDR_MISALIGNED: Instruction address misalignment
 *   - RISCV_SCAUSE_INSTR_ACCESS_FAULT: Instruction access fault
 *   - RISCV_SCAUSE_ILLEGAL_INSTR: Illegal instruction
 *   - RISCV_SCAUSE_LOAD_ADDR_MISALIGNED: Load address misalignment
 *   - RISCV_SCAUSE_LOAD_ACCESS_FAULT: Load access fault
 *   - RISCV_SCAUSE_STORE_ACCESS_FAULT: Store access fault
 *   - RISCV_SCAUSE_STORE_ADDR_MISALIGNED: Store address misalignment
 *   - Others: Default exception handling
 */


void umode_excp_handler(unsigned int code, struct trap_frame * tfr) {
    trace("umode_excp_handler(%d, %p)", code, tfr);
    debug("Exception %d at %p: ", code, (void*)tfr->sepc);
    //console_puts(excp_names[code]);
    switch (code) {
        // TODO: FIXME dispatch to various U mode exception handlers

        // Handle system call exceptions (ECALL) from U-mode
        case RISCV_SCAUSE_ECALL_FROM_UMODE:
            syscall_handler(tfr);
            break;
        // Handle environment calls from S-mode (supervisor mode)
        case RISCV_SCAUSE_ECALL_FROM_SMODE:
            panic("Environment call from S mode");
            break;
        // Handle page faults: memory access errors during load, store, or instruction fetch
        case RISCV_SCAUSE_LOAD_PAGE_FAULT:
        case RISCV_SCAUSE_STORE_PAGE_FAULT:
        case RISCV_SCAUSE_INSTR_PAGE_FAULT:
            memory_handle_page_fault((void*)csrr_stval());
            break;
        // case RISCV_SCAUSE_BREAKPOINT:
        // Handle misalignment or access faults for instructions or memory accesses
        case RISCV_SCAUSE_INSTR_ADDR_MISALIGNED:
        case RISCV_SCAUSE_INSTR_ACCESS_FAULT:
        case RISCV_SCAUSE_ILLEGAL_INSTR:
        case RISCV_SCAUSE_LOAD_ADDR_MISALIGNED: 
        case RISCV_SCAUSE_LOAD_ACCESS_FAULT:
        case RISCV_SCAUSE_STORE_ACCESS_FAULT:
        case RISCV_SCAUSE_STORE_ADDR_MISALIGNED:
            process_exit();
            break;
        // Default case: handle any other exception codes with a generic handler
        default:
            default_excp_handler(code, tfr);
            break;
    }
}

void default_excp_handler (
    unsigned int code, const struct trap_frame * tfr)
{
    const char * name = NULL;

    if (0 <= code && code < sizeof(excp_names)/sizeof(excp_names[0]))
		name = excp_names[code];
	
	if (name == NULL)
		kprintf("Exception %d at %p\n", code, (void*)tfr->sepc);
	else
		kprintf("%s at %p\n", name, (void*)tfr->sepc);
	
    panic(NULL);
}