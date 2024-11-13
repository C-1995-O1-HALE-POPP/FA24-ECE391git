//           vioblk.c - VirtIO serial port (console)
//          
#ifndef _VIOBLK_H_
#define _VIOBLK_H_

#include "virtio.h"
#include "intr.h"
#include "halt.h"
#include "heap.h"
#include "io.h"
#include "device.h"
#include "error.h"
#include "string.h"
#include "thread.h"

//           COMPILE-TIME PARAMETERS
//          

#define VIOBLK_IRQ_PRIO 1

//           INTERNAL CONSTANT DEFINITIONS
//          

//           VirtIO block device feature bits (number, *not* mask)

#define VIRTIO_BLK_F_SIZE_MAX       1
#define VIRTIO_BLK_F_SEG_MAX        2
#define VIRTIO_BLK_F_GEOMETRY       4
#define VIRTIO_BLK_F_RO             5
#define VIRTIO_BLK_F_BLK_SIZE       6
#define VIRTIO_BLK_F_FLUSH          9
#define VIRTIO_BLK_F_TOPOLOGY       10
#define VIRTIO_BLK_F_CONFIG_WCE     11
#define VIRTIO_BLK_F_MQ             12
#define VIRTIO_BLK_F_DISCARD        13
#define VIRTIO_BLK_F_WRITE_ZEROES   14

//           INTERNAL TYPE DEFINITIONS
//          

//           All VirtIO block device requests consist of a request header, defined below,
//           followed by data, followed by a status byte. The header is device-read-only,
//           the data may be device-read-only or device-written (depending on request
//           type), and the status byte is device-written.

struct vioblk_request_header {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
}__attribute__((packed));

//           Request type (for vioblk_request_header)

#define VIRTIO_BLK_T_IN             0
#define VIRTIO_BLK_T_OUT            1

//           Status byte values

#define VIRTIO_BLK_S_OK         0
#define VIRTIO_BLK_S_IOERR      1
#define VIRTIO_BLK_S_UNSUPP     2

#define DEFAULT_DISK_SIZE 0
#define VIOBLK_INTR_PRIO 1
#define DIRECT_DESC_NUM 3

#define VQ_INDIRECT_DESC 0
#define VQ_HEADER_DESC 1
#define VQ_DATA_DESC 2
#define VQ_STATUS_DESC 3

#define USED_BUFFER_INTR_NOTIFY 1
#define VIO_DEV_CONF_NOTIFY 1<<1

//           Main device structure.
//          
//           FIXME You may modify this structure in any way you want. It is given as a
//           hint to help you, but you may have your own (better!) way of doing things.

struct vioblk_device {
    volatile struct virtio_mmio_regs * regs;
    struct io_intf io_intf;
    uint16_t instno;
    uint16_t irqno;
    int8_t opened;
    int8_t readonly;

    // optimal block size
    uint32_t blksz;
    // current position
    uint64_t pos;
    // sizeo of device in bytes
    uint64_t size;
    // size of device in blksz blocks
    uint64_t blkcnt;

    struct {
        // signaled from ISR
        struct condition used_updated;

        // We use a simple scheme of one transaction at a time.

        union {
            struct virtq_avail avail;
            char _avail_filler[VIRTQ_AVAIL_SIZE(1)];
        };

        union {
            volatile struct virtq_used used;
            char _used_filler[VIRTQ_USED_SIZE(1)];
        };

        // The first descriptor is an indirect descriptor and is the one used in
        // the avail and used rings. The second descriptor points to the header,
        // the third points to the data, and the fourth to the status byte.

        struct virtq_desc desc[4];
        struct vioblk_request_header req_header;
        uint8_t req_status;
    } vq;

    //           Block currently in block buffer
    uint64_t bufblkno;
    //           Block buffer
    char * blkbuf;
};

//           INTERNAL FUNCTION DECLARATIONS
//          

static int vioblk_open(struct io_intf ** ioptr, void * aux);

static void vioblk_close(struct io_intf * io);

static long vioblk_read (
    struct io_intf * restrict io,
    void * restrict buf,
    unsigned long bufsz);

static long vioblk_write (
    struct io_intf * restrict io,
    const void * restrict buf,
    unsigned long n);

