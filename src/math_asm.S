.text
.global add_asm
.type add_asm, @function
add_asm:
    mov %edi, %eax
    add %esi, %eax
    ret
