OUTPUT_FORMAT("elf32-i386")
ENTRY(start)
SECTIONS
{
  . = 0x00100000;
  .multiboot : { *(.multiboot) }
  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
}