static int vioblk_ioctl (
    struct io_intf * restrict io, int cmd, void * restrict arg);

static void vioblk_isr(int irqno, void * aux);

//           IOCTLs

static int vioblk_getlen(const struct vioblk_device * dev, uint64_t * lenptr);
static int vioblk_getpos(const struct vioblk_device * dev, uint64_t * posptr);
static int vioblk_setpos(struct vioblk_device * dev, const uint64_t * posptr);
static int vioblk_getblksz (
    const struct vioblk_device * dev, uint32_t * blkszptr);

//           EXPORTED FUNCTION DEFINITIONS
//          

//           Attaches a VirtIO block device. Declared and called directly from virtio.c.

extern void vioblk_attach(volatile struct virtio_mmio_regs * regs, int irqno) {
    //           FIXME add additional declarations here if needed
    //  input:
    //      regs: the virtio_mmio_regs struct for the device
    //      irqno: the irq number for the device
    //  output:
    //      none
    //  side effect:
    //      Attaches a VirtIO block device to the system. This function is called from virtio.c

    virtio_featset_t enabled_features, wanted_features, needed_features;
    struct vioblk_device * dev;
    uint_fast32_t blksz, capacity, ro;
    int result;

    assert (regs->device_id == VIRTIO_ID_BLOCK);

    // Signal device that we found the device and then we have a driver
    // set the bits in the status register
    ///////////////////////////////////////

    regs->status |= VIRTIO_STAT_DRIVER;
    // you should know how to acknowledge the device
    //////////////////////////////////////
    //           fence o,io
    __sync_synchronize();

    //           Negotiate features. We need:
    //            - VIRTIO_F_RING_RESET and
    //            - VIRTIO_F_INDIRECT_DESC
    //           We want:
    //            - VIRTIO_BLK_F_BLK_SIZE and
    //            - VIRTIO_BLK_F_TOPOLOGY.

    virtio_featset_init(needed_features);
    virtio_featset_add(needed_features, VIRTIO_F_RING_RESET);
    virtio_featset_add(needed_features, VIRTIO_F_INDIRECT_DESC);
    virtio_featset_init(wanted_features);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_BLK_SIZE);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_TOPOLOGY);
    virtio_featset_add(wanted_features, VIRTIO_BLK_F_RO);
    result = virtio_negotiate_features(regs,
        enabled_features, wanted_features, needed_features);

    if (result != 0) {
        kprintf("%p: virtio feature negotiation failed\n", regs);
        return;
    }

    // If the device provides a block size, use it. Otherwise, use 512.

    if (virtio_featset_test(enabled_features, VIRTIO_BLK_F_BLK_SIZE))
        blksz = regs->config.blk.blk_size;
    else
        blksz = 512;

    debug("%p: virtio block device block size is %lu", regs, (long)blksz);

    // Allocate initialize device struct

    dev = kmalloc(sizeof(struct vioblk_device) + blksz);
    memset(dev, 0, sizeof(struct vioblk_device));

    // FIXME Finish initialization of vioblk device here

    // use feature test to get more info

    //////////////////////////////////
    // capacity is the maxium blkno //
    //////////////////////////////////

    capacity = regs->config.blk.capacity;
    debug("%p: virtio block device capacity is %lu", regs, (long)blksz);
    ro = 0; // 0 - rw, 1 - ro
    ro = virtio_featset_test(enabled_features, VIRTIO_BLK_F_RO);
    debug("%p: virtio block device is %s", regs, ro ? "read-only" : "read-write");

    // define pointer to ops, and set its context
    static const struct io_ops vioblk_ops ={
        .close = vioblk_close,
        .read = vioblk_read,
        .write = vioblk_write,
        .ctl = vioblk_ioctl
    };

    dev->regs = regs;
    dev->irqno = irqno;
    dev->opened = 0;
    dev->readonly = ro;
    dev->blksz = blksz;
    dev->pos = 0;
    dev->size = capacity*blksz;
    dev->blkcnt = capacity;
    dev->io_intf.ops = &vioblk_ops;
    dev->blkbuf = (void*)((void*)dev + sizeof(struct vioblk_device));
    // fills out the descriptors in the virtq struct
    condition_init(&dev->vq.used_updated, "vq_used_updated");
    // desc[0] -> desc[1], desc[2], desc[3] indirect, and uses the avail and used rings in vq
    // first one is the indirect descriptor
    dev->vq.desc[VQ_INDIRECT_DESC] = (struct virtq_desc) {
        .addr = (uint64_t)&(dev->vq.desc[1]),
        .len = sizeof(dev->vq.desc[1]) + sizeof(dev->vq.desc[2]) + sizeof(dev->vq.desc[3]),
        .flags = VIRTQ_DESC_F_INDIRECT,
    };
    // second one is the header - first direct
    dev->vq.desc[VQ_HEADER_DESC] = (struct virtq_desc) {
        .addr = (uint64_t)&(dev->vq.req_header),
        .len = sizeof(dev->vq.req_header),
        .flags = VIRTQ_DESC_F_NEXT,
        .next = VQ_DATA_DESC - 1 // we have first one indirect
    };
    // third one is the data
    dev->vq.desc[VQ_DATA_DESC] = (struct virtq_desc) {
        // data put in latest block buffer
        .addr = (uint64_t)(dev->blkbuf),
        .len = blksz,
        .flags = VIRTQ_DESC_F_NEXT, // we need to modifythis based on request
        .next = VQ_STATUS_DESC - 1
    };
    // fourth one is the status
    dev->vq.desc[VQ_STATUS_DESC] = (struct virtq_desc) {
        .addr = (uint64_t)&dev->vq.req_status,
        .len = sizeof(uint8_t),
        .flags = VIRTQ_DESC_F_WRITE,
        .next = 0   // no need
    };
    __sync_synchronize();

    // initialize the indirect virtq, which usues the avail and used rings in vq
    virtio_attach_virtq(dev->regs, 0, 1, (uint64_t)&dev->vq.desc[0], (uint64_t)&dev->vq.used, (uint64_t)&dev->vq.avail);
    __sync_synchronize();
    // register the interrupt handler and device to the system
    intr_register_isr(dev->irqno, VIOBLK_INTR_PRIO, vioblk_isr, dev);
    dev->instno = device_register("blk", &vioblk_open, dev);

    regs->status |= VIRTIO_STAT_FEATURES_OK;
    regs->status |= VIRTIO_STAT_ACKNOWLEDGE;
    regs->status |= VIRTIO_STAT_DRIVER;
    regs->status |= VIRTIO_STAT_DRIVER_OK;
    //           fence o,oi
    __sync_synchronize();
}

