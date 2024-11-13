// serial.c - NS16550a serial port
// 

#include "serial.h"
#include "intr.h"
#include "halt.h"

#include <stddef.h>
#include <stdint.h>

// COMPILE-TIME CONSTANT DEFINITIONS
//

#ifndef UART0_MMIO_BASE
#define UART0_MMIO_BASE 0x10000000
#endif

#ifndef UART1_MMIO_BASE
#define UART1_MMIO_BASE 0x10000100
#endif

#ifndef UART1_IRQNO
#define UART1_IRQNO 11
#endif

#ifndef SERIAL_RBUFSZ
#define SERIAL_RBUFSZ 64
#endif

// EXPORTED VARIABLE DEFINITIONS
//

char com0_initialized = 0;
char com1_initialized = 0;

// INTERNAL TYPE DEFINITIONS
// 

struct ns16550a_regs {
    union {
        char rbr; // DLAB=0 read
        char thr; // DLAB=0 write
        uint8_t dll; // DLAB=1
    };
    
    union {
        uint8_t ier; // DLAB=0
        uint8_t dlm; // DLAB=1
    };
    
    union {
        uint8_t iir; // read
        uint8_t fcr; // write
    };

    uint8_t lcr;
    uint8_t mcr;
    uint8_t lsr;
    uint8_t msr;
    uint8_t scr;
};

#define LCR_DLAB (1 << 7)
#define LSR_OE (1 << 1)
#define LSR_DR (1 << 0)
#define LSR_THRE (1 << 5)
#define IER_DRIE (1 << 0)
#define IER_THREIE (1 << 1)


struct ringbuf {
    uint16_t hpos; // head of queue (from where elements are removed)
    uint16_t tpos; // tail of queue (where elements are inserted)
    char data[SERIAL_RBUFSZ];
};

// INTERAL FUNCTION DECLARATIONS
//

static void uart1_isr(int irqno, void * aux);

static void rbuf_init(struct ringbuf * rbuf);
static int rbuf_empty(const struct ringbuf * rbuf);
static int rbuf_full(const struct ringbuf * rbuf);
static void rbuf_put(struct ringbuf * rbuf, char c);
static char rbuf_get(struct ringbuf * rbuf);

static void rbuf_wait_not_empty(const struct ringbuf * rbuf);
static void rbuf_wait_not_full(const struct ringbuf * rbuf);


// INTERNAL MACRO DEFINITIONS
//

#define UART0 (*(volatile struct ns16550a_regs*)UART0_MMIO_BASE)
#define UART1 (*(volatile struct ns16550a_regs*)UART1_MMIO_BASE)

// INTERNAL GLOBAL VARIABLES
//

static struct ringbuf uart1_rxbuf;
static struct ringbuf uart1_txbuf;

// EXPORTED FUNCTION DEFINITIONS
//

void com0_init(void) {
    // Configure UART0. We set the baud rate divisor to 1, the lowest value,
    // for the fastest baud rate. In a physical system, the actual baud rate
    // depends on the attached oscillator frequency. In a virtualized system,
    // it doesn't matter.
    
    UART0.lcr = LCR_DLAB;
    UART0.dll = 0x01;
    UART0.dlm = 0x00;
    UART0.lcr = 0; // DLAB=0

    UART0.rbr; // flush receive buffer
    UART0.ier = 0x00;
    //UART0.ier = IER_DRIE|IER_THREIE;
    //com0_initialized = 1;
}

void com0_putc(char c) {
    // Spin until THR is empty
    while (!(UART0.lsr & LSR_THRE))
        continue;

    UART0.thr = c;
}

char com0_getc(void) {
    // Spin until RBR contains a byte
    while (!(UART0.lsr & LSR_DR))
        continue;
    
    return UART0.rbr;
}

void com1_init(void) {
    // FIXME your code goes here

    // input, output: none
    // side effect: Initializes the UART.
    //
    //      It should set up the receive and transmit ring buffers, register and
    //      enable its interrupt service routine (ISR) with the PLIC, 
    //      configure the control registers for the UART. It should set the
    //      baud rate divisor to 1, 
    //      flush the receive buffer, and 
    //      enable the interrupt for receiving data.

    // set up RBUFs
    rbuf_init(&uart1_rxbuf); rbuf_init(&uart1_txbuf);

    // register and enable ISR
    intr_register_isr(UART1_IRQNO, 1, uart1_isr, NULL); 
    intr_enable_irq(UART1_IRQNO);

    // configure control registers
    // set baud rate divisor to 1
    UART1.lcr = LCR_DLAB;
    UART1.dll = 0x01;
    UART1.dlm = 0x00;
    UART1.lcr = 0; // DLAB=0

    // flush receive buffer
    UART1.rbr;

    // enable interrupt for receiving data
    UART1.ier = IER_DRIE;

    com1_initialized = 1;
}

