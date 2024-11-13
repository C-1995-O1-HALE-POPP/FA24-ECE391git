// plic.c - RISC-V PLIC
//

#include "plic.h"
#include "console.h"
#include "halt.h"
#include <stdint.h>

// COMPILE-TIME CONFIGURATION
//

// *** Note to student: you MUST use PLIC_IOBASE for all address calculations,
// as this will be used for testing!

#ifndef PLIC_IOBASE
#define PLIC_IOBASE 0x0C000000
#endif

#define PLIC_SRCCNT 0x400
#define PLIC_CTXCNT 1

#define PENDING_BASE (0x1000 + PLIC_IOBASE)

#define ENABLE_BASE (0x2000 + PLIC_IOBASE)
#define ENABLE_SIZE_PER_CONTEXT 0x80

#define THRESHOLD_BASE (0x200000 + PLIC_IOBASE)
#define THRESHOLD_SIZE_PER_CONTEXT 0x1000

#define CLAIM_BASE (0x200004 + PLIC_IOBASE)
#define CLAIM_SIZE_PER_CONTEXT 0x1000

#define SIZE_OF_SRCNO 4
#define SIZE_OF_PENDING_ARRAY 32
#define SIZE_OF_ENABLE_ARRAY 32

// INTERNAL FUNCTION DECLARATIONS
//

// *** Note to student: the following MUST be declared extern. Do not change these
// function delcarations!

extern void plic_set_source_priority(uint32_t srcno, uint32_t level);
extern int plic_source_pending(uint32_t srcno);
extern void plic_enable_source_for_context(uint32_t ctxno, uint32_t srcno);
extern void plic_disable_source_for_context(uint32_t ctxno, uint32_t srcno);
extern void plic_set_context_threshold(uint32_t ctxno, uint32_t level);
extern uint32_t plic_claim_context_interrupt(uint32_t ctxno);
extern void plic_complete_context_interrupt(uint32_t ctxno, uint32_t srcno);

// Currently supports only single-hart operation. The low-level PLIC functions
// already understand contexts, so we only need to modify the high-level
// functions (plic_init, plic_claim, plic_complete).

// EXPORTED FUNCTION DEFINITIONS
// 

void plic_init(void) {
    int i;

    // Disable all sources by setting priority to 0, enable all sources for
    // context 0 (M mode on hart 0).

    for (i = 0; i < PLIC_SRCCNT; i++) {
        plic_set_source_priority(i, 0);
        plic_enable_source_for_context(0, i);
    }
}

extern void plic_enable_irq(int irqno, int prio) {
    trace("%s(irqno=%d,prio=%d)", __func__, irqno, prio);
    plic_set_source_priority(irqno, prio);
}

extern void plic_disable_irq(int irqno) {
    if (0 < irqno)
        plic_set_source_priority(irqno, 0);
    else
        debug("plic_disable_irq called with irqno = %d", irqno);
}

extern int plic_claim_irq(void) {
    // Hardwired context 0 (M mode on hart 0)
    trace("%s()", __func__);
    return plic_claim_context_interrupt(0);
}

extern void plic_close_irq(int irqno) {
    // Hardwired context 0 (M mode on hart 0)
    trace("%s(irqno=%d)", __func__, irqno);
    plic_complete_context_interrupt(0, irqno);
}

// INTERNAL FUNCTION DEFINITIONS
//

void plic_set_source_priority(uint32_t srcno, uint32_t level) {
    // FIXME your code goes here

    // input: 
    //     srcno: interrupt source  level: the priority level
    // output:
    //      None
    // side effect:
    //      Sets the priority level for a specific interrupt source.
    //      This function modifies the priority array, 
    //      where each entry corresponds to a specific interrupt source.
    #if 0
    if(srcno < 1 || srcno > 1023){
        panic("Invalid interrupt source number");
    }
    #endif
    volatile uint32_t* dst = (volatile uint32_t*)((uintptr_t)(srcno * SIZE_OF_SRCNO + PLIC_IOBASE));
    *dst = level;

}

int plic_source_pending(uint32_t srcno) {
    // FIXME your code goes here
    
    // input:
    //      srcno: interrupt source
    // output:
    //      returns 1 if the interrupt is pending, 0 otherwise.
    // side effect:
    //      checks if an interrupt source is pending by inspecting the pending array.
    //      The pending bit is determined by checking the bit corresponding to 
    //      srcno in the pending array
    #if 0
    if(srcno < 1 || srcno > 1023){
        panic("Invalid interrupt source number");
    }
    #endif
    volatile uint32_t* array = ((volatile uint32_t*)((uintptr_t)(PENDING_BASE + SIZE_OF_SRCNO * (srcno/SIZE_OF_PENDING_ARRAY))));

    // move the corresponding bit to the least significant bit, then cmp
    return (1 & (*array >> (srcno % SIZE_OF_PENDING_ARRAY))) != 0;
}

