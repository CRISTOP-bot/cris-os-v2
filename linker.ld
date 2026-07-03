OUTPUT_FORMAT("elf32-i386")
ENTRY(start)

PHDRS
{
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}

SECTIONS
{
  . = 0x00100000;
  .multiboot : { *(.multiboot) } :text
  .text : { *(.text*) } :text
  .rodata : { *(.rodata*) } :text
  .data : { *(.data*) } :data
  .bss : { *(.bss*) *(COMMON) } :data
}