int vioblk_open(struct io_intf ** ioptr, void * aux) {
    //           FIXME your code here
    // input:
    //     ioptr: a pointer to the io interface to be returned
    //     aux: a pointer to the vioblk device
    // output:
    //     return 0 on success, -EBUSY on failure
    // side effect:
    //     Opens the vioblk device and returns the io interface to the device.
    assert(ioptr!=NULL && aux!=NULL);
    struct vioblk_device * dev = aux;
    if(dev->opened)
        return -EBUSY;

    // Sets the virtq_avail and virtq used queues such that they are available for use
    virtio_enable_virtq(dev->regs, 0);
    __sync_synchronize();
    // Enables the interupt line for the virtio device
    intr_enable_irq(dev->irqno);
    // sets necessary flags in vioblk device.
    dev->opened = 1;
    // Return the IO operations to ioptr
    *ioptr =  &(dev->io_intf);

    return 0;
}

//           Must be called with interrupts enabled to ensure there are no pending
//           interrupts (ISR will not execute after closing).

void vioblk_close(struct io_intf * io) {
    //           FIXME your code here
    // input:
    //     io: the io interface to the file to close
    // output:
    //     none
    // side effect:
    //     Closes the vioblk device and marks the device as closed.

    assert(intr_enabled());
    struct vioblk_device * dev = (void*)io - offsetof(struct vioblk_device, io_intf);
    if(!dev->opened)
        return;
    // reset the queue
    virtio_reset_virtq(dev->regs, 0);
    __sync_synchronize();
    // set flags
    dev->opened = 0;
    // Disables the interrupt line for the virtio device
    intr_disable_irq(dev->irqno);
    __sync_synchronize();
    // reset status flags
    dev->regs->status &= ~VIRTIO_STAT_DRIVER_OK;
    dev->regs->status &= ~VIRTIO_STAT_DRIVER;
    dev->regs->status &= ~VIRTIO_STAT_ACKNOWLEDGE;
    dev->regs->status &= ~VIRTIO_STAT_FEATURES_OK;
    __sync_synchronize();

}

