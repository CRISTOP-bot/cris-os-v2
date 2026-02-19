.text
.global add_asm
.type add_asm, @function
add_asm:
    push %ebp
    mov %esp, %ebp
    mov 8(%ebp), %eax
    add 12(%ebp), %eax
    pop %ebp
    ret
