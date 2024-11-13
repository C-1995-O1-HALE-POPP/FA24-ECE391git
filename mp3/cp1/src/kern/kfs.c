//           kfs.c 
//          

#include "string.h"
#include "error.h"
#include "io.h"
#include "fs.h"
#include "kfs.h"
#include "console.h"
#include "heap.h"

#include <stddef.h>
#include <stdint.h>
#define EOF 0
//           INTERNAL TYPE DEFINITIONS
//          

const struct io_ops fs_io_ops = {
    .close = fs_close,
    .read = fs_read,
    .write = fs_write,
    .ctl = fs_ioctl
};
static struct fs fs;
static struct file_desc* file_desc_head;
static uint64_t start_of_data_blks;
//           INTERNAL FUNCTION DECLARATIONS
//          


//           EXPORTED FUNCTION DEFINITIONS
//     
char fs_initialized;
extern void fs_init(void){
    // input:
    //     none
    // output:
    //     none
    // side effect:
    // Initializes the filesystem. This function should be called once at boot time.
    fs_initialized = FS_INITIALIZED;
    kprintf("fs_init: fs initialized\n");
}
extern int fs_mount(struct io_intf* io){
    // input:
    //     io: the io interface to the block device
    // output:
    //     return 0 on success, -1 on failure
    // side effect:
    // Takes an io intf* to the filesystem provider and sets up the filesystem for future fs open operations.
    // Once you complete this checkpoint, io will come from the vioblk device struct.


    // initialize the device
    struct boot_blk boot_blk;
    uint64_t pos = POS_BOOT_BLK;
    ioseek(io, pos);
    ioread_full(io, (void*)&boot_blk, SIZE_OF_4K_BLK);

    // create a new fs based on the boot block
    fs.dev_io_intf = io;
    fs.boot_blk = boot_blk;
    file_desc_head = (struct file_desc*) NULL;
    start_of_data_blks = (fs.boot_blk.num_inodes+1) * SIZE_OF_4K_BLK;
    //fs_initialized = FS_INITIALIZED;
    kprintf("fs_mount: fs mounted successfully\n");
    return 0;
}

extern int fs_open(const char* name, struct io_intf** io){
    // input:
    //     name: the name of the file to open
    //     io: a pointer to the io_intf* to be returned
    // output:
    //     return 0 on success, relative errcode on failure
    // side effect:
    // Takes the name of the file to be opened and modifies the given pointer to contain the io intf of the
    // file. This function should also associate a specific file struct with the file and mark it as in-use. The
    // user program will use this io intf to interact with the file.
    #if 0
    if (fs_initialized != FS_INITIALIZED){
        kprintf("fs_open: fs not initialized");
        return -EINVAL;
    }
    #endif
    // find the inode of the file
    uint32_t inodes = find_inode_by_name(name);
    if(inodes == fs.boot_blk.num_inodes){
        kprintf("fs_open: file not found\n");
        return -ENOENT;
    }
    // read it
    struct inode inode;
    uint64_t inode_pos = SIZE_OF_4K_BLK * (START_IDX_OF_INODE + inodes);
    ioseek(fs.dev_io_intf, inode_pos);
    ioread_full(fs.dev_io_intf, (void*)&inode, SIZE_OF_4K_BLK);

    // create a new file descriptor
    struct io_intf* io_intf = (struct io_intf*)kmalloc(sizeof(struct io_intf));
    if (io_intf == NULL){
        kprintf("fs_open: kmalloc failed\n");
        return -EINVAL;
    }
    io_intf->ops = &fs_io_ops;
    struct file_desc* new_file_desc = (struct file_desc*)kmalloc(sizeof(struct file_desc));
    if (new_file_desc == NULL){
        kprintf("fs_open: kmalloc failed\n");
        return -EINVAL;
    }
    new_file_desc->io_intf = io_intf;
    new_file_desc->pos = FILE_START;
    new_file_desc->size = (uint64_t)inode.length;
    new_file_desc->inodes = (uint64_t)inodes;
    new_file_desc->flag = FILE_IN_USE;
    new_file_desc->next = file_desc_head;
    file_desc_head = new_file_desc;
    
    *io = io_intf;
    kprintf("fs_open: file opened successfully\n");
    return 0;
}