long vioblk_read (
    struct io_intf * restrict io,
    void * restrict buf,
    unsigned long bufsz)
{
    //           FIXME your code here
    // input:
    //     io: the io interface to the blk to read
    //     buf: the buffer to read into
    //     bufsz: the number of bytes to read
    // output:
    //     return the number of bytes read on success, relative errcode on failure
    // side effect:
    //     reads bufsz bytes from the blk associated with io into buf.
    //     Updates metadata in the blk struct as appropriate.



    // here we assume the read operation is aligned with the block size
    // which is assured in fs


    // get device and block number and offset    
    struct vioblk_device * dev = (void*)io - offsetof(struct vioblk_device, io_intf);
    
    // sanity check
    if(dev->pos > dev->size)
        return -EINVAL;
    else if(dev->pos + bufsz > dev->size)
        bufsz = dev->size - dev->pos;
    if(bufsz == 0){
        kprintf("vioblk_read: bufsz is 0");
        return 0;   
    }

    // read the block one by one
    long bytes_read = 0;
    int s = intr_disable();

    uint64_t start_blk = dev->pos / dev->blksz;
    uint64_t end_blk = (dev->pos + bufsz - 1) / dev->blksz;
    dev->bufblkno = start_blk;
    while(dev->bufblkno <= end_blk){
        // first one is the indirect descriptor
        dev->vq.desc[VQ_INDIRECT_DESC] = (struct virtq_desc) {
            .addr = (uint64_t)&(dev->vq.desc[1]),
            .len = sizeof(dev->vq.desc[1]) + sizeof(dev->vq.desc[2]) + sizeof(dev->vq.desc[3]),
            .flags = VIRTQ_DESC_F_INDIRECT,
        };
        // second one is the header - first direct
        dev->vq.desc[VQ_HEADER_DESC] = (struct virtq_desc) {
            .addr = (uint64_t)&(dev->vq.req_header),
            .len = sizeof(dev->vq.req_header),
            .flags = VIRTQ_DESC_F_NEXT,
            .next = VQ_DATA_DESC - 1 // we have first one indirect
        };
        // third one is the data
        dev->vq.desc[VQ_DATA_DESC] = (struct virtq_desc) {
            // data put in latest block buffer
            .addr = (uint64_t)(dev->blkbuf),
            .len = dev->blksz,
            .flags = VIRTQ_DESC_F_NEXT | VIRTQ_DESC_F_WRITE, // we need to modifythis based on request
            .next = VQ_STATUS_DESC - 1
        };
        // fourth one is the status
        dev->vq.desc[VQ_STATUS_DESC] = (struct virtq_desc) {
            .addr = (uint64_t)&dev->vq.req_status,
            .len = sizeof(uint8_t),
            .flags = VIRTQ_DESC_F_WRITE,
            .next = 0   // no need
        };
        __sync_synchronize();

        // set registers to request read the block
        // set vq.req_header
        dev->vq.req_header = (struct vioblk_request_header) {
            .type = VIRTIO_BLK_T_IN,
            .reserved = 0,
            .sector = dev->bufblkno
        };
        // send request to the available ring via indirect descriptor
        dev->vq.avail.ring[0] = 0; // 0 - index of the first descriptor, 0 for direct index
        dev->vq.avail.idx++;
        __sync_synchronize();
        // notify the device
        virtio_notify_avail(dev->regs, 0);

        // please wait until all request processed
        while(dev->vq.used.idx != dev->vq.avail.idx){
            condition_wait(&dev->vq.used_updated);
        }
        // check the status
        if(dev->vq.req_status != VIRTIO_BLK_S_OK){
            debug("vioblk_read: request failed");
            return -EIO;
        }
        // copy the data to the buffer
        memcpy(buf + bytes_read, dev->blkbuf, dev->blksz);
        dev->pos += dev->blksz;
        bytes_read += dev->blksz;
        dev->bufblkno++;
    }
    intr_restore(s);
    return bytes_read;
}

