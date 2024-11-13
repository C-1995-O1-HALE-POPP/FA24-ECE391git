// serial.h - NS16550a serial port
//

#ifndef _SERIAL_H_
#define _SERIAL_H_

#ifndef NCOM
#define NCOM 8
#endif

// EXPORTED VARIABLE DECLARATIONS
//

extern char com0_initialized; // compatiblity with cp1
extern char com1_initialized; // compatiblity with cp1


// EXPORTED FUNCTION DECLARATIONS
//

extern void com_init_sync(int k);
extern void com_init_async(int k);
extern int com_initialized(int k);
extern void com_putc(int k, char c);
extern char com_getc(int k);

// COM0 is special (used by console), so gets its own init, putc, getc.

extern void com0_init(void);
extern void com0_putc(char c);
extern char com0_getc(void);

// Compatibility functions

extern void com1_init(void);
extern void com1_putc(char c);
extern char com1_getc(void);

#endif // _SERIAL_H_