// timer.c
//

#include "timer.h"
#include "thread.h"
#include "csr.h"
#include "intr.h"
#include "halt.h" // for assert

// EXPORTED GLOBAL VARIABLE DEFINITIONS
// 

char timer_initialized = 0;

struct condition tick_1Hz;
struct condition tick_10Hz;

uint64_t tick_1Hz_count;
uint64_t tick_10Hz_count;

#define MTIME_FREQ 10000000 // from QEMU include/hw/intc/riscv_aclint.h
#define TIMER_10HZ 10

// INTERNVAL GLOBAL VARIABLE DEFINITIONS
//

// INTERNAL FUNCTION DECLARATIONS
//

static inline uint64_t get_mtime(void);
static inline void set_mtime(uint64_t val);
static inline uint64_t get_mtimecmp(void);
static inline void set_mtimecmp(uint64_t val);

// EXPORTED FUNCTION DEFINITIONS
//

void timer_init(void) {
    assert (intr_initialized);
    condition_init(&tick_1Hz, "tick_1Hz");
    condition_init(&tick_10Hz, "tick_10Hz");

    // Set mtimecmp to maximum so timer interrupt does not fire

    set_mtime(0);
    set_mtimecmp(UINT64_MAX);
    csrc_mie(RISCV_MIE_MTIE);

    timer_initialized = 1;
}

void timer_start(void) {
    set_mtime(0);
    set_mtimecmp(MTIME_FREQ / 10);
    csrs_mie(RISCV_MIE_MTIE);
}

// timer_handle_interrupt() is dispatched from intr_handler in intr.c

void timer_intr_handler(void) {
    // FIXME your code goes here
    // 1. increment tick_1Hz_count and tick_10Hz_count
    // 2. signal the condition variables
    //console_printf("last tick running on %d \n", running_thread());
    // uint64_t mtime = get_mtime();
    uint64_t mtimecmp = get_mtimecmp();
    tick_10Hz_count++;
    condition_broadcast(&tick_10Hz);
    //console_printf("10Hz ");

    if (tick_10Hz_count % TIMER_10HZ == 0) {
        // I hate magic numbers - here I used 10 and therefore lost 2 pts in MP2 CP2
        tick_1Hz_count++;
        condition_broadcast(&tick_1Hz);
        //console_printf("1Hz ");
    }

    // Set the next timer interrupt
    set_mtimecmp(mtimecmp + MTIME_FREQ / TIMER_10HZ);
    // console_printf("tick_1Hz_count: %d, tick_10Hz_count: %d, current time stamp: %d, mtime cpr: %d \n", tick_1Hz_count, tick_10Hz_count, mtime, mtimecmp);
}

// Hard-coded MTIMER device addresses for QEMU virt device

#define MTIME_ADDR 0x200BFF8
#define MTCMP_ADDR 0x2004000

static inline uint64_t get_mtime(void) {
    return *(volatile uint64_t*)MTIME_ADDR;
}

static inline void set_mtime(uint64_t val) {
    *(volatile uint64_t*)MTIME_ADDR = val;
}

static inline uint64_t get_mtimecmp(void) {
    return *(volatile uint64_t*)MTCMP_ADDR;
}

static inline void set_mtimecmp(uint64_t val) {
    *(volatile uint64_t*)MTCMP_ADDR = val;
}
