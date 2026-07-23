#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_PROCESSES 32
#define PROCESS_NAME_SIZE 64
#define USER_STACK_SIZE (PAGE_SIZE * 4)
#define KERNEL_STACK_SIZE (PAGE_SIZE * 4)
#define USER_STACK_TOP 0x7FFFFFF000ULL
#define USER_CODE_TOP 0x400000ULL

enum process_state {
	PROCESS_UNUSED = 0,
	PROCESS_RUNNING,
	PROCESS_READY,
	PROCESS_BLOCKED,
	PROCESS_ZOMBIE,
};

struct process_context {
	uint64_t rax, rbx, rcx, rdx;
	uint64_t rsi, rdi, rbp, rsp;
	uint64_t r8, r9, r10, r11;
	uint64_t r12, r13, r14, r15;
	uint64_t rip;
	uint64_t rflags;
	uint64_t cr3;
	uint64_t cs, ss;
};

struct process {
	int pid;
	int parent_pid;
	enum process_state state;
	char name[PROCESS_NAME_SIZE];
	struct process_context ctx;
	uint64_t kernel_stack;
	uint64_t user_stack;
	uint64_t user_code;
	uint64_t brk;
	uint64_t brk_limit;
	int exit_code;
	int wait_exit;
};

void process_init(void);
int process_create(const char *name, uint64_t entry, bool user);
void process_exit(int code);
int process_fork(void);
int process_exec(const char *path);
int process_wait(int *status);
struct process *process_current(void);
void process_list(void);
void process_schedule(void);
void process_switch_to(struct process *next);
int process_get_pid(void);

int keyboard_readline_user(char *buf, int maxlen);

#endif
