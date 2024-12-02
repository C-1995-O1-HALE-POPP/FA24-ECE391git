
#include "syscall.h"
#include "string.h"

void main(void) {
    const char * const greeting = "Hello, world!\r\n";
    size_t slen;
    int result;

    // Open ser1 device as fd=0

    result = _devopen(0, "ser", 1);

    if (result < 0)
        return;
    char buffer[20];
    memset(buffer, 0, 20);
    // I don't want to use gdb, so adding this for see the ser
    while(!buffer[0]){
        _read(0, buffer, 1);
    }
    slen = strlen(greeting);
    _write(0, greeting, slen);
    _close(0);
}
