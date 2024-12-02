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
#include "string.h"

//           end of kernel image (defined in kernel.ld)
extern char _kimg_end[];

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
#define SMALL_BUFFER 512

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

    for (i = 0; i < 8; i++) {
        mmio_base = (void*)VIRT0_IOBASE;
        mmio_base += (VIRT1_IOBASE-VIRT0_IOBASE)*i;
        virtio_attach(mmio_base, VIRT0_IRQNO+i);
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
    struct io_intf * blkio;
    char cmdbuf[9];
    int result;
    char small_buf[SMALL_BUFFER];
    char large_buf[VERY_LARGE_BUFFER];
    long pos;
    memset(small_buf, 0, SMALL_BUFFER);
    memset(large_buf, 0, VERY_LARGE_BUFFER);
    result = device_open(&blkio, "blk", 0);
    if (result != 0)
        panic("device_open failed");

    kprintf("Mounted blk0");
    termio = ioterm_init(&ioterm, termio_raw);

    ioputs(termio, "Test for blk drv by rw on raw disks\n");
    ioputs(termio, "sequential read with small buf");
    ioseek(blkio, 0);
    for(int i = 0; i < 5; i++){                     // magic number for small test
        result = ioread(blkio, small_buf, SMALL_BUFFER);
        if (result != SMALL_BUFFER)
            kprintf("blk read failed\n");
        else
            kprintf("blk read success\n");
        blkio->ops->ctl(blkio, IOCTL_GETPOS, &pos);
        kprintf("read %d bytes, pos at %x", result, pos);
        kprintf("%s\n", small_buf);
        for (;;) {
            ioprintf(termio, "CMD> ");
            ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));
            if (cmdbuf[0] == '\0')
                break;
        } 
    }
    ioputs(termio, "sequential read with large buf");
    ioseek(blkio, 0);
    for(int i = 0; i < 5; i++){                     // magic number for small test
        result = ioread(blkio, large_buf, VERY_LARGE_BUFFER);
        if (result != VERY_LARGE_BUFFER)
            kprintf("blk read failed\n");
        else
            kprintf("blk read success\n");
        blkio->ops->ctl(blkio, IOCTL_GETPOS, &pos);
        kprintf("read %d bytes, pos at %x", result, pos);
        kprintf("first 20 chars is in terminal");
        for(int j = 0; j < 20; j++){
            ioputc(termio, large_buf[j]);
        }
        ioputc(termio, '\n');
        for (;;) {
            ioprintf(termio, "CMD> ");
            ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));
            if (cmdbuf[0] == '\0')
                break;
        } 
    }
    kprintf("test ioctl\n");
    result = blkio->ops->ctl(blkio, IOCTL_GETBLKSZ, &pos);
    if (result != 0)
        kprintf("ioctl failed\n");
    else
        kprintf("ioctl success - blk size = %d\n", pos);
    result = blkio->ops->ctl(blkio, IOCTL_GETLEN, &pos);
    if (result != 0)
        kprintf("ioctl failed\n");
    else
        kprintf("ioctl success - image len =  %d\n", pos);
    result = blkio->ops->ctl(blkio, IOCTL_GETPOS, &pos);
    if(result != 0)
        kprintf("ioctl failed\n");
    else
        kprintf("ioctl success - image pos = %d\n", pos);
    kprintf("move to 8192 \n");
    ioseek(blkio, VERY_LARGE_BUFFER);
    result = blkio->ops->ctl(blkio, IOCTL_GETPOS, &pos);
    if(result != 0)
        kprintf("ioctl failed\n");
    else
        kprintf("ioctl success - image pos = %d\n", pos);
    kprintf("write 1024 \"a\" to blk0\n");
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));
        if (cmdbuf[0] == '\0')
            break;
    } 
    __sync_synchronize();
    memset(large_buf, 'a', VERY_LARGE_BUFFER);
    result = iowrite(blkio, large_buf, SMALL_BUFFER);
    if (result != SMALL_BUFFER)
        kprintf("blk write failed\n");
    else
        kprintf("blk write success\n");
    kprintf("write data %d\n", result);
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));
        if (cmdbuf[0] == '\0')
            break;
    } 
    result = iowrite(blkio, large_buf, SMALL_BUFFER);
    if (result != SMALL_BUFFER)
        kprintf("blk write failed\n");
    else
        kprintf("blk write success\n");
    kprintf("write data %d\n", result);
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));
        if (cmdbuf[0] == '\0')
            break;
    }
    memset(small_buf, 0, SMALL_BUFFER);
    kprintf("read from blk0 at 8192\n");
    ioseek(blkio, VERY_LARGE_BUFFER);
    result = ioread(blkio, small_buf, SMALL_BUFFER);
    if(result <= 0)
        kprintf("blk read failed\n");
    else{
        kprintf("blk read success for %d bytes\n", result);
        kprintf("%s\n", small_buf);
    }
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));
        if (cmdbuf[0] == '\0')
            break;
    } 
    result = ioread(blkio, small_buf, SMALL_BUFFER);
    if(result <= 0)
        kprintf("blk read failed\n");
    else{
        kprintf("blk read success for %d bytes\n", result);
        kprintf("%s\n", small_buf);
    }
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));
        if (cmdbuf[0] == '\0')
            break;
    } 
    result = ioread(blkio, small_buf, SMALL_BUFFER);
    if(result <= 0)
        kprintf("blk read failed\n");
    else{
        kprintf("blk read success for %d bytes\n", result);
        kprintf("%s\n", small_buf);
    }
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));
        if (cmdbuf[0] == '\0')
            break;
    } 
    kprintf("test ioctls\n");
    result = blkio->ops->ctl(blkio, IOCTL_GETBLKSZ, &pos);
    if (result != 0)
        kprintf("IOCTL_GETBLKSZ failed\n");
    else
        kprintf("IOCTL_GETBLKSZ success - blk size = %d\n", pos);
    result = blkio->ops->ctl(blkio, IOCTL_GETLEN, &pos);
    if (result != 0)
        kprintf("IOCTL_GETLEN failed\n");
    else
        kprintf("IOCTL_GETLEN success - image len =  %d\n", pos);
    result = blkio->ops->ctl(blkio, IOCTL_GETPOS, &pos);
    if(result != 0)
        kprintf("IOCTL_GETPOS failed\n");
    else
        kprintf("IOCTL_GETPOS success - image pos = %d\n", pos);
    kprintf("move to 8192 \n");
    result = ioseek(blkio, VERY_LARGE_BUFFER);
    if(result != 0)
        kprintf("ioseek failed\n");
    else
        kprintf("ioseek success - image pos = %d\n", pos);
    kprintf("move to 819200 - should be at eof\n");
    result = ioseek(blkio, VERY_LARGE_BUFFER*100);
    if(result != 0)
        kprintf("ioseek failed\n");
    else
        kprintf("ioseek success\n");
    result = blkio->ops->ctl(blkio, IOCTL_GETPOS, &pos);
    if(result != 0)
        kprintf("IOCTL_GETPOS failed\n");
    else
        kprintf("IOCTL_GETPOS success - image pos = %d\n", pos);
    result = blkio->ops->ctl(blkio, IOCTL_SETLEN, &pos);
    if(result != 0)
        kprintf("IOCTL_SETLEN failed - should fail\n");
    else
        kprintf("IOCTL_SETLEN success\n");
    result = blkio->ops->ctl(blkio, IOCTL_FLUSH, &pos);
    if(result != 0)
        kprintf("IOCTL_FLUSH failed - should fail\n");
    else
        kprintf("IOCTL_FLUSH success\n");

    ioclose(blkio);
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));

        if (cmdbuf[0] == '\0')
            return;
    }
}
