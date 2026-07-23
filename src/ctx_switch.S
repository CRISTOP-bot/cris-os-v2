.text

.global enter_user_mode
.type enter_user_mode, @function
enter_user_mode:
    /* rdi = rip, rsi = rsp, rdx = rflags */
    mov %rsi, %rsp
    pushq $0x23          /* SS (user data) */
    pushq %rsi           /* RSP */
    pushq %rdx           /* RFLAGS */
    pushq $0x1B          /* CS (user code) */
    pushq %rdi           /* RIP */
    iretq

.global context_switch_to
.type context_switch_to, @function
context_switch_to:
    /* rdi = pointer to next process struct */
    mov %rdi, %rax
    /* Save current context is done by C code, this just does the switch */

    /* Load new RSP from process->ctx.rsp (offset 0x58 in struct) */
    mov 0x58(%rdi), %rsp

    /* Load RFLAGS */
    mov 0x80(%rdi), %rax
    pushq %rax
    popfq

    /* Push return address for IRETQ */
    mov 0x78(%rdi), %rax  /* rip */
    pushq $0x23           /* SS */
    mov 0x58(%rdi), %rax
    pushq %rax            /* RSP */
    pushfq
    pushq $0x1B           /* CS */
    mov 0x78(%rdi), %rax
    pushq %rax            /* RIP */
    iretq

.global syscall_entry
.type syscall_entry, @function
syscall_entry:
    /* Save registers */
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* Set up kernel stack segments */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es

    /* Arguments: rdi, rsi, rdx, r10, r8, rax (syscall number) */
    /* Already in correct registers, just call */
    call syscall_handler

    /* Restore segments */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    iretq

.global tss_load
.type tss_load, @function
tss_load:
    mov %di, %ax
    ltr %ax
    ret
