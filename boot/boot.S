.section .multiboot
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
    and $-16, %esp
    push %ebx
    call kmain
    add $4, %esp
1:  jmp 1b

.section .bss
.align 16
.lcomm stack, 8192
.set stack_top, stack + 8192
