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
#include "fs.h"
#include "string.h"

struct io_lit lit;
//           end of kernel image (defined in kernel.ld)
extern char _kimg_end[];
extern char _companion_f_start[];
extern char _companion_f_end[];
#define VERY_LARGE_BUFFER 8192
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

static void shell_main(struct io_intf * termio);

void main(void) {
    struct io_intf * termio;
    struct io_intf * blkio;
    void * mmio_base;
    int result;
    int i;

    console_init();
    intr_init();
    devmgr_init();
    thread_init();
    timer_init();
    fs_init();

    heap_init(_kimg_end, (void*)USER_START);

    //           Attach NS16550a serial devices

    for (i = 0; i < 2; i++) {
        mmio_base = (void*)UART0_IOBASE;
        mmio_base += (UART1_IOBASE-UART0_IOBASE)*i;
        uart_attach(mmio_base, UART0_IRQNO+i);
    }

    intr_enable();
    timer_start();

    blkio = iolit_init(&lit, _companion_f_start, _companion_f_end-_companion_f_start);
    result = fs_mount(blkio);

    kprintf("Mounted blk0\n");

    if (result != 0)
        panic("fs_mount failed\n");

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
    //void (*exe_entry)(struct io_intf*);
    //struct io_intf * exeio;
    char cmdbuf[9];
    //int tid;
    int result;
    
    termio = ioterm_init(&ioterm, termio_raw);
    char buf[VERY_LARGE_BUFFER];
    struct io_intf * testio;
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));

        if (cmdbuf[0] == '\0')
            break;

    }
    kprintf("Testing fs_open\n");
    kprintf("Testing non existing fs_open\n");
    result = fs_open("i_am_not_here", &testio);
    if (result == -ENOENT)
        kprintf("fs_open failed\n");

    kprintf("Testing existing fs_open\n");
    result = fs_open("read_me", &testio);
    if (result != 0)
        kprintf("fs_open failed\n");
    else
        kprintf("fs_open success\n");
    kprintf("Testing fs_read\n");

    result = ioread(testio, buf, 5);  // magic number 5, just test
    if (result != 5)
        kprintf("fs_read failed\n");
    else{
        kprintf("fs_read success\n");
        kprintf("%s", buf);
    }
    kprintf("\n");
    kprintf("Testing fs_read and ioctl - go back to over EOF\n");
    ioseek(testio, 0);
    result = ioread(testio, buf, 512);
    if (result <= 0)
        kprintf("fs_read failed\n");
    else{
        kprintf("fs_read success\n");
        kprintf("%s", buf);
    }
    kprintf("\n");
    kprintf("close the file and reset buffer\n");
    memset(buf, 0, VERY_LARGE_BUFFER);
    ioclose(testio);
    kprintf("Testing fs_read after close\n");
    result = ioread(testio, buf, 5);  // magic number 5, just test
    if (result < 0)
        kprintf("fs_read failed\n");
    else
        kprintf("fs_read success\n");
    kprintf("\n");

    kprintf("Testing re_open\n");
    result = fs_open("read_me", &testio);
    if (result != 0)
        kprintf("fs_open failed\n");
    else
        kprintf("fs_open success\n");
    kprintf("Testing fs_read\n");
    result = ioread(testio, buf, 5);  // magic number 5, just test
    if (result != 5)
        kprintf("fs_read failed\n");
    else{
        kprintf("fs_read success\n");
        kprintf("%s", buf);
    }
    kprintf("go on to read more\n");
    result = ioread(testio, buf, 5);  // magic number 5, just test
    if (result != 5)
        kprintf("fs_read failed\n");
    else{
        kprintf("fs_read success\n");
        kprintf("%s", buf);
    }
    kprintf("read from middle\n");;
    ioseek(testio, 50);                // magic number 50, just test
    result = ioread(testio, buf, 5);  // magic number 5, just test
    if (result != 5)
        kprintf("fs_read failed\n");
    else{
        kprintf("fs_read success\n");
        kprintf("%s", buf);
    }
    kprintf("read from EOF\n");
    ioseek(testio, 111);                // magic number 111, just test, file length = 96
    result = ioread(testio, buf, 5);  // magic number 5, just test
    if (result != 0)
        kprintf("fs_read failed\n");
    else
        kprintf("fs_read success\n");
    kprintf("\n");
    kprintf("close the file\n");
    ioclose(testio);
    kprintf("enter in terminal to go on\n");
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));

        if (cmdbuf[0] == '\0')
            break;

    }
    kprintf("read large file\n");
    result = fs_open("i_am_long", &testio);
    if (result != 0)
        kprintf("fs_open failed\n");
    else
        kprintf("fs_open success\n");
    memset(buf, 0, VERY_LARGE_BUFFER);
    result = ioread(testio, buf, VERY_LARGE_BUFFER);
    if (result <= 0)
        kprintf("fs_read failed\n");
    else{

        kprintf("%s", buf);
        kprintf("fs_read success reading %d bytes\n", result);
    }
    kprintf("\n");
    kprintf("close the file and reset buf\n");
    kprintf("click enter to go on\n");
    ioclose(testio);
    memset(buf, 0, VERY_LARGE_BUFFER);
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));

        if (cmdbuf[0] == '\0')
            break;

    }
    kprintf("write 5 bytes to write_me, which is originally empty\n");
    result = fs_open("write_me", &testio);
    if (result != 0)
        kprintf("fs_open failed\n");
    else
        kprintf("fs_open success\n");
    char write_buf[5] = "haha ";
    result = iowrite(testio, write_buf, 5);
    if (result != 5)
        kprintf("fs_write failed\n");
    else
        kprintf("fs_write success\n");
    kprintf("read from write_me\n");
    ioseek(testio, 0);
    result = ioread(testio, buf, 8);
    if (result != 5)
        kprintf("fs_read failed\n");
    else{
        kprintf("fs_read success\n");
        kprintf("%s", buf);
    }    

    kprintf("go on writing more - 20 times\n");
    for (int i = 0; i < 20; i++){
        result = iowrite(testio, write_buf, 5);
        if (result != 5)
            kprintf("fs_write failed\n");
        else
            kprintf("fs_write success\n");
    }
    kprintf("read from write_me\n");
    ioseek(testio, 0);
    result = ioread(testio, buf, VERY_LARGE_BUFFER);
    kprintf("read %d bytes\n", result);
    kprintf("fs_read success\n");
    kprintf("%s", buf);
    
    kprintf("write from middle - move to idx = 3 and write once\n");
    ioseek(testio, 3);
    result = iowrite(testio, write_buf, 5);
    if (result != 5)
        kprintf("fs_write failed\n");
    else
        kprintf("fs_write success\n");
    kprintf("read from write_me\n");

    kprintf("reset pos and write 1000 times - over 4096 blksz, should be limited by capacity\n");
    ioseek(testio, 0);
    for (int i = 0; i < 3000; i++){
        result = iowrite(testio, write_buf, 5);
        if (result != 5){
            kprintf("fs_write failed at pos = %d\n", 5 * i);
            break;
        }
    }
    kprintf("read from write_me\n");
    ioseek(testio, 0);
    result = ioread(testio, buf, VERY_LARGE_BUFFER);
    kprintf("read %d bytes\n", result);

    kprintf("close the file and reset buf\n");
    kprintf("click enter to go on\n");
    ioclose(testio);
    memset(buf, 0, VERY_LARGE_BUFFER);
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));

        if (cmdbuf[0] == '\0')
            break;

    }

    kprintf("read and write large file\n");
    result = fs_open("i_am_long", &testio);
    if (result != 0)
        kprintf("fs_open failed\n");
    else
        kprintf("fs_open success\n");

    kprintf("write 5 bytes to i_am_long at its end\n");
    uint64_t pos;
    result = testio->ops->ctl(testio, IOCTL_GETLEN, &pos);
    if (result != 0)
        kprintf("fs_ioctl failed\n");
    else
        kprintf("fs_ioctl success\n");
    kprintf("file length = %d\n", pos);
    ioseek(testio, pos);
    char write_buf1[5] = "nana ";
    result = iowrite(testio, write_buf1, 5);
    if (result != 5)
        kprintf("fs_write failed\n");
    else
        kprintf("fs_write success\n");
    kprintf("read from i_am_long\n");
    kprintf("click enter to go on\n");
    memset(buf, 0, VERY_LARGE_BUFFER);
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));

        if (cmdbuf[0] == '\0')
            break;
    }
    ioseek(testio, 3000);
    memset(buf, 0, VERY_LARGE_BUFFER);
    result = ioread(testio, buf, 3000);
    if(result <= 0)
        kprintf("fs_read failed\n");
    else{
        kprintf("fs_read success\n");
    }
    kprintf("read %d bytes - should be 3000\n", result);

    kprintf("print cross-block rw\n");
    ioseek(testio, SIZE_OF_4K_BLK-2); // 2- magic number for test
    memset(buf, 0, VERY_LARGE_BUFFER);
    result = ioread(testio, buf, 5);
    if(result <= 0)
        kprintf("fs_read failed\n");
    else{
        kprintf("fs_read success\n");
        kprintf("%s", buf);
    }

    ioseek(testio, SIZE_OF_4K_BLK-2); // 2- magic number for test
    result = iowrite(testio, write_buf1, 5);
    if (result != 5)
        kprintf("fs_write failed\n");
    else
        kprintf("fs_write success\n");
    kprintf("read from i_am_long\n");
    ioseek(testio, SIZE_OF_4K_BLK-2); // 2- magic number for test
    ioread(testio, buf, 5);
    if(result <= 0)
        kprintf("fs_read failed\n");
    else{
        kprintf("fs_read success\n");
        kprintf("%s", buf);
    }
    kprintf("test ioctls\n");
    result = testio->ops->ctl(testio, IOCTL_GETBLKSZ, &pos);
    if (result != 0)
        kprintf("IOCTL_GETBLKSZ failed\n");
    else
        kprintf("IOCTL_GETBLKSZ success - blk size = %d\n", pos);
    result = testio->ops->ctl(testio, IOCTL_GETLEN, &pos);
    if (result != 0)
        kprintf("IOCTL_GETLEN failed\n");
    else
        kprintf("IOCTL_GETLEN success - image len =  %d\n", pos);
    result = testio->ops->ctl(testio, IOCTL_GETPOS, &pos);
    if(result != 0)
        kprintf("IOCTL_GETPOS failed\n");
    else
        kprintf("IOCTL_GETPOS success - image pos = %d\n", pos);
    kprintf("move to 8192 \n");
    result = ioseek(testio, VERY_LARGE_BUFFER);
    if(result != 0)
        kprintf("ioseek failed\n");
    else
        kprintf("ioseek success - image pos = %d\n", pos);
    kprintf("move to 819200 - should be at eof\n");
    result = ioseek(testio, VERY_LARGE_BUFFER*100);
    if(result != 0)
        kprintf("ioseek failed\n");
    else
        kprintf("ioseek success\n");
    result = testio->ops->ctl(testio, IOCTL_GETPOS, &pos);
    if(result != 0)
        kprintf("IOCTL_GETPOS failed\n");
    else
        kprintf("IOCTL_GETPOS success - image pos = %d\n", pos);
    result = testio->ops->ctl(testio, IOCTL_SETLEN, &pos);
    if(result != 0)
        kprintf("IOCTL_SETLEN failed - should fail\n");
    else
        kprintf("IOCTL_SETLEN success\n");
    result = testio->ops->ctl(testio, IOCTL_FLUSH, &pos);
    if(result != 0)
        kprintf("IOCTL_FLUSH failed - should fail\n");
    else
        kprintf("IOCTL_FLUSH success\n");


    ioputs(termio, "Enter executable name or \"exit\" to exit");
    for (;;) {
        ioprintf(termio, "CMD> ");
        ioterm_getsn(&ioterm, cmdbuf, sizeof(cmdbuf));

        if (cmdbuf[0] == '\0')
            continue;
        if(strcmp(cmdbuf, "exit") == 0)
            break;

    }
}