void fs_close(struct io_intf* io){
    // input:
    //     io: the io interface to the file to close
    // output:
    //     none
    // side effect:
    // Marks the file struct associated with io as unused
    /*
    if (fs_initialized != FS_INITIALIZED){
        kprintf("fs_close: fs not initialized");
        return;
    }*/

    struct file_desc* current = find_file_desc_by_io(io);
    if (current != NULL){
        current->flag = FILE_NOT_IN_USE;
        // do we need to free the io_intf and desc?
        kfree(current->io_intf);
        current->io_intf = NULL;
        
        kprintf("fs_close: file closed successfully\n");
        return;
    }

    kprintf("fs_close: file with given io not found\n");
    return;
}

long fs_read(struct io_intf* io, void* buf, unsigned long n){
    // input:
    //     io: the io interface to the file to read
    //     buf: the buffer to read into
    //     n: the number of bytes to read
    // output:
    //     return the number of bytes read on success, relative errcode on failure
    // side effect:
    // reads n bytes from the file associated with io into buf. 
    // Updates metadata in the file struct as appropriate. Use fs_open to get io.
    /*
    if (fs_initialized != FS_INITIALIZED){
        kprintf("fs_read: fs not initialized");
        return -EINVAL;
    }
*/
    // check if the file exists
    struct file_desc* current = find_file_desc_by_io(io);
    if (current == NULL){
        kprintf("fs_read: file not found\n");
        return -ENOENT;
    }

    // check if the file is in use
    if (current->flag == FILE_NOT_IN_USE){
        kprintf("fs_read: file not in use\n");
        return -EIO;
    }

    uint32_t inodes = current->inodes;
    uint32_t pos = current->pos;
    uint32_t size = current->size;

    // sanity check
    if (pos > size){
        kprintf("fs_read: pos > size\n");
        return -EINVAL;
    }else if(pos + n > size){
        n = size - pos;
    }

    // read the data blk index from the inode
    struct inode inode;
    uint64_t inode_pos = SIZE_OF_4K_BLK * (START_IDX_OF_INODE + inodes);
    ioseek(fs.dev_io_intf, inode_pos);
    ioread_full(fs.dev_io_intf, (void*)&inode, SIZE_OF_4K_BLK);
    
    // get the index for the data blocks and offset
    // also, the physical location for data blocks is hashed in inode.data_blks,
    // not in order, access by reading the list
    uint32_t num_blks = inode.length / SIZE_OF_4K_BLK + 1; // upper bound (e.g. if length < 4096)
    // prevent unaligned warning
    uint32_t data_blk_array[num_blks];
    for (int i = 0; i < num_blks; i++){
        data_blk_array[i] = inode.data_blks[i];
    }
    if(n == 0){
        return EOF;
    }
    uint32_t start_blk = pos / SIZE_OF_4K_BLK;
    uint32_t start_offset = pos % SIZE_OF_4K_BLK;
    uint32_t end_blk = (pos + n - 1) / SIZE_OF_4K_BLK;
    uint32_t end_offset = (pos + n - 1) % SIZE_OF_4K_BLK;
    // again, sanity check
    if (start_blk >= num_blks || end_blk >= num_blks){
        kprintf("fs_read: start_blk or end_blk >= num_blks\n");
        return -EINVAL;
    }
    // read the data blocks one by one and copy the data to the buffer based on offset
    long bytes_read = 0;    // this records the number of bytes read, as index in buf, incrment after each read
    for(int i = start_blk; i <= end_blk; i++){
        uint64_t data_blk_pos = start_of_data_blks + data_blk_array[i] * SIZE_OF_4K_BLK;
        struct data_blk data_blk_crt;
        ioseek(fs.dev_io_intf, data_blk_pos);
        ioread_full(fs.dev_io_intf, (void*)&data_blk_crt, SIZE_OF_4K_BLK);
            for(int j = start_offset; j < SIZE_OF_4K_BLK; j++){
                // check if finish
                if(i == end_blk && j == end_offset + 1){
                    // break after last byte
                    break;
                }
                *(char*)(buf + bytes_read) = data_blk_crt.data[j];
                bytes_read++;
            }
            start_offset = 0; // reset the start offset for reading next block
    }
    current->pos += bytes_read;
    return bytes_read;
}

