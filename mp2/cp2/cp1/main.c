// main.c - Main function
//

#include "console.h"
#include "serial.h"
#include "intr.h"

extern void trek_main(void);

void main(void) {
	console_init();
	intr_init();
	com1_init();

	intr_enable();

	trek_main();
}
