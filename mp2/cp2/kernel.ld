/* Based on linker script from the xv6-riscv project. */

OUTPUT_ARCH("riscv")

PHDRS {
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}

SECTIONS {
  /* ensure that _start is at 0x80000000 where qemu's -kernel jumps. */

  . = 0x80000000;

  PROVIDE(_kimg_start = .);

  .text (READONLY) : {
    PROVIDE(_kimg_text_start = .);
    * (.text.start)
    *(.text .text.*)
    . = ALIGN(16);
    PROVIDE(_kimg_text_end = .);
    . = ALIGN(4096);
  } :text

  .rodata : {
    *(.srodata .srodata.*)
    . = ALIGN(16);
    *(.rodata .rodata.*)
    . = ALIGN(16);
    . = ALIGN(4096);
  } :data

  .data : {
    *(.sdata .sdata.*)
    . = ALIGN(16);
    *(.data .data.*)
    . = ALIGN(16);
  } :data

  .bss : {
    PROVIDE(_kimg_bss_start = .);
    *(.sbss .sbss.*)
    . = ALIGN(16);
    *(.bss .bss.*)
    . = ALIGN(16);
    PROVIDE(_kimg_bss_end = .);
    . = ALIGN(4096);
  } :data
  
  PROVIDE(_kimg_end = .);
}