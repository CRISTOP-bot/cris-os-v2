#include "process.h"
#include "memory.h"
#include "pmm.h"
#include "vmm.h"
#include "vfs.h"
#include "console.h"
#include "kstring.h"
#include "timer.h"
#include "keyboard.h"
#include "tss.h"
#include "asm.h"

static struct process processes[MAX_PROCESSES];
static int current_pid = -1;
static int next_pid = 1;
static bool scheduler_enabled = false;

extern void enter_user_mode(uint64_t rip, uint64_t rsp, uint64_t rflags);
extern void context_switch_to(struct process *next);

static struct process *find_process(int pid)
{
	for (int i = 0; i < MAX_PROCESSES; ++i) {
		if (processes[i].pid == pid && processes[i].state != PROCESS_UNUSED)
			return &processes[i];
	}
	return 0;
}

static int alloc_pid(void)
{
	return next_pid++;
}

static struct process *alloc_process(void)
{
	for (int i = 0; i < MAX_PROCESSES; ++i) {
		if (processes[i].state == PROCESS_UNUSED)
			return &processes[i];
	}
	return 0;
}

void process_init(void)
{
	for (int i = 0; i < MAX_PROCESSES; ++i) {
		processes[i].state = PROCESS_UNUSED;
		processes[i].pid = 0;
	}
	current_pid = -1;
	next_pid = 1;
	scheduler_enabled = false;
	console_print("[ OK ] Process manager initialized\n");
}

int process_create(const char *name, uint64_t entry, bool user)
{
	struct process *proc = alloc_process();
	if (!proc)
		return -1;

	kmemset(proc, 0, sizeof(*proc));
	proc->pid = alloc_pid();
	proc->parent_pid = current_pid;
	proc->state = PROCESS_READY;
	proc->exit_code = 0;
	proc->wait_exit = 0;
	kstrcpy(proc->name, name, PROCESS_NAME_SIZE);

	proc->user_code = USER_CODE_TOP;
	proc->user_stack = USER_STACK_TOP;
	proc->brk = USER_STACK_TOP + USER_STACK_SIZE;
	proc->brk_limit = proc->brk + PAGE_SIZE * 16;

	proc->kernel_stack = (uint64_t)kmalloc_aligned(KERNEL_STACK_SIZE);
	if (!proc->kernel_stack) {
		proc->state = PROCESS_UNUSED;
		return -1;
	}
	proc->kernel_stack += KERNEL_STACK_SIZE;

	/* Map user code page */
	for (uint64_t offset = 0; offset < USER_STACK_SIZE + USER_STACK_SIZE; offset += PAGE_SIZE) {
		uint64_t vaddr = proc->user_code + offset;
		void *page = pmm_alloc_page();
		if (!page) {
			proc->state = PROCESS_UNUSED;
			return -1;
		}
		unsigned int flags = VMM_PRESENT | VMM_WRITE;
		if (user)
			flags |= VMM_USER;
		vmm_map_page((unsigned long)page, vaddr, flags);
	}

	/* Copy entry point code into user code page */
	if (entry) {
		uint8_t *code_dst = (uint8_t *)proc->user_code;
		const uint8_t *code_src = (const uint8_t *)entry;
		for (uint64_t i = 0; i < 256; ++i)
			code_dst[i] = code_src[i];
	}

	/* Set up initial kernel context for ring 3 entry */
	proc->ctx.rip = proc->user_code;
	proc->ctx.rsp = proc->user_stack + USER_STACK_SIZE - 8;
	proc->ctx.rflags = 0x200; /* IF=1 */
	proc->ctx.cr3 = 0; /* use kernel page tables for now */
	proc->ctx.cs = user ? 0x1B : 0x08;
	proc->ctx.ss = user ? 0x23 : 0x10;

	return proc->pid;
}

void process_exit(int code)
{
	struct process *proc = process_current();
	if (!proc)
		return;

	proc->exit_code = code;
	proc->state = PROCESS_ZOMBIE;

	/* Wake up parent if waiting */
	if (proc->parent_pid > 0) {
		struct process *parent = find_process(proc->parent_pid);
		if (parent && parent->state == PROCESS_BLOCKED) {
			parent->state = PROCESS_READY;
			parent->wait_exit = proc->pid;
		}
	}

	scheduler_enabled = false;
	process_schedule();
}