long vioblk_write (
    struct io_intf * restrict io,
    const void * restrict buf,
    unsigned long n)
{
    //           FIXME your code here
    // input:
    //     io: the io interface to the blk to write
    //     buf: the buffer to write from
    //     n: the number of bytes to write
    // output:
    //     return the number of bytes written on success, relative errcode on failure
    // side effect:
    //     writes n bytes from the blk associated with io into buf.

    // here we assume the write operation is aligned with the block size
    // which is assured in fs

    // get device and block number and offset
    struct vioblk_device * dev = (void*)io - offsetof(struct vioblk_device, io_intf);

    // is ro?
    if(dev->readonly)
        return -ENOTSUP;
    // sanity check
    if(dev->pos > dev->size)
        return -EINVAL;
    else if(dev->pos + n > dev->size)
        n = dev->size - dev->pos;
    if(n == 0){
        debug("vioblk_write: n is 0");
        return 0;
    }
    if(n%dev->blksz != 0 || dev->pos%dev->blksz != 0){
        debug("vioblk_write: n or pos not aligned with block size");
        return -EIO;
    }
    // reset vq avail for the new request
    // write the block one by one
    long bytes_written = 0;
    int s = intr_disable();
    uint64_t start_blk = dev->pos / dev->blksz;
    uint64_t end_blk = (dev->pos + n - 1) / dev->blksz; // eg 0-4095, 4096-8191, we have 4096 blks! so -1

    dev->bufblkno = start_blk;
    while(dev->bufblkno <= end_blk){
        // prepare the buffer
        memcpy(dev->blkbuf, buf + bytes_written, dev->blksz);
        // first one is the indirect descriptor
        dev->vq.desc[VQ_INDIRECT_DESC] = (struct virtq_desc) {
            .addr = (uint64_t)&(dev->vq.desc[1]),
            .len = sizeof(dev->vq.desc[1]) + sizeof(dev->vq.desc[2]) + sizeof(dev->vq.desc[3]),
            .flags = VIRTQ_DESC_F_INDIRECT,
        };
        // second one is the header - first direct
        dev->vq.desc[VQ_HEADER_DESC] = (struct virtq_desc) {
            .addr = (uint64_t)&(dev->vq.req_header),
            .len = sizeof(dev->vq.req_header),
            .flags = VIRTQ_DESC_F_NEXT,
            .next = VQ_DATA_DESC - 1 // we have first one indirect
        };
        // third one is the data
        dev->vq.desc[VQ_DATA_DESC] = (struct virtq_desc) {
            // data put in latest block buffer
            .addr = (uint64_t)(dev->blkbuf),
            .len = dev->blksz,
            .flags = VIRTQ_DESC_F_NEXT, // we need to modifythis based on request
            .next = VQ_STATUS_DESC - 1
        };
        // fourth one is the status
        dev->vq.desc[VQ_STATUS_DESC] = (struct virtq_desc) {
            .addr = (uint64_t)&dev->vq.req_status,
            .len = sizeof(uint8_t),
            .flags = VIRTQ_DESC_F_WRITE,
            .next = 0   // no need
        };
        __sync_synchronize();

        // set registers to request read the block
        // set vq.req_header
        dev->vq.req_header = (struct vioblk_request_header) {
            .type = VIRTIO_BLK_T_OUT,
            .reserved = 0,
            .sector = dev->bufblkno
        };
        // send request to the available ring via indirect descriptor
        dev->vq.avail.ring[0] = 0; // 0 - index of the first descriptor, 0 for direct index
        dev->vq.avail.idx++;
        __sync_synchronize();
        // notify the device
        virtio_notify_avail(dev->regs, 0);

        // please wait until all request processed
        while(dev->vq.used.idx != dev->vq.avail.idx){
            condition_wait(&dev->vq.used_updated);
        }
        // check the status
        if(dev->vq.req_status != VIRTIO_BLK_S_OK){
            debug("vioblk_write: request failed");
            return -EIO;
        }
        dev->pos += dev->blksz;
        bytes_written += dev->blksz;
        dev->bufblkno++;
    }
    intr_restore(s);
    return bytes_written;
}

