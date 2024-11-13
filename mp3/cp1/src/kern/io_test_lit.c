//           main.c - Main function: runs shell to load executable
//          

#include "console.h"
#include "thread.h"
#include "device.h"
#include "uart.h"
#include "timer.h"
#include "intr.h"
#include "heap.h"
#include "halt.h"
#include "elf.h"
#include "string.h"



struct io_lit lit;
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
#define VERY_LARGE_BUFFER 8192
#define SMALL_BUFFER 64
#define MINI_BUF 5

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

    intr_enable();
    timer_start();

    //           Open terminal for trek

    result = device_open(&termio, "ser", 1);

    if (result != 0)
        panic("Could not open ser1");
    kprintf("Opened ser1\n");
    
    shell_main(termio);
}

void shell_main(struct io_intf * termio_raw) {
    struct io_term ioterm;
    struct io_intf * termio;
    struct io_intf * exeio;
    char cmdbuf[9];
    char buf_large[VERY_LARGE_BUFFER];
    char buf_small[SMALL_BUFFER];
    char write_buf[MINI_BUF] = "haha ";
    uint64_t pos;
    int result;

    termio = ioterm_init(&ioterm, termio_raw);
    exeio = iolit_init(&lit, _companion_f_start, _companion_f_end-_companion_f_start);
    kprintf("Sequence read small\n");
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));
        if (cmdbuf[0] == '\0')
            break;
    }    
    ioseek(exeio, 0);
    for(int i = 0; i < 5; i++){
        result = ioread(exeio, buf_small, SMALL_BUFFER);
        if (result != SMALL_BUFFER)
            kprintf("blk read failed\n");
        else
            kprintf("blk read success\n");
        exeio->ops->ctl(exeio, IOCTL_GETPOS, &pos);
        kprintf("read %d bytes, pos at %x", result, pos);
        kprintf("%s\n", buf_small);
        for (;;) {
            ioprintf(termio, "CMD> ");
            ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));
            if (cmdbuf[0] == '\0')
                break;
        } 
    }
    kprintf("random read small\n");
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));
        if (cmdbuf[0] == '\0')
            break;
    }    
    ioseek(exeio, 1234);    // random magic number
    result = ioread(exeio, buf_small, SMALL_BUFFER);
    if (result != SMALL_BUFFER)
        kprintf("blk read failed\n");
    else
        kprintf("blk read success\n");
    exeio->ops->ctl(exeio, IOCTL_GETPOS, &pos);
    kprintf("read %d bytes, pos at %x", result, pos);
    kprintf("%s\n", buf_small);

    kprintf("sequence read large\n");
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));
        if (cmdbuf[0] == '\0')
            break;
    }    
    ioseek(exeio, 0);
    for(int i = 0; i < 5; i++){
        result = ioread(exeio, buf_large, VERY_LARGE_BUFFER);
        if (result != VERY_LARGE_BUFFER)
            kprintf("blk read failed\n");
        else
            kprintf("blk read success\n");
        exeio->ops->ctl(exeio, IOCTL_GETPOS, &pos);
        kprintf("read %d bytes, pos at %x", result, pos);
        kprintf("first 5 chars in terminal");
        for(int j = 0; j < 5; j++){
            ioputc(termio, buf_large[j]);
        }
        for (;;) {
            ioprintf(termio, "CMD> ");
            ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));
            if (cmdbuf[0] == '\0')
                break;
        } 
    }

    kprintf("random write small\n");
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));
        if (cmdbuf[0] == '\0')
            break;
    }    

    ioseek(exeio, 5678);    // random magic number
    result = iowrite(exeio, write_buf, 5);
    if (result != SMALL_BUFFER)
        kprintf("blk write failed\n");
    else
        kprintf("blk write success\n");
    result = iowrite(exeio, write_buf, 5);
    if (result != SMALL_BUFFER)
        kprintf("blk write failed\n");
    else
        kprintf("blk write success\n");
    ioseek(exeio, 5678);    // goback
    result = ioread(exeio, buf_small, SMALL_BUFFER);
    if (result != SMALL_BUFFER)
        kprintf("blk read failed\n");
    else
        kprintf("blk read success\n");
    exeio->ops->ctl(exeio, IOCTL_GETPOS, &pos);
    kprintf("read %d bytes, pos at %x", result, pos);
    kprintf("%s\n", buf_small);

    kprintf("test ioctls\n");
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));
        if (cmdbuf[0] == '\0')
            break;
    }    
    result = exeio->ops->ctl(exeio, IOCTL_GETBLKSZ, &pos);
    if (result != 0)
        kprintf("IOCTL_GETBLKSZ failed - should fail\n");
    else
        kprintf("IOCTL_GETBLKSZ success\n");
    result = exeio->ops->ctl(exeio, IOCTL_GETLEN, &pos);
    if (result != 0)
        kprintf("IOCTL_GETLEN failed\n");
    else
        kprintf("IOCTL_GETLEN success - image len =  %d\n", pos);
    result = exeio->ops->ctl(exeio, IOCTL_GETPOS, &pos);
    if(result != 0)
        kprintf("IOCTL_GETPOS failed\n");
    else
        kprintf("IOCTL_GETPOS success - image pos = %d\n", pos);
    kprintf("move to 8192 \n");
    result = ioseek(exeio, VERY_LARGE_BUFFER);
    if(result != 0)
        kprintf("ioseek failed\n");
    else
        kprintf("ioseek success\n");
    result = exeio->ops->ctl(exeio, IOCTL_SETLEN, &pos);
    if(result != 0)
        kprintf("IOCTL_SETLEN failed - should be failed\n");
    else
        kprintf("IOCTL_SETLEN success\n");
    result = exeio->ops->ctl(exeio, IOCTL_FLUSH, &pos);
    if(result != 0)
        kprintf("IOCTL_FLUSH failed - should be failed\n");
    else
        kprintf("IOCTL_FLUSH success\n");

    ioputs(termio, "Enter executable name or \"exit\" to exit");


    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));

        if (cmdbuf[0] == '\0')
            continue;

        if (strcmp("exit", cmdbuf) == 0)
            return;

        ioclose(exeio);
    }
}
