#include "syscall.h"
#include "string.h"
int main(void) {
    _msgout("a string in user spaces from 0xC00C1234\n");
    char * ptr = (char*)0xC0031234;
    for (int i = 0; i < 10; i++) {
         ptr[i] = 'A' + i;
    }
    _msgout("Write done, now read\n");
    _msgout(ptr);
    _msgout("now we allocate a close page 0xC0002000, which stores the ptr to the string\n");
    char ** ptr2 = (char **)0xC0032000;
    *ptr2 = ptr;
    _msgout("*ptr2 = 0xC0001234\n");
    _msgout("now we read the string from *ptr2\n");
    _msgout(*ptr2);
    _msgout("done\n");
    _msgout("we can also read the string using &ptr[0]\n");
    _msgout(&ptr[0]);
    _msgout("done\n");
    _msgout("now we try to access the kernel space 0x801c1234, which should crash\n");
    char * ptr3 = (char *)0x801c1234;
    ptr3[0] = 'A';
    _msgout("we should not be here\n");
    return 0;
}