void com1_putc(char c) {
    // FIXME your code goes here

    // input: c: character to transmit
    // output: none
    // side effect: Writes a character to the UART.
    //      This function writes a character to the UART transmit ring buffer.
    //      If the buffer is full, it waits until there is space to write the character. 
    //      It also enables the appropriate interrupt to ask the UART to send the character.

    // wait until there is space in the buffer
    rbuf_wait_not_full(&uart1_txbuf);

    // write the character to the buffer
    int s = intr_disable();
    rbuf_put(&uart1_txbuf, c);
    // renable interrupt
    intr_restore(s);
    UART1.ier |= IER_THREIE;
    
}

char com1_getc(void) {
    // FIXME your code goes here

    // input: none
    // output: returns the character read from the UART
    // side effect: Reads a character from the UART.
    //      This function reads a character from the UART receive buffer.
    //      If the buffer is empty, it waits until there is a character to read.
    //      It also enables the appropriate interrupt to ask the UART to receive the character.

    // wait until there is a character to read
    rbuf_wait_not_empty(&uart1_rxbuf);

    // renable interrupt
    int c = intr_disable();
    char ret = rbuf_get(&uart1_rxbuf);
    // read the character from the buffer
    intr_restore(c);
    UART1.ier |= IER_DRIE;

    return ret;

}

static void uart1_isr (int irqno, void * aux) {
    const uint_fast8_t line_status = UART1.lsr;

    if (line_status & LSR_OE)
        panic("Receive buffer overrun");
    
    // FIXME your code goes here
    // 1. data to be received
    // reads from the receive buffer register (RBR) and 
    // stores the data in the receive ring buffer, 
    // unless the receive ring buffer is full.
    if (line_status & LSR_DR) {
        if (!rbuf_full(&uart1_rxbuf)) {
            rbuf_put(&uart1_rxbuf, UART1.rbr);
        }
        // preventing further interrupts
        else{
            UART1.ier &= ~IER_DRIE;
        }
    }
    // 2. data can be transmitted
    if (line_status & LSR_THRE) {
        if (!rbuf_empty(&uart1_txbuf)) {
            UART1.thr = rbuf_get(&uart1_txbuf);
        }
        // preventing further interrupts 
        else{
            UART1.ier &= ~IER_THREIE;
        }
    }

}

void rbuf_init(struct ringbuf * rbuf) {
    rbuf->hpos = 0;
    rbuf->tpos = 0;
}

int rbuf_empty(const struct ringbuf * rbuf) {
    const volatile struct ringbuf * const vrbuf = rbuf;
    return (rbuf->hpos == vrbuf->tpos);
}

int rbuf_full(const struct ringbuf * rbuf) {
    const volatile struct ringbuf * const vrbuf = rbuf;
    return (rbuf->tpos - vrbuf->hpos == SERIAL_RBUFSZ);
}

void rbuf_put(struct ringbuf * rbuf, char c) {
    rbuf->data[rbuf->tpos++ % SERIAL_RBUFSZ] = c;
}

char rbuf_get(struct ringbuf * rbuf) {
    return rbuf->data[rbuf->hpos++ % SERIAL_RBUFSZ];
}

// The rbuf_wait_not_empty and rbuf_wait_not_full functions until a ring buffer
// is not empty and not full, respectively, using the wait-for-interrupt
// intruction. (This is more efficient than spin-waiting.)

void rbuf_wait_not_empty(const struct ringbuf * rbuf) {
    // Note: we need to disable interrupts to avoid a race condition where the
    // ISR runs and places a character in the buffer after we check if its empty
    // but before we execute the wfi instruction.

    intr_disable();

    while (rbuf_empty(rbuf)) {
        asm ("wfi"); // wait until interrupt pending
        intr_enable();
        intr_disable();
    }

    intr_enable();
}

void rbuf_wait_not_full(const struct ringbuf * rbuf) {
    // See comment in rbuf_wait_not_empty().

    intr_disable();

    while (rbuf_full(rbuf)) {
        asm ("wfi"); // wait until interrupt pending
        intr_enable();
        intr_disable();
    }

    intr_enable();
}