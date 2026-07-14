.section .multiboot, "a", @progbits
.align 4
MultibootMagic:
    .long 0x1BADB002
    .long 0x00000003
    .long -(0x1BADB002 + 0x00000003)

.code32

.section .text, "ax", @progbits
.global start
.type start, @function
start:
    cli

    lgdt temp32_gdt_ptr

    ljmp $0x08, $1f
1:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    mov $stack_top, %esp
    and $-16, %esp
    xor %ebp, %ebp

    cld

    push $0
    popfl

    mov %eax, multiboot_magic
    mov %ebx, multiboot_info_ptr

    mov $pml4_table, %edi
    xor %eax, %eax
    mov $1024, %ecx
    rep stosl

    mov $pdpt_low, %edi
    xor %eax, %eax
    mov $1024, %ecx
    rep stosl

    mov $pd_low, %edi
    xor %eax, %eax
    mov $1024, %ecx
    rep stosl

    movl $pdpt_low + 0x03, pml4_table
    movl $pd_low + 0x03, pdpt_low

    xor %ebx, %ebx
    xor %eax, %eax
    mov $512, %ecx
1:
    movl %eax, pd_low(,%ebx,8)
    orl $0x83, pd_low(,%ebx,8)
    addl $0x200000, %eax
    incl %ebx
    decl %ecx
    jnz 1b

    mov $pml4_table, %eax
    mov %eax, %cr3

    mov %cr4, %eax
    orl $(1 << 5), %eax
    mov %eax, %cr4

    mov $0xC0000080, %ecx
    rdmsr
    orl $(1 << 8), %eax
    wrmsr

    mov %cr0, %eax
    orl $(1 << 31), %eax
    mov %eax, %cr0

    lgdt gdt64_ptr

    ljmpl $0x08, $long_mode_start

.code64
long_mode_start:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    mov $stack_top, %rsp
    and $-16, %rsp
    xor %rbp, %rbp

    mov $0xB8000, %rdi
    movw $0x0F41, (%rdi)

    movq multiboot_info_ptr, %rdi
    call kmain

.Lhalt64:
    cli
    hlt
    jmp .Lhalt64

.size start, . - start

.section .data, "aw", @progbits

.align 4
temp32_gdt_null:
    .quad 0
temp32_gdt_code:
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0x9A
    .byte 0xCF
    .byte 0x00
temp32_gdt_data:
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0x92
    .byte 0xCF
    .byte 0x00
temp32_gdt_end:

temp32_gdt_ptr:
    .word temp32_gdt_end - temp32_gdt_null - 1
    .long temp32_gdt_null

.align 4
.global multiboot_magic
.type multiboot_magic, @object
multiboot_magic:
    .long 0

.global multiboot_info_ptr
.type multiboot_info_ptr, @object
multiboot_info_ptr:
    .long 0

.align 16
gdt64_null:
    .quad 0
gdt64_code:
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0x9A
    .byte 0xAF
    .byte 0x00
gdt64_data:
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0x92
    .byte 0xCF
    .byte 0x00
gdt64_end:

gdt64_ptr:
    .word gdt64_end - gdt64_null - 1
    .quad gdt64_null

.align 4096
pml4_table:
    .skip 4096, 0
pdpt_low:
    .skip 4096, 0
pd_low:
    .skip 4096, 0

.section .bss, "aw", @nobits
.align 16
stack_bottom:
    .skip 16384, 0
stack_top:

.section .note.GNU-stack, "", @progbits
