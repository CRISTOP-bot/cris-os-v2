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

.global halt_cpu
.type halt_cpu, @function
halt_cpu:
    cli
    hlt
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
