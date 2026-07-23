.text

.global sub_asm
.type sub_asm, @function
sub_asm:
    mov %edi, %eax
    sub %esi, %eax
    ret

.global mul_asm
.type mul_asm, @function
mul_asm:
    mov %edi, %eax
    imull %esi, %eax
    ret

.global div_asm
.type div_asm, @function
div_asm:
    mov %edi, %eax
    testl %esi, %esi
    jnz .Ldiv_ok
    xorl %eax, %eax
    ret
.Ldiv_ok:
    cltd
    idivl %esi
    ret

.global sti
.type sti, @function
sti:
    sti
    ret

.global halt_cpu
.type halt_cpu, @function
halt_cpu:
    cli
1:  hlt
    jmp 1b

.global reboot_cpu
.type reboot_cpu, @function
reboot_cpu:
    cli
    movw $0x64, %dx
    movb $0xFE, %al
    outb %al, %dx
1:  jmp 1b

.global inb
.type inb, @function
inb:
    movw %di, %dx
    inb %dx, %al
    movzbl %al, %eax
    ret

.global outb
.type outb, @function
outb:
    movw %di, %dx
    movb %sil, %al
    outb %al, %dx
    ret

.global inl
.type inl, @function
inl:
    movw %di, %dx
    inl %dx, %eax
    ret

.global outl
.type outl, @function
outl:
    movw %di, %dx
    movl %esi, %eax
    outl %eax, %dx
    ret

.global inw
.type inw, @function
inw:
    movw %di, %dx
    inw %dx, %ax
    movzwl %ax, %eax
    ret

.global outw
.type outw, @function
outw:
    movw %di, %dx
    movw %si, %ax
    outw %ax, %dx
    ret

.global console_putxy
.type console_putxy, @function
console_putxy:
    cmpl $80, %edi
    jnb .Lputxy_ret
    cmpl $25, %esi
    jnb .Lputxy_ret
    movq $0xB8000, %r8
    imull $80, %esi, %eax
    addl %edi, %eax
    shlq $1, %rax
    addq %rax, %r8
    movb %dl, %al
    movb %cl, %ah
    movw %ax, (%r8)
.Lputxy_ret:
    ret

.global memsetw_asm
.type memsetw_asm, @function
memsetw_asm:
    movq %rsi, %rax
    movl %edx, %ecx
    cld
    rep stosw
    ret

.macro isr_no_err num
.global isr\num
.type isr\num, @function
isr\num:
    pushq $0
    pushq $\num
    jmp isr_common_stub
.endm

.macro isr_err num
.global isr\num
.type isr\num, @function
isr\num:
    pushq $\num
    jmp isr_common_stub
.endm

isr_no_err 0
isr_no_err 1
isr_no_err 2
isr_no_err 3
isr_no_err 4
isr_no_err 5
isr_no_err 6
isr_no_err 7
isr_err    8
isr_no_err 9
isr_err    10
isr_err    11
isr_err    12
isr_err    13
isr_err    14
isr_no_err 15
isr_no_err 16
isr_err    17
isr_no_err 18
isr_no_err 19
isr_no_err 20
isr_err    21
isr_no_err 22
isr_no_err 23
isr_no_err 24
isr_no_err 25
isr_no_err 26
isr_no_err 27
isr_no_err 28
isr_no_err 29
isr_err    30
isr_no_err 31

isr_common_stub:
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    movq %ds, %rax
    pushq %rax
    movq $0x10, %rax
    movq %rax, %ds
    movq %rax, %es
    movq %rsp, %rdi
    call exception_handler
    popq %rax
    movq %rax, %ds
    movq %rax, %es
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax
    addq $16, %rsp
    iretq

.global cpuid
.type cpuid, @function
cpuid:
    pushq %rbx
    movl %edi, %eax
    movq %rsi, %rdi
    cpuid
    movl %eax, 0(%rdi)
    movl %ebx, 4(%rdi)
    movl %ecx, 8(%rdi)
    movl %edx, 12(%rdi)
    popq %rbx
    ret

.global gdt_load
.type gdt_load, @function
gdt_load:
    lgdt (%rdi)
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    pushq $0x08
    lea gdt_flush(%rip), %rax
    pushq %rax
    lretq
gdt_flush:
    ret

.macro irq_def num
.global irq\num
.type irq\num, @function
irq\num:
    pushq $0
    pushq $\num
    jmp irq_common_stub
.endm

irq_def 0
irq_def 1
irq_def 2
irq_def 3
irq_def 4
irq_def 5
irq_def 6
irq_def 7
irq_def 8
irq_def 9
irq_def 10
irq_def 11
irq_def 12
irq_def 13
irq_def 14
irq_def 15

irq_common_stub:
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rbx
    pushq %rbp
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    movq %ds, %rax
    pushq %rax
    movq $0x10, %rax
    movq %rax, %ds
    movq %rax, %es
    movq %rsp, %rdi
    call irq_handler
    popq %rax
    movq %rax, %ds
    movq %rax, %es
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rbp
    popq %rbx
    popq %rdx
    popq %rcx
    popq %rax
    addq $16, %rsp
    iretq
