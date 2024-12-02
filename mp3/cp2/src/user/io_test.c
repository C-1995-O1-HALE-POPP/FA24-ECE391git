#include "syscall.h"
#include "string.h"
#include "stdint.h"

//           arg is pointer to uint64_t
#define IOCTL_GETLEN        1
//           arg is pointer to uint64_t
#define IOCTL_SETLEN        2
//           arg is pointer to uint64_t
#define IOCTL_GETPOS        3
//           arg is pointer to uint64_t
#define IOCTL_SETPOS        4
//           arg is ignored
#define IOCTL_FLUSH         5
//           arg is pointer to uint32_t
#define IOCTL_GETBLKSZ      6
void main(void) {
    // set variables for testing
    int return_val;
    char buffer[20];
    
    const char *msg = "It's the text written by _write.\r\n";
    const char* ha = "HAHAHA";
    
    size_t length;

    // demonstrates successful use of the msgout in syscall
    _msgout("Hello, we're Group59.\r\n");
    _msgout("_msgout succeed!\r\n");

    // demonstrates successful use of the devopen in syscall
    return_val=_devopen(0,"ser",1);
    if(0>return_val){
        _msgout("_devopen failed");
        _exit();
    }
    _msgout("_devopen succeed!\r\n");
    memset(buffer, 0, 20);
    _write(0, "Enter any key to list files in the current directory: ", 53);
    while(!buffer[0]){
        _read(0, buffer, 1);
    }
    memset(buffer, 0, 20);
    _msgout("open nonexistent file\r\n");
    _fsopen(3, "mt3");
    
    // demonstrates successful use of the write in syscall
    // set the device as fd=0
    length=strlen(msg);
    return_val=_write(0,msg,length);
    if(0>return_val){
        _msgout("_write failed");
        _exit();
    }
    _msgout("_write succeed!\r\n");

    // demonstrates successful use of the fsopen in syscall

    return_val = _fsopen(2, "mt2");
    if (return_val < 0) {
        _msgout("_fsopen failed");
        _exit();
    }
    _msgout("_fsopen mt2 succeed!\r\n");
    // demonstrates successful use of the read in syscall
    memset(buffer, 0, 20);
    return_val = _read(2,buffer,sizeof(buffer));
    if (return_val<0) {
        _msgout("_read failed");
        _exit();
    }
    _msgout("read 20 bytes at pos 0 from mt2\r\n");
    _msgout(buffer);
    return_val = _read(2,buffer,sizeof(buffer));
    if (return_val<0) {
        _msgout("_read failed");
        _exit();
    }
    _msgout("read 20 bytes at pos 20 from mt2\r\n");
    _msgout(buffer);
    uint64_t arg = 0;
    return_val = _ioctl(2,IOCTL_SETPOS,&arg);
    if (return_val < 0) {
        _msgout("_ioctl failed");
        _exit();
    }
    _msgout("_ioctl succeed!\r\n");
    _msgout("mt2 head move to 0");

    return_val = _write(2,ha,strlen(ha));
    if (return_val<0) {
        _msgout("_write failed");
        _exit();
    }
    _msgout("write 6 bytes \"HAHAHA\" at pos 0 from mt2\r\n");
    return_val = _ioctl(2,IOCTL_SETPOS,&arg);
    if (return_val < 0) {
        _msgout("_ioctl failed");
        _exit();
    }
    _msgout("_ioctl succeed!\r\n");
    _msgout("mt2 head move to 0");
    return_val = _read(2,buffer,sizeof(buffer));
    if (return_val<0) {
        _msgout("_read failed");
        _exit();
    }
    _msgout("read 20 bytes at pos 0 from mt2\r\n");
    _msgout(buffer);
    // demonstrates successful use of the exit in syscall
    return_val = _fsopen(1, "trek");
    if (return_val < 0) {
        _msgout("_fsopen failed");
        _exit();
    }
    _msgout("_fsopen trek succeed!\r\n");

    _exec(1);

    _exit();
}
