#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define SYSCALL_READ    0
#define SYSCALL_WRITE   1
#define SYSCALL_OPEN    2
#define SYSCALL_CLOSE   3
#define SYSCALL_EXIT    4
#define SYSCALL_FORK    5
#define SYSCALL_EXEC    6
#define SYSCALL_WAIT    7
#define SYSCALL_GETPID  8
#define SYSCALL_SBRK    9
#define SYSCALL_GETCWD  10
#define SYSCALL_CHDIR   11
#define SYSCALL_PS      12
#define SYSCALL_TICKS   13
#define SYSCALL_MAX     14

void syscall_init(void);
void syscall_handler(uint64_t rdi, uint64_t rsi, uint64_t rdx,
		     uint64_t r10, uint64_t r8, uint64_t rax);

#endif
