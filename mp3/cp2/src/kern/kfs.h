#ifndef _KFS_H_
#define _KFS_H_

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include "error.h"
#include "io.h"
#include "fs.h"

#define RESERVED_SP_BOOTBLK 52
#define RESERVED_SP_ENTRY 28
#define MAX_FINENAME 32
#define MAX_FILES 63
#define MAX_DATA_BLKS 1023
#define SIZE_OF_4K_BLK 4096
#define MASK_TASKS_PER_FILE 32
#define FS_BOOT_BLK_POS 0
#define FS_INITIALIZED 1
#define FILE_START 0
#define FILE_IN_USE 1
#define FILE_NOT_IN_USE 0
#define START_IDX_OF_INODE 1
#define POS_BOOT_BLK 0
#define MAX_FILE_DESC 16

struct entry{
    char name[MAX_FINENAME];
    uint32_t inode;
    uint8_t preserved[RESERVED_SP_ENTRY];
}__attribute((packed));

struct boot_blk{
    uint32_t num_entries;
    uint32_t num_inodes;
    uint32_t num_data_blks;
    uint8_t preserved[RESERVED_SP_BOOTBLK];
    struct entry entries[MAX_FILES];

}__attribute((packed));

struct fs{
    struct io_intf* dev_io_intf;
    struct boot_blk boot_blk;
};


struct inode{
    uint32_t length;
    uint32_t data_blks[MAX_DATA_BLKS];
}__attribute((packed));

struct data_blk{
    char data[SIZE_OF_4K_BLK];
}__attribute((packed));

struct file_desc{
    struct io_intf* io_intf;
    uint64_t pos;
    uint64_t size;
    uint64_t inodes;
    uint64_t flag;
};


extern void fs_init(void);
extern int fs_mount(struct io_intf* io);
extern int fs_open(const char* name, struct io_intf** io);

void fs_close(struct io_intf* io);
long fs_read(struct io_intf* io, void* buf, unsigned long n);
long fs_write(struct io_intf* io, const void* buf, unsigned long n);
int fs_ioctl(struct io_intf* io, int cmd, void* arg);
int fs_getlen(int fd, void* arg);
int fs_setpos(int fd, void* arg);
int fs_getpos(int fd, void* arg);
int fs_getblksize(int fd, void* arg);

uint32_t find_inode_by_name(const char* name);
//struct file_desc* find_last_file_desc_by_io(struct io_intf* io);
int find_file_desc_by_io(struct io_intf* io);
int find_idle_file_desc(void);
#endif
