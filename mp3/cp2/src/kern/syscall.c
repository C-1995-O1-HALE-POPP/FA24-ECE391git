// syscall.c - systemcall handler and dispatcher
// 
#include "excp.h"
#include "memory.h"
#include "config.h"
#include "io.h"
#include "thread.h"
#include "error.h"
#include "string.h"
#include "console.h"
#include "elf.h"
#include "thread.h"
#include "trap.h"
#include "scnum.h"
#include "kfs.h"
#include "device.h"
#include "process.h"
#include <stddef.h>
#include <stdint.h>

//           INTERNAL FUNCTION DECLARATIONS
//          
int64_t syscall(struct trap_frame * tfr);

static int sysexit(void);
static int sysexec(int fd);
static int sysmsgout(const char * msg);
static int sysdevopen(int fd, const char * name, int instno);
static int sysfsopen(int fd, const char * name);
static int sysclose(int fd);
static long sysread(int fd, void * buf, size_t bufsz);
static long syswrite(int fd, const void * buf, size_t len);
static int sysioctl(int fd, int cmd, void * arg);


//           EXPORTED FUNCTION DEFINITIONS
//     

void syscall_handler(struct trap_frame * tfr){
    // input: tfr - trap frame
    //
    // output: none
    //
    // side effect: 
    //      handle system call
    //      dispatch system call based on A7
    //  
    tfr->sepc += 4;                     // skip the ecall instruction
    
    tfr->x[TFR_A0] = syscall(tfr);      // dispatch the syscall - return value in A0
}

int64_t syscall(struct trap_frame * tfr) {
    // input: tfr - trap frame
    //
    // output: return value to tfr->A0
    //
    // side effect: 
    //      dispatch system call based on A7
    //
    const uint64_t * const a = &(tfr->x[TFR_A0]);
    // a is a pointer to the first argument of the syscall
    // the following arguments are at a[1], a[2], ...

    // dispatch the syscall
    // solve different case by judging x[TFR_A7]
    switch (tfr->x[TFR_A7]) {
        case SYSCALL_EXIT:
            return sysexit();
        case SYSCALL_EXEC:
            return sysexec((int)a[0]);
        case SYSCALL_MSGOUT:
            return sysmsgout((const char *)a[0]);
        case SYSCALL_DEVOPEN:
            return sysdevopen((int)a[0], (const char *)a[1], (int)a[2]);
        case SYSCALL_FSOPEN:
            return sysfsopen((int)a[0], (const char *)a[1]);
        case SYSCALL_CLOSE:
            return sysclose((int)a[0]);
        case SYSCALL_READ:
            return sysread((int)a[0], (void *)a[1], (size_t)a[2]);
        case SYSCALL_WRITE:
            return syswrite((int)a[0], (const void *)a[1], (size_t)a[2]);
        case SYSCALL_IOCTL:
            return sysioctl((int)a[0], (int)a[1], (void *)a[2]);
        default:
            kprintf("syscall: invalid syscall %d\n", tfr->x[TFR_A7]);
            return -ENOTSUP;
    }
}

static int sysexit(void) {
    // input: none
    //
    // output: none
    //
    // side effect: exit the current thread
    //
    trace("%s()", __func__);
    kprintf("sysexit: Thread %d exiting due to syscall\n", running_thread());
    // exit the process
    process_exit();
    return 0;
}

/*
 * sysmsgout
 *
 * Input:
 * - `msg`: A pointer to the message string to be printed
 *
 * Output:
 * - Returns `0` if the message is successfully validated
 * - Returns a non-zero error code if invalid
 *
 * Description:
 * This function validates a user-space message, logs the message if valid, and
 * prints it to the system output with information about the current thread.
 * If valid, it prints the message along with the thread's name and ID.
 *
 * Side Effects:
 * - Logs the message or an error message to the system output.
 */

static int sysmsgout(const char * msg) {
    int result;
    trace("%s(msg=%p)", __func__, msg);
    result = memory_validate_vstr(msg, PTE_U);
    // judge whether it exists invaild message in it
    if (result != 0){
        kprintf("sysmsgout: invalid message at %x\n", msg);
        return result;
    }
    // print out the output
    kprintf("sysmsgout: Thread <%s:%d> says: %s\n", thread_name(running_thread()), running_thread(), msg);
    return 0;
}

static int sysdevopen(int fd, const char *name, int instno){
    // input: fd - file descriptor
    //        name - device name
    //        instno - instance number
    //
    // output: return 0 on success, relative errcode on failure
    //
    // side effect: open a device
    //
    trace("%s(fd=%d, name=%p, instno=%d)", __func__, fd, name, instno);
    // open a device via io_intf

    // check if fd is valid, if fd<0, find an empty slot
    if(fd >= PROCESS_IOMAX){
        kprintf("sysdevopen: invalid file descriptor %d\n", fd);
        return -EBADFD;
    }else if(fd<0){
        for(int i = 0; i < PROCESS_IOMAX; i++){
            if(current_process()->iotab[i] == NULL){
                fd = i;
                break;
            }
        }
    }else if(current_process()->iotab[fd] != NULL){
        kprintf("sysdevopen: file descriptor %d already open\n", fd);
        return -EBADFD;
    }

    struct io_intf *io = NULL;
    int result = device_open(&io, name, instno);
    if (result < 0){
        kprintf("sysdevopen: failed to open device %s:%d, err code: %d\n", name, instno, result);
        return result;
    }
    // save the io_intf to current process
    struct process* proc = current_process();
    proc->iotab[fd] = io;

    return fd;
}

