// process.c - user process
//
#include "process.h"
#ifdef PROCESS_TRACE
#define TRACE
#endif

#ifdef PROCESS_DEBUG
#define DEBUG
#endif

#include "config.h"
#include "io.h"
#include "thread.h"
#include "error.h"
#include "memory.h"
#include "csr.h"
#include "intr.h"
#include "string.h"
#include "heap.h"
#include "console.h"
#include "elf.h"
#include "thread.h"
#include "halt.h" 
#include <stdint.h>
// COMPILE-TIME PARAMETERS
//

// NPROC is the maximum number of processes

#ifndef NPROC
#define NPROC 16
#endif

// INTERNAL FUNCTION DECLARATIONS
//

// INTERNAL GLOBAL VARIABLES
//

#define MAIN_PID 0

// Define the process structure
// The main user process struct

static struct process main_proc;

// A table of pointers to all user processes in the system

struct process * proctab[NPROC] = {
    [MAIN_PID] = &main_proc
};

// EXPORTED GLOBAL VARIABLES
//

char procmgr_initialized = 0;

// EXPORTED FUNCTION DEFINITIONS
//
void procmgr_init(void){
    // input: none
    //
    // output: none
    //
    // side effect:
    //   Initializes processes globally by initializing a process 
    //   structure for the main user process (init). The init process 
    //   should always be assigned process ID (PID) 0.
    assert(!procmgr_initialized);

    // create struct for main process
    main_proc = (struct process) {
        .id = MAIN_PID,
        .tid = running_thread(),
        .mtag = active_memory_space(),
    };
    // set thread for process
    thread_set_process(main_proc.tid, &main_proc);
    for(int i = 1; i < PROCESS_IOMAX; i++){
        main_proc.iotab[i] = NULL;
    }
    // sign to show having initialized
    procmgr_initialized = 1;
}

int process_exec(struct io_intf * exeio){
    // input:
    //  exeio: the I/O interface for the executable
    //
    // output:
    //  Returns the process ID (PID) of the new process
    //
    // side effect:
    // Executes a program referred to by the I/O interface passed in 
    // as an argument. We only require a maximum of 16 concurrent processes.

    // create a new process

    // Executing a loaded program with process exec has 4 main requirements:
    // (a) First any virtual memory mappings belonging to other user processes should be unmapped.
    memory_unmap_and_free_user();

    // (b) Then a fresh 2nd level (root) page table should be created and initialized with the default mappings for a user process.
    // proc->id = pid;
    // proc->mtag = memory_space_create(pid);

    // (c) Next the executable should be loaded from the I/O interface provided as an argument into the mapped pages. (Hint: elf load)
    void (*exe_entry)(void);
    if(elf_load(exeio, &exe_entry)!=0){
        return -1;
    }
    // for(int i = 0; i < PROCESS_IOMAX; i++){
    //     proctab[MAIN_PID]->iotab[i] = NULL;
    // }

    // (d) Finally, the thread associated with the process needs to be started in user-mode. 
    // (Hint: An assembly function in *thrasm.s* would be useful here)
    thread_jump_to_user(USER_END_VMA,(uintptr_t)exe_entry);    

}

void __attribute__ ((noreturn)) process_exit(void){
    // input: none
    //
    // output: none
    //
    // side effect:
    //   Exits the current process. This function should be called 
    //   when a process is done executing. It should free all memory 
    //   associated with the process and terminate the thread associated 
    //   with the process. The process ID (PID) of the process should 
    //   be returned to the pool of available PIDs.
    process_terminate(current_pid());
    panic("process_exit: should never reach here");
}

extern void process_terminate(int pid){
    // input:
    //  pid: the process to terminate
    //
    // output: none
    //
    // side effect:
    // Terminates the process with the given PID. This function should 
    // free all memory associated with the process and terminate the 
    // thread associated with the process. The process ID (PID) of the 
    // process should be returned to the pool of available PIDs.
    assert(proctab[pid] != NULL);
    struct process * proc = proctab[pid];
    // free all memory associated with the process
    memory_space_reclaim();
    for(int i = 1; i < PROCESS_IOMAX; i++){
        if(proc->iotab[i] != NULL){
            ioclose(proc->iotab[i]);
        }
        proc->iotab[i] = NULL;
    }
    // terminate the thread associated with the process
    proctab[pid] = NULL;
    kfree(proc);
    thread_exit();
}
