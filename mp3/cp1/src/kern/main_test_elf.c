//           main.c - Main function: runs shell to load executable
//          

#include "console.h"
#include "thread.h"
#include "device.h"
#include "uart.h"
#include "timer.h"
#include "intr.h"
#include "heap.h"
#include "virtio.h"
#include "halt.h"
#include "elf.h"
#include "fs.h"
#include "string.h"

//           end of kernel image (defined in kernel.ld)
extern char _kimg_end[];
extern char _companion_f_start[];
extern char _companion_f_end[];
#define RAM_SIZE (8*1024*1024)
#define RAM_START 0x80000000UL
#define KERN_START RAM_START
#define USER_START 0x80100000UL

#define UART0_IOBASE 0x10000000
#define UART1_IOBASE 0x10000100
#define UART0_IRQNO 10

#define VIRT0_IOBASE 0x10001000
#define VIRT1_IOBASE 0x10002000
#define VIRT0_IRQNO 1

struct io_lit lit;

static void shell_main(struct io_intf * termio);

void main(void) {
    struct io_intf * termio;
    void * mmio_base;
    int result;
    int i;

    console_init();
    intr_init();
    devmgr_init();
    thread_init();
    timer_init();

    heap_init(_kimg_end, (void*)USER_START);

    //           Attach NS16550a serial devices

    for (i = 0; i < 2; i++) {
        mmio_base = (void*)UART0_IOBASE;
        mmio_base += (UART1_IOBASE-UART0_IOBASE)*i;
        uart_attach(mmio_base, UART0_IRQNO+i);
    }
    
    //           Attach virtio devices
    intr_enable();
    timer_start();
    //           Open terminal for trek

    result = device_open(&termio, "ser", 1);

    if (result != 0)
        panic("Could not open ser1");
    kprintf("%x %x\n", _companion_f_start, _companion_f_end);
    
    shell_main(termio);
}

void shell_main(struct io_intf * termio_raw) {
    struct io_term ioterm;
    struct io_intf * termio;
    void (*exe_entry)(struct io_intf*);
    struct io_intf * exeio;
    char cmdbuf[9];
    int tid;
    int result;

    termio = ioterm_init(&ioterm, termio_raw);

    ioputs(termio, "MAIN_TEST_ELF\n");
    
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));

        if (cmdbuf[0] == '\0')
            continue;

        if (strcmp("exit", cmdbuf) == 0)
            return;
        
        exeio = iolit_init(&lit, _companion_f_start, _companion_f_end - _companion_f_start);

        kprintf("Calling elf_load\n");

        result = elf_load(exeio, &exe_entry);

        kprintf("elf_load iolit returned %d\n", result);

        if (result < 0) {
            ioprintf(termio, "Error %d", -result);
        
        } else {
            tid = thread_spawn(cmdbuf, (void*)exe_entry, termio_raw);

            if (tid < 0)
                ioprintf(termio, "Error %d", -result);
            else
                thread_join(tid);
        }

        ioclose(exeio);
    }
}