long fs_write(struct io_intf* io, const void* buf, unsigned long n){
    // input:
    //     io: the io interface to the file to read
    //     buf: the buffer to read into
    //     n: the number of bytes to read
    // output:
    //     return the number of bytes read on success, relative errcode on failure
    // side effect:
    // reads n bytes from the file associated with io into buf. 
    // Updates metadata in the file struct as appropriate. Use fs_open to get io.

    // if (fs_initialized != FS_INITIALIZED){
    //     kprintf("fs_write: fs not initialized");
    //     return -EINVAL;
    // }

    // check if the file exists
    struct file_desc* current = find_file_desc_by_io(io);
    if (current == NULL){
        kprintf("fs_write: file not found\n");
        return -ENOENT;
    }

    // check if the file is in use
    if (current->flag == FILE_NOT_IN_USE){
        kprintf("fs_write: file not in use\n");
        return -EIO;
    }

    uint32_t inodes = current->inodes;
    uint32_t pos = current->pos;
    uint32_t size = current->size;

    // extend size
    if(pos + n > size){
        size = pos + n;
    }
    // extend the data blocks if needed
    int new_data_blks = size / SIZE_OF_4K_BLK + 1;
    if(new_data_blks > MAX_DATA_BLKS){
        kprintf("fs_write: file too large\n");
        return -EIO;
    }
    // read the data blk index from the inode
    struct inode inode;
    uint64_t inode_pos = SIZE_OF_4K_BLK * (START_IDX_OF_INODE + inodes);
    ioseek(fs.dev_io_intf, inode_pos);
    ioread_full(fs.dev_io_intf, (void*)&inode, SIZE_OF_4K_BLK);
    
    // get the index for the data blocks and offset
    // also, the physical location for data blocks is hashed in inode.data_blks,
    // not in order, access by reading the list
    uint32_t num_blks = inode.length / SIZE_OF_4K_BLK + 1; // upper bound (e.g. if length < 4096)
    uint32_t data_blk_array[new_data_blks];
    // if new data blocks are needed, try to allocate new one
    // compare with max capacity
    uint64_t capacity;
    fs.dev_io_intf->ops->ctl(fs.dev_io_intf, IOCTL_GETLEN, &capacity);
    if(new_data_blks > num_blks){
        if((fs.boot_blk.num_data_blks + 1) * SIZE_OF_4K_BLK + start_of_data_blks >= capacity){
            kprintf("fs_write: no more space\n");
            return -EIO;
        }
        uint32_t new_blk_idx = fs.boot_blk.num_data_blks;
        fs.boot_blk.num_data_blks++;
        data_blk_array[num_blks - 1] = new_blk_idx;
    }
    // copy the old data blocks
    for (int i = 0; i < num_blks; i++){
        data_blk_array[i] = inode.data_blks[i];
    }
    // prepare the structure and send back
    ioseek(fs.dev_io_intf, POS_BOOT_BLK);
    iowrite(fs.dev_io_intf, (void*)&fs.boot_blk, SIZE_OF_4K_BLK);
    struct inode updated_node= {
        .length = size
    };
    for (int i = 0; i < new_data_blks; i++){
        updated_node.data_blks[i] = data_blk_array[i];
    }
    ioseek(fs.dev_io_intf, inode_pos);
    iowrite(fs.dev_io_intf, (void*)&updated_node, SIZE_OF_4K_BLK);
    
    // write the data blocks
    current->size = size;
    if(n == 0){
        return EOF;
    }
    uint32_t start_blk = pos / SIZE_OF_4K_BLK;
    uint32_t start_offset = pos % SIZE_OF_4K_BLK;
    uint32_t end_blk = (pos + n - 1) / SIZE_OF_4K_BLK;
    uint32_t end_offset = (pos + n - 1) % SIZE_OF_4K_BLK;
    // again, sanity check
    if (start_blk >= num_blks || end_blk >= num_blks){
        kprintf("fs_read: start_blk or end_blk >= num_blks\n");
        return -EINVAL;
    }

    // read the data blocks one by one and copy the data to the buffer based on offset
    long bytes_written = 0;
    for(int i = start_blk; i <= end_blk; i++){
        // prepare the 4k blk to write
        uint64_t data_blk_pos = start_of_data_blks + data_blk_array[i] * SIZE_OF_4K_BLK;
        struct data_blk data_blk_crt;
        ioseek(fs.dev_io_intf, data_blk_pos);
        ioread_full(fs.dev_io_intf, (void*)&data_blk_crt, SIZE_OF_4K_BLK);
        for(int j = start_offset; j < SIZE_OF_4K_BLK; j++){
            if(i == end_blk && j == end_offset+1){
                // break at last byte
                break;
            }
            
            data_blk_crt.data[j] = *(char*)(buf + bytes_written);
            bytes_written++;
        }
        // write the 4k blk back
        fs.dev_io_intf->ops->ctl(fs.dev_io_intf, IOCTL_SETPOS, &data_blk_pos);
        iowrite(fs.dev_io_intf, (void*)&data_blk_crt, SIZE_OF_4K_BLK);
        start_offset = 0; // reset the start offset for reading next block
    }
    current->pos += bytes_written;
    return bytes_written;
}