int process_fork(void)
{
	struct process *parent = process_current();
	if (!parent)
		return -1;

	struct process *child = alloc_process();
	if (!child)
		return -1;

	kmemcpy(child, parent, sizeof(struct process));
	child->pid = alloc_pid();
	child->parent_pid = parent->pid;
	child->state = PROCESS_READY;
	child->exit_code = 0;
	child->wait_exit = 0;
	kstrcpy(child->name, parent->name, PROCESS_NAME_SIZE);

	child->kernel_stack = (uint64_t)kmalloc_aligned(KERNEL_STACK_SIZE);
	if (!child->kernel_stack) {
		child->state = PROCESS_UNUSED;
		return -1;
	}
	child->kernel_stack += KERNEL_STACK_SIZE;

	child->user_stack = (uint64_t)kmalloc_aligned(USER_STACK_SIZE);
	if (!child->user_stack) {
		child->state = PROCESS_UNUSED;
		return -1;
	}
	child->user_stack += USER_STACK_SIZE;

	/* Copy parent's kernel stack to child */
	kmemcpy((void *)(child->kernel_stack - KERNEL_STACK_SIZE),
		(const void *)(parent->kernel_stack - KERNEL_STACK_SIZE),
		KERNEL_STACK_SIZE);

	/* Copy parent's user data to child */
	kmemcpy((void *)child->user_stack,
		(const void *)(parent->user_stack),
		USER_STACK_SIZE);

	return child->pid;
}

int process_exec(const char *path)
{
	struct process *proc = process_current();
	if (!proc)
		return -1;

	const void *data = vfs_read(path);
	size_t size = vfs_get_size(path);
	if (!data || size == 0)
		return -1;

	/* Copy program data to user code area */
	uint8_t *dst = (uint8_t *)proc->user_code;
	const uint8_t *src = (const uint8_t *)data;
	uint64_t copy_size = size;
	if (copy_size > PAGE_SIZE * 4)
		copy_size = PAGE_SIZE * 4;
	for (uint64_t i = 0; i < copy_size; ++i)
		dst[i] = src[i];

	proc->ctx.rip = proc->user_code;
	proc->ctx.rsp = proc->user_stack + USER_STACK_SIZE - 8;

	return 0;
}

int process_wait(int *status)
{
	struct process *proc = process_current();
	if (!proc)
		return -1;

	/* Check for zombie children */
	for (int i = 0; i < MAX_PROCESSES; ++i) {
		if (processes[i].state == PROCESS_ZOMBIE &&
		    processes[i].parent_pid == proc->pid) {
			int child_pid = processes[i].pid;
			if (status)
				*status = processes[i].exit_code;
			processes[i].state = PROCESS_UNUSED;
			return child_pid;
		}
	}

	/* Block until a child exits */
	proc->state = PROCESS_BLOCKED;
	process_schedule();
	return proc->wait_exit;
}

struct process *process_current(void)
{
	if (current_pid < 0)
		return 0;
	return find_process(current_pid);
}

void process_list(void)
{
	const char *state_names[] = { "unused", "running", "ready", "blocked", "zombie" };
	console_print_color("  PID  NAME                 STATE\n", VGA_ATTR(VGA_YELLOW, VGA_BLACK));
	for (int i = 0; i < MAX_PROCESSES; ++i) {
		if (processes[i].state == PROCESS_UNUSED)
			continue;
		char buf[16];
		console_print("  ");
		kitoa(processes[i].pid, buf, sizeof(buf));
		int len = kstrlen(buf);
		while (len < 5) { console_print(" "); len++; }
		console_print(buf);
		console_print("  ");
		int nlen = kstrlen(processes[i].name);
		console_print(processes[i].name);
		while (nlen < 20) { console_print(" "); nlen++; }
		console_print_color(state_names[processes[i].state],
				     VGA_ATTR(VGA_LIGHT_GREY, VGA_BLACK));
		console_print("\n");
	}
}

void process_schedule(void)
{
	if (!scheduler_enabled)
		return;

	int start = current_pid;
	int best = -1;

	for (int i = 0; i < MAX_PROCESSES; ++i) {
		int idx = (start + i + 1) % MAX_PROCESSES;
		if (processes[idx].state == PROCESS_READY) {
			best = idx;
			break;
		}
	}

	if (best < 0)
		return;

	struct process *next = &processes[best];
	struct process *prev = process_current();

	if (prev && prev == next)
		return;

	if (prev && prev->state == PROCESS_RUNNING)
		prev->state = PROCESS_READY;

	next->state = PROCESS_RUNNING;
	current_pid = next->pid;

	tss_set_rsp0(next->kernel_stack);

	if (prev && prev != next) {
		context_switch_to(next);
	}
}

void process_switch_to(struct process *next)
{
	(void)next;
}

int process_get_pid(void)
{
	return current_pid;
}

int keyboard_readline_user(char *buf, int maxlen)
{
	return keyboard_readline(buf, maxlen);
}

void process_yield(void)
{
	process_schedule();
}