void plic_enable_source_for_context(uint32_t ctxno, uint32_t srcno) {
    // FIXME your code goes here

    // input:
    //      ctxno: interrupt context   srcno: interrupt source
    // output:
    //      none
    // side effect:
    //      Enables a specific interrupt source for a given context.
    //      This function sets the appropriate bit in the enable array. 
    //      It calculates the index based on the source number
    //      and context, and sets the corresponding bit for the source.
    #if 0
    if(srcno < 1 || srcno > 1023){
        panic("Invalid interrupt source number");
    }
    if(ctxno > 15871){
        panic("Invalid interrupt context number");
    }
    #endif
    volatile uint32_t* ctx_base_loc = (volatile uint32_t*)((uintptr_t)(ENABLE_BASE + ctxno * ENABLE_SIZE_PER_CONTEXT));
    volatile uint32_t* src_array_loc = (volatile uint32_t*)((uintptr_t)(ctx_base_loc + SIZE_OF_SRCNO * (srcno / SIZE_OF_ENABLE_ARRAY)));
    *src_array_loc |= (1 << (srcno % SIZE_OF_ENABLE_ARRAY));
}

void plic_disable_source_for_context(uint32_t ctxno, uint32_t srcid) {
    // FIXME your code goes here

    // input:
    //      ctxno: interrupt context   srcid: interrupt source
    // output:
    //      none
    // side effect:
    //      Disables a specific interrupt source for a given context.
    //      This function sets the appropriate bit in the enable array. 
    //      It calculates the index based on the source number
    //      and context, and sets the corresponding bit for the source.
    #if 0
    if(srcid < 1 || srcid > 1023){
        panic("Invalid interrupt source number");
    }
    if(ctxno > 15871){
        panic("Invalid interrupt context number");
    }
    #endif
    volatile uint32_t* ctx_base_loc = (volatile uint32_t*)((uintptr_t)(ENABLE_BASE + ctxno * ENABLE_SIZE_PER_CONTEXT));
    volatile uint32_t* src_array_loc = (volatile uint32_t*)((uintptr_t)(ctx_base_loc + SIZE_OF_SRCNO * (srcid / SIZE_OF_ENABLE_ARRAY)));
    *src_array_loc &= ~(1 << (srcid % SIZE_OF_ENABLE_ARRAY));
}

void plic_set_context_threshold(uint32_t ctxno, uint32_t level) {
    // FIXME your code goes here

    // input:
    //      ctxno: interrupt context   level: the threshold level
    // output:
    //      none
    // side effect:
    //      Sets the interrupt priority threshold for a specific context.
    //      Interrupts with a priority lower than the threshold will not be handled
    //      by the context.
    #if 0
    if(ctxno > 15871){
        panic("Invalid interrupt context number");
    }
    #endif
    volatile uint32_t* dst = (volatile uint32_t*)((uintptr_t)(THRESHOLD_BASE + ctxno * THRESHOLD_SIZE_PER_CONTEXT));
    *dst = level;

}

uint32_t plic_claim_context_interrupt(uint32_t ctxno) {
    // FIXME your code goes here

    // input:
    //      ctxno: interrupt context
    // output:
    //      returns the interrupt source number 
    // side effect:
    //      Claims an interrupt for a given context.
    //      This function reads from the claim register 
    //      and returns the interrupt ID of the highest-priority pending interrupt. 
    //      It returns 0 if no interrupts are pending.
    #if 0
    if(ctxno > 15871){
        panic("Invalid interrupt context number");
    }
    #endif
    volatile uint32_t* dst = (volatile uint32_t*)((uintptr_t)(CLAIM_BASE + ctxno * CLAIM_SIZE_PER_CONTEXT));
    return *dst;
}

void plic_complete_context_interrupt(uint32_t ctxno, uint32_t srcno) {
    // FIXME your code goes here

    // input:
    //      ctxno: interrupt context   srcno: interrupt source
    // output:
    //      none
    // side effect:
    //      Completes the handling of an interrupt for a given context.
    //      This function writes the interrupt source number back to the claim register, 
    //      notifying the PLIC that the interrupt has been serviced
    #if 0
    if(srcno < 1 || srcno > 1023){
        panic("Invalid interrupt source number");
    }
    if(ctxno > 15871){
        panic("Invalid interrupt context number");
    }
    #endif
    volatile uint32_t* dst = (volatile uint32_t*)((uintptr_t)(CLAIM_BASE + ctxno * CLAIM_SIZE_PER_CONTEXT));
    *dst = srcno;
}