.text

.global sub_asm
.type sub_asm, @function
sub_asm:
    push %ebp
    mov %esp, %ebp
    movl 8(%ebp), %eax
    subl 12(%ebp), %eax
    pop %ebp
    ret

.global mul_asm
.type mul_asm, @function
mul_asm:
    push %ebp
    mov %esp, %ebp
    movl 8(%ebp), %eax
    imull 12(%ebp), %eax
    pop %ebp
    ret

.global div_asm
.type div_asm, @function
div_asm:
    push %ebp
    mov %esp, %ebp
    movl 8(%ebp), %eax
    movl 12(%ebp), %ecx
    testl %ecx, %ecx
    jnz .Ldiv_ok
    xorl %eax, %eax
    jmp .Ldiv_end
.Ldiv_ok:
    cdq
    idivl %ecx
.Ldiv_end:
    pop %ebp
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
    ret

.global inb
.type inb, @function
inb:
    push %ebp
    mov %esp, %ebp
    movw 8(%ebp), %dx
    inb %dx, %al
    movzbl %al, %eax
    pop %ebp
    ret

.global outb
.type outb, @function
outb:
    push %ebp
    mov %esp, %ebp
    movw 8(%ebp), %dx
    movb 12(%ebp), %al
    outb %al, %dx
    pop %ebp
    ret

.global console_putxy
.type console_putxy, @function
console_putxy:
    push %ebp
    mov %esp, %ebp
    movl 8(%ebp), %eax
    movl 12(%ebp), %ebx
    movzbl 16(%ebp), %ecx
    movzbl 20(%ebp), %edx
    movw %bx, %bx
    movzwl %bx, %ebx
    imull $80, %ebx
    addl %eax, %ebx
    shll $1, %ebx
    movb %cl, %al
    movb %dl, %ah
    movl $0xB8000, %edi
    addl %ebx, %edi
    movw %ax, (%edi)
    pop %ebp
    ret

.global memsetw_asm
.type memsetw_asm, @function
memsetw_asm:
    push %ebp
    mov %esp, %ebp
    movl 8(%ebp), %edi
    movw 12(%ebp), %ax
    movl 16(%ebp), %ecx
    cld
    rep stosw
    pop %ebp
    ret

.macro isr_no_err num
.global isr\num
.type isr\num, @function
isr\num:
    push $0
    push $\num
    jmp isr_common_stub
.endm

.macro isr_err num
.global isr\num
.type isr\num, @function
isr\num:
    push $\num
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
    pusha
    push %ds
    push %es
    push %fs
    push %gs
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %esp, %eax
    push %eax
    call exception_handler
    add $4, %esp
    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    add $8, %esp
    iret

.global cpuid
.type cpuid, @function
cpuid:
    push %ebp
    mov %esp, %ebp
    push %ebx
    push %edi
    mov 8(%ebp), %eax
    mov 12(%ebp), %edi
    cpuid
    mov %eax, 0(%edi)
    mov %ebx, 4(%edi)
    mov %ecx, 8(%edi)
    mov %edx, 12(%edi)
    pop %edi
    pop %ebx
    pop %ebp
    ret

.global gdt_load
.type gdt_load, @function
gdt_load:
    movl 4(%esp), %eax
    lgdt (%eax)
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    ljmp $0x08, $gdt_flush
gdt_flush:
    ret

.macro irq_def num
.global irq\num
.type irq\num, @function
irq\num:
    push $0
    push $\num
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
    pusha
    push %ds
    push %es
    push %fs
    push %gs
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %esp, %eax
    push %eax
    call irq_handler
    add $4, %esp
    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    add $8, %esp
    iret