int vioblk_ioctl(struct io_intf * restrict io, int cmd, void * restrict arg) {
    struct vioblk_device * const dev = (void*)io -
        offsetof(struct vioblk_device, io_intf);
    
    trace("%s(cmd=%d,arg=%p)", __func__, cmd, arg);
    
    switch (cmd) {
    case IOCTL_GETLEN:
        return vioblk_getlen(dev, arg);
    case IOCTL_GETPOS:
        return vioblk_getpos(dev, arg);
    case IOCTL_SETPOS:
        return vioblk_setpos(dev, arg);
    case IOCTL_GETBLKSZ:
        return vioblk_getblksz(dev, arg);
    default:
        return -ENOTSUP;
    }
}

void vioblk_isr(int irqno, void * aux) {
    //           FIXME your code here
    // input:
    //     irqno: the interrupt number
    //     aux: a pointer to the vioblk device
    // output:
    //     none
    // side effect:
    //     sets the appropriate device registers and
    //     wakes the thread up after waiting for the disk to finish servicing a request.
    struct vioblk_device * dev = aux;
    if(dev == NULL || aux == NULL){
        debug("vioblk_isr: invalid input");
        return;
    }
    if(irqno != dev->irqno){
        debug("vioblk_isr: invalid irqno");
        return;
    }
    uint32_t status = dev->regs->interrupt_status;
    // check intrupt status
    if(status & USED_BUFFER_INTR_NOTIFY){
        // clear the interrupt and wake up the thread
        dev->regs->interrupt_ack |= USED_BUFFER_INTR_NOTIFY;
        condition_broadcast(&dev->vq.used_updated);
    }

    if(status & VIO_DEV_CONF_NOTIFY){
        // clear the interrupt
        dev->regs->interrupt_ack |= VIO_DEV_CONF_NOTIFY;
        // nothing is waiting on this, maybe
    }

}

int vioblk_getlen(const struct vioblk_device * dev, uint64_t * lenptr) {
    //           FIXME your code here
    // input:
    //     dev: the vioblk device
    //     lenptr: pointer to return value
    // output:
    //     return 0 on success, relative errcode on failure
    // side effect:
    //     Gets the length of the device associated with io in pointer.
    if(lenptr == NULL || dev == NULL){
        debug("fs_getlen: invalid input");
        return -EINVAL;
    }
    *lenptr = dev->size;
    return 0;
}

int vioblk_getpos(const struct vioblk_device * dev, uint64_t * posptr) {
    //           FIXME your code here
    // input:
    //     dev: the vioblk device
    //     posptr: pointer to return value
    // output:
    //     return 0 on success, relative errcode on failure
    // side effect:
    //     Gets the position of the device associated with io in pointer.
    if (posptr == NULL || dev == NULL){
        debug("fs_getpos: invalid input");
        return -EINVAL;
    }

    *posptr = dev->pos;
    return 0;
}

int vioblk_setpos(struct vioblk_device * dev, const uint64_t * posptr) {
    //           FIXME your code here
    // input:
    //     dev: the vioblk device
    //     posptr: pointer contains the position to set
    // output:
    //     return 0 on success, relative errcode on failure
    // side effect:
    //     Sets the position of the device associated with io.

    if (posptr == NULL || dev == NULL){
        debug("fs_setpos: invalid input");
        return -EINVAL;
    }
    // sanity check
    if(*posptr > dev->size){
        debug("fs_setpos: pos > size");
        return -EINVAL;
    }
    if (*posptr % dev->blksz != 0){
        debug("fs_setpos: pos not aligned with block size");
        return -EINVAL;
    }
    dev->pos = *posptr;
    return 0;
}

int vioblk_getblksz (
    const struct vioblk_device * dev, uint32_t * blkszptr)
{
    //           FIXME your code here
    // input:
    //     dev: the vioblk device
    //     blkszptr: pointer to return value
    // output:
    //     return 0 on success, relative errcode on failure
    // side effect:
    //     Gets the block size of the device associated with io in pointer.
    if (blkszptr == NULL || dev == NULL){
        debug("fs_getblksize: invalid input");
        return -EINVAL;
    }
    *blkszptr = dev->blksz;
    return 0;
}
#endif