static int sysfsopen(int fd, const char *name){
    // input: fd - file descriptor
    //        name - file name
    //
    // output: return 0 on success, relative errcode on failure
    //
    // side effect: open a file
    //
    trace("%s(fd=%d, name=%p)", __func__, fd, name);
    // open a file via io_intf
    if(fd >= PROCESS_IOMAX){
        kprintf("sysfsopen: invalid file descriptor %d\n", fd);
        return -EBADFD;
    }else if(fd<0){
        for(int i = 0; i < PROCESS_IOMAX; i++){
            if(current_process()->iotab[i] == NULL){
                fd = i;
                break;
            }
        }
    }else if (current_process()->iotab[fd] != NULL){
        kprintf("sysfsopen: file descriptor %d already open\n", fd);
        return -EBADFD;
    }
    
    struct io_intf *io = NULL;
    int result = fs_open(name, &io);
    if (result < 0){
        kprintf("sysfsopen: failed to open file %s, err code: %d\n", name, result);
        return result;
    }
    // save the io_intf to current process
    struct process* proc = current_process();
    proc->iotab[fd] = io;
    return fd;
}

static int sysclose(int fd){
    // input: fd - file descriptor
    //
    // output: return 0 on success, relative errcode on failure
    //
    // side effect: close a file
    //
    trace("%s(fd=%d)", __func__, fd);
    // get the io_intf from current process
    struct process* proc = current_process();
    struct io_intf *io = proc->iotab[fd];
    // judge whether io is not NULL
    if (io == NULL){
        kprintf("sysclose: file descriptor %d not open\n", fd);
        return -EBADFD;
    }

    io->ops->close(io);
    // clear the io_intf from current process
    proc->iotab[fd] = NULL;
    return 0;
}

static long sysread(int fd, void *buf, size_t bufsz){
    // input: fd - file descriptor
    //        buf - buffer to read into
    //        bufsz - buffer size
    //
    // output: return the number of bytes read on success, relative errcode on failure
    //
    // side effect: read from a file
    //
    trace("%s(fd=%d, buf=%p, bufsz=%d)", __func__, fd, buf, bufsz);
    // get the io_intf from current process
    struct process* proc = current_process();
    struct io_intf *io = proc->iotab[fd];
    // judge whether io is not NULL
    if (io == NULL){
        kprintf("sysread: file descriptor %d not open\n", fd);
        return -EBADFD;
    }

    return io->ops->read(io, buf, bufsz);
}

static long syswrite(int fd, const void *buf, size_t len){
    // input: fd - file descriptor
    //        buf - buffer to write
    //        len - buffer length
    //
    // output: return the number of bytes written on success, relative errcode on failure
    //
    // side effect: write to a file
    //
    trace("%s(fd=%d, buf=%p, len=%d)", __func__, fd, buf, len);
    // get the io_intf from current process
    struct process* proc = current_process();
    struct io_intf *io = proc->iotab[fd];
    // judge whether io is not NULL
    if (io == NULL){
        kprintf("syswrite: file descriptor %d not open\n", fd);
        return -EBADFD;
    }

    return io->ops->write(io, buf, len);
}

static int sysioctl(int fd, int cmd, void *arg){
    // input: fd - file descriptor
    //        cmd - ioctl command
    //        arg - argument
    //
    // output: return 0 on success, relative errcode on failure
    //
    // side effect: ioctl to a file
    //
    trace("%s(fd=%d, cmd=%d, arg=%p)", __func__, fd, cmd, arg);
    // get the io_intf from current process
    struct process* proc = current_process();
    struct io_intf *io = proc->iotab[fd];
    if (io == NULL){
        kprintf("sysioctl: file descriptor %d not open\n", fd);
        return -EBADFD;
    }

    return io->ops->ctl(io, cmd, arg);
}

static int sysexec(int fd){
    // input: fd - file descriptor
    //
    // output: return 0 on success, relative errcode on failure
    //
    // side effect: execute a file
    //
    trace("%s(fd=%d)", __func__, fd);
    // get the io_intf from current process
    struct process* proc = current_process();
    struct io_intf *io = proc->iotab[fd];
    // judge whether io is not NULL
    if (io == NULL){
        kprintf("sysexec: file descriptor %d not open\n", fd);
        return -EBADFD;
    }

    return process_exec(io);
}
