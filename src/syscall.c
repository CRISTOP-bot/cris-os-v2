#include "syscall.h"
#include "console.h"
#include "process.h"
#include "vfs.h"
#include "timer.h"
#include "kstring.h"

typedef int64_t (*syscall_fn)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

static int64_t sys_read(uint64_t fd, uint64_t buf, uint64_t count, uint64_t a4, uint64_t a5)
{
	(void)a4; (void)a5;
	if (fd != 0)
		return -1;
	struct process *proc = process_current();
	if (!proc)
		return -1;
	int r = keyboard_readline_user((char *)buf, count);
	return r;
}

static int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t count, uint64_t a4, uint64_t a5)
{
	(void)a4; (void)a5;
	if (fd > 2)
		return -1;
	const char *src = (const char *)buf;
	for (uint64_t i = 0; i < count; ++i)
		console_putchar(src[i]);
	return (int64_t)count;
}

static int64_t sys_open(uint64_t path, uint64_t flags, uint64_t a3, uint64_t a4, uint64_t a5)
{
	(void)flags; (void)a3; (void)a4; (void)a5;
	if (!path)
		return -1;
	if (vfs_exists((const char *)path))
		return 0;
	return -1;
}

static int64_t sys_close(uint64_t fd, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
	(void)fd; (void)a2; (void)a3; (void)a4; (void)a5;
	return 0;
}

static int64_t sys_exit(uint64_t code, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
	(void)a2; (void)a3; (void)a4; (void)a5;
	process_exit((int)code);
	return 0;
}

static int64_t sys_fork(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
	return process_fork();
}

static int64_t sys_exec(uint64_t path, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
	(void)a2; (void)a3; (void)a4; (void)a5;
	if (!path)
		return -1;
	return process_exec((const char *)path);
}

static int64_t sys_wait(uint64_t status, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
	(void)a2; (void)a3; (void)a4; (void)a5;
	return process_wait((int *)status);
}

static int64_t sys_getpid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
	struct process *p = process_current();
	return p ? p->pid : -1;
}

static int64_t sys_sbrk(uint64_t increment, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
	(void)a2; (void)a3; (void)a4; (void)a5;
	struct process *p = process_current();
	if (!p)
		return -1;
	uint64_t old = p->brk;
	if (p->brk + increment > p->brk_limit)
		return -1;
	p->brk += increment;
	return (int64_t)old;
}

static int64_t sys_getcwd(uint64_t buf, uint64_t size, uint64_t a3, uint64_t a4, uint64_t a5)
{
	(void)a3; (void)a4; (void)a5;
	if (!buf || size == 0)
		return -1;
	const char *cwd = vfs_pwd();
	size_t len = kstrlen(cwd);
	if (len >= size)
		return -1;
	kstrcpy((char *)buf, cwd, size);
	return 0;
}

static int64_t sys_chdir(uint64_t path, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
	(void)a2; (void)a3; (void)a4; (void)a5;
	if (!path)
		return -1;
	return vfs_cd((const char *)path) ? 0 : -1;
}

static int64_t sys_ps(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
	process_list();
	return 0;
}

static int64_t sys_ticks(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
{
	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5;
	return (int64_t)timer_get_ticks();
}

static syscall_fn syscall_table[SYSCALL_MAX] = {
	sys_read,
	sys_write,
	sys_open,
	sys_close,
	sys_exit,
	sys_fork,
	sys_exec,
	sys_wait,
	sys_getpid,
	sys_sbrk,
	sys_getcwd,
	sys_chdir,
	sys_ps,
	sys_ticks,
};

void syscall_init(void)
{
	console_print("[ OK ] Syscalls initialized (INT 0x80)\n");
}

void syscall_handler(uint64_t rdi, uint64_t rsi, uint64_t rdx,
		     uint64_t r10, uint64_t r8, uint64_t rax)
{
	if (rax >= SYSCALL_MAX) {
		return;
	}
	syscall_fn fn = syscall_table[rax];
	if (!fn)
		return;
	fn(rdi, rsi, rdx, r10, r8);
}
