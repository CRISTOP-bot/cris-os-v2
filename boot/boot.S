/* Put multiboot header inside .text so it appears early in the file
    (GRUB scans the first 8 KiB of the kernel for the header). */
.text
.align 4
.long 0x1BADB002
.long 0x00000003
.long -(0x1BADB002 + 0x00000003)

.global start
.type start, @function
start:
    cli
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov $stack_top, %esp
    call kmain
1:  jmp 1b

.section .bss
.lcomm stack, 8192
.set stack_top, stack + 8192
