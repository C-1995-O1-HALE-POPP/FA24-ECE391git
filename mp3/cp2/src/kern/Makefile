TOOLPREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(TOOLPREFIX)gcc
AS=$(TOOLPREFIX)as
LD=$(TOOLPREFIX)ld
OBJCOPY=$(TOOLPREFIX)objcopy
OBJDUMP=$(TOOLPREFIX)objdump

CORE_OBJS_CP2 = \
	start.o \
	halt.o \
	string.o \
	trapasm.o \
	intr.o \
	plic.o \
	timer.o \
	thread.o \
	thrasm.o \
	ezheap.o \
	io.o \
	device.o \
	uart.o \
	virtio.o \
	vioblk.o \
	console.o \
	memory.o \
	elf.o \
	excp.o \
	process.o \
	syscall.o \
	kfs.o 
	# Add more object files here

# CORE_OBJS_CP2_VM = \
# 	start.o halt.o string.o trapasm.o intr.o plic.o timer.o thread.o thrasm.o ezheap.o \
# 	io.o device.o uart.o virtio.o vioblk.o console.o memory.o elf.o excp.o process.o \
# 	syscall.o kfs.o


CORE_OBJS_CP1 = \
	start.o halt.o string.o trapasm.o intr.o io.o device.o virtio.o vioblk.o console.o \
	thread.o elf.o plic.o timer.o uart.o thrasm.o ezheap.o kfs.o 

CORE_OBJS_ELF = \
	start.o halt.o string.o trapasm.o intr.o io.o device.o console.o \
	thread.o elf.o plic.o timer.o uart.o thrasm.o ezheap.o 

CORE_OBJS_FS = \
	start.o halt.o string.o trapasm.o intr.o io.o device.o console.o \
	thread.o elf.o plic.o timer.o uart.o thrasm.o ezheap.o kfs.o 

CORE_OBJS_BLK = \
	start.o halt.o string.o trapasm.o intr.o io.o device.o virtio.o vioblk.o console.o \
	thread.o plic.o timer.o uart.o thrasm.o ezheap.o 

CORE_OBJS_LIT = \
	start.o halt.o string.o trapasm.o intr.o io.o device.o console.o \
	thread.o plic.o timer.o uart.o thrasm.o ezheap.o 

CFLAGS = -Wall -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64g -mabi=lp64d
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -I. # -DDEBUG -DTRACE

QEMUOPTS = -global virtio-mmio.force-legacy=false
QEMUOPTS += -machine virt -bios none -kernel $< -m 8M -nographic
QEMUOPTS += -serial mon:stdio
QEMUOPTS += -drive file=kfs.raw,id=blk0,if=none,format=raw
QEMUOPTS += -device virtio-blk-device,drive=blk0
QEMUOPTS += -serial pty
QEMUOPTS += -monitor pty

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)

cp1:  run-cp1-shell
########################### cp2 ############################
all: kernel.elf

kernel.elf: $(CORE_OBJS_CP2) main.o companion.o
	$(LD) -T kernel.ld -o $@ $^

run-kernel: kernel.elf
	$(QEMU) $(QEMUOPTS)

debug-kernel: kernel.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

illegal_inst.elf: $(CORE_OBJS_CP2) test_src_cp2/illegal_inst.o companion.o
	$(LD) -T kernel.ld -o $@ $^
test-illegal-inst: illegal_inst.elf
	$(QEMU) $(QEMUOPTS)
debug-illegal-inst: illegal_inst.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)
illegal_read.elf: $(CORE_OBJS_CP2) test_src_cp2/illegal_read.o companion.o
	$(LD) -T kernel.ld -o $@ $^
test-illegal-read: illegal_read.elf
	$(QEMU) $(QEMUOPTS)
debug-illegal-read: illegal_read.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)
illegal_write.elf: $(CORE_OBJS_CP2) test_src_cp2/illegal_write.o companion.o
	$(LD) -T kernel.ld -o $@ $^
test-illegal-write: illegal_write.elf
	$(QEMU) $(QEMUOPTS)
debug-illegal-write: illegal_write.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)
########################### cp1 shell ############################
cp1-shell.elf: $(CORE_OBJS_CP1) main_shell.o companion.o
	$(LD) -T kernel.ld -o $@ $^

run-cp1-shell: cp1-shell.elf
	$(QEMU) $(QEMUOPTS)

debug-cp1-shell: cp1-shell.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

########################### tests for cp1 ############################
elf-test.elf: $(CORE_OBJS_ELF) test_src_cp1/main_test_elf.o companion.o
	$(LD) -T kernel.ld -o $@ $^

run-elf-test: elf-test.elf
	$(QEMU) $(QEMUOPTS)

run-elf-test-debug: elf-test.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

fs-test.elf: $(CORE_OBJS_FS) test_src_cp1/main_test_fs.o companion.o
	$(LD) -T kernel.ld -o $@ $^

run-fs-test: fs-test.elf 
	$(QEMU) $(QEMUOPTS)

run-fs-test-debug: fs-test.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

io-fs-test.elf: $(CORE_OBJS_FS) test_src_cp1/io_test_fs.o companion.o
	$(LD) -T kernel.ld -o $@ $^

run-io-fs-test: io-fs-test.elf
	$(QEMU) $(QEMUOPTS)

run-io-fs-test-debug: io-fs-test.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

io-blk-test.elf: $(CORE_OBJS_BLK) test_src_cp1/io_test_blk.o companion.o
	$(LD) -T kernel.ld -o $@ $^

run-io-blk-test: io-blk-test.elf
	$(QEMU) $(QEMUOPTS)

run-io-blk-test-debug: io-blk-test.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

io-lit-test.elf: $(CORE_OBJS_LIT) test_src_cp1/io_test_lit.o companion.o
	$(LD) -T kernel.ld -o $@ $^

run-io-lit-test: io-lit-test.elf
	$(QEMU) $(QEMUOPTS)

run-io-lit-test-debug: io-lit-test.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

# You might want to add more here

clean:
	if [ -f companion.o ]; then cp companion.o companion.o.save; fi
	rm -rf *.o *.elf *.asm test_src_cp1/*.o test_src_cp2/*.o
	if [ -f companion.o.save ]; then mv companion.o.save companion.o; fi

companion.o:
	if [ -f ../user/trek ]; then sh ./mkcomp.sh ../user/trek; fi
	if ! [ -f ../user/trek ]; then sh ./mkcomp.sh; fi

