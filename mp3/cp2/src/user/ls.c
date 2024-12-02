// ls.c - list files in the current directory

#include "io.h"
#include "string.h"
#include "error.h"
#include "syscall.h"

#include <stddef.h>
#include <stdint.h>
#define RESERVED_SP_BOOTBLK 52
#define RESERVED_SP_ENTRY 28
#define MAX_FINENAME 32
#define MAX_FILES 63
#define SIZE_OF_4K_BLK 4096
#define FS_BOOT_BLK_POS 0

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

int main(void) {
    char buf_l[4096];
    int i;
    char buf[32];
    uint64_t pos = FS_BOOT_BLK_POS;
    memset(buf, 0, 9);
    _write(0, "Enter any key to list files in the current directory: ", 53);
    while(!buf[0]){
        _read(0, buf, 1);
    }
    memset(buf_l, 0, 4096);
    _ioctl(1, IOCTL_SETPOS, &pos);
    _read(1, (void*)buf_l, SIZE_OF_4K_BLK);
    struct boot_blk* boot_blk = (struct boot_blk* )buf_l;
    _msgout("Hello, I am <ls>\r\n");

    for (i = 0; i < boot_blk->num_entries; i++) {
        if(boot_blk->entries[i].name[0] != 0){
            strncpy(buf, boot_blk->entries[i].name, MAX_FINENAME);
        }
    }
    _write(0, "\r\n", 2);
    _exit();
    return 0;
}