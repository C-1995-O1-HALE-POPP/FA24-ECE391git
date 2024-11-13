// serial.h - NS16550a serial port
//

#ifndef _SERIAL_H_
#define _SERIAL_H_

// EXPORTED VARIABLE DECLARATIONS
//

extern char com0_initialized;
extern char com1_initialized;

// EXPORTED FUNCTION DECLARATIONS
//

extern void com0_init(void);
extern void com0_putc(char c);
extern char com0_getc(void);

extern void com1_init(void);
extern void com1_putc(char c);
extern char com1_getc(void);

#endif // _SERIAL_H_