# Based on xv6-riscv Makefile; see LICENSE.

TOOLPREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(TOOLPREFIX)gcc
AS=$(TOOLPREFIX)as
LD=$(TOOLPREFIX)ld
OBJCOPY=$(TOOLPREFIX)objcopy
OBJDUMP=$(TOOLPREFIX)objdump

KERN_OBJS = \
	kern/start.o \
	kern/halt.o \
	kern/string.o \
	kern/trapasm.o \
	kern/intr.o \
	kern/plic.o \
	kern/serial.o \
	kern/console.o

# Add -DDEBUG to CFLAGS below to enable debug() messages
# Add -DTRACE to CFLAGS below to enable trace() messages

CFLAGS = -Wall -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64g -mabi=lp64d
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -Ikern -DTREK_RAW_MODE

all: cp1.elf

cp1.asm: cp1.elf
	$(OBJDUMP) -d $< > $@

cp1.elf: $(KERN_OBJS) cp1/trek.o cp1/main.o
	$(LD) -T kernel.ld -o $@ $^

run-cp1: cp1.elf
	$(QEMU) -machine virt -bios none -kernel cp1.elf -m 8M -nographic -serial mon:stdio -serial pty

run-cp1-gold: cp1-gold.elf
	$(QEMU) -machine virt -bios none -kernel cp1-gold.elf -m 8M -nographic -serial mon:stdio -serial pty

debug-cp1: cp1.elf
	$(QEMU) -S -s -machine virt -bios none -kernel cp1.elf -m 8M -nographic -serial mon:stdio -serial pty

clean:
	find . -type f -name '*.o' ! -name 'trek.o' -delete
	rm -rf cp1.elf *.asm