int fs_ioctl(struct io_intf* io, int cmd, void* arg){
    // input:
    //     io: the io interface to the file to read
    //     cmd: the ioctl command
    //     arg: the argument to the ioctl command
    // output:
    //     return 0 on success, relative errcode on failure
    // side effect:
    //      Performs a device-specific function based on cmd.
    //      Note, ioctl functions should return values by using arg. See io.h for details.

    // if (fs_initialized != FS_INITIALIZED){
    //     kprintf("fs_ioctl: fs not initialized");
    //     return -EINVAL;
    // }

    // check if the file exists
    struct file_desc* current = find_file_desc_by_io(io);
    if (current == NULL){
        kprintf("fs_ioctl: file not found\n");
        return -EIO;
    }

    // check the command
    switch(cmd){
        case IOCTL_GETLEN:
            return fs_getlen(current, arg);
        case IOCTL_GETPOS:
            return fs_getpos(current, arg);
        case IOCTL_SETPOS:
            return fs_setpos(current, arg);
        case IOCTL_GETBLKSZ:
            return fs_getblksize(current, arg);
        default:
            kprintf("fs_ioctl: invalid cmd\n");
            return -ENOTSUP;
    }
}

int fs_getlen(struct file_desc* file, void* arg){
    // input:
    //     file: the file descriptor to the file to read
    //     arg: pointer to return value
    // output:
    //     return 0 on success, relative errcode on failure
    // side effect:
    //      Returns the length of the file associated with io in pointer.

    *(uint64_t*)arg = file->size;
    return 0;
}

int fs_setpos(struct file_desc* file, void* arg){
    // input:
    //     file: the file descriptor to the file to read
    //     arg: pointer contains the position to set
    // output:
    //     return 0 on success, relative errcode on failure
    // side effect:
    //      Sets the position of the file associated with io.

    // check if the file is in use
    if (file->flag == FILE_NOT_IN_USE){
        kprintf("fs_ioctl: file not in use\n");
        return -EIO;
    }

    uint64_t pos = *(uint64_t*)arg;
    if(pos > file->size){
        kprintf("fs_setpos: pos > size\n");
        file->pos = file->size;
        return 0;
    }

    file->pos = pos;
    return 0;
}

int fs_getpos(struct file_desc* file, void* arg){
    // input:
    //     file: the file descriptor to the file to read
    //     arg: pointer to return value
    // output:
    //     return 0 on success, relative errcode on failure
    // side effect:
    //      Gets the position of the file associated with io in pointer.

    // check if the file is in use
    if (file->flag == FILE_NOT_IN_USE){
        kprintf("fs_ioctl: file not in use\n");
        return -EIO;
    }

    *(uint64_t*)arg = file->pos;
    return 0;
}

int fs_getblksize(struct file_desc* file, void* arg){
    // input:
    //     file: the file descriptor to the file to read
    //     arg: pointer to return value
    // output:
    //     return 0 on success, relative errcode on failure
    // side effect:
    //      Gets the block size of the file associated with io in pointer.

    *(uint64_t*)arg = SIZE_OF_4K_BLK;
    return 0;
}

uint32_t find_inode_by_name(const char* name){
    // input:
    //     name: the name of the file to find
    // output:
    //     return the inode of the file if found, N if not found
    // side effect:
    // Takes the name of the file to be found and returns the inode of the file if found, NULL if not found.
    // This function should be used to find the inode of a file given its name.

    for (int i = 0; i < fs.boot_blk.num_entries; i++){
        if (strcmp(fs.boot_blk.entries[i].name, name) == 0){
            return fs.boot_blk.entries[i].inode;
        }
    }
    return fs.boot_blk.num_inodes;
}

struct file_desc* find_file_desc_by_io(struct io_intf* io){
    // input:
    //     io: the io interface to the file to find
    // output:
    //     return the file descriptor of the file if found, NULL if not found
    // side effect:
    // none

    struct file_desc* current = file_desc_head;
    while(current != NULL){
        if(current->io_intf == io){
            return current;
        }
        current = current->next;
    }
    return NULL;
}
#if 0
struct file_desc* find_last_file_desc_by_io(struct io_intf* io){
    // input:
    //     io: the io interface to the file to find
    // output:
    //     return the last file desc pointinb to this file desc with given io
    // side effect:
    // none

    struct file_desc* current = file_desc_head;
    if (current == NULL){
        return NULL;
    }
    while(current->next != NULL){
        if(current->next->io_intf == io){
            return current;
        }
        current = current->next;
    }
    return NULL;
}
#endif