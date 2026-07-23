#include "idt.h"
#include "console.h"
#include "asm.h"
#include "kstring.h"
#include "pic.h"
#include "timer.h"
#include "mouse.h"
#include "keyboard.h"
#include "syscall.h"
#include "process.h"
#include <stdbool.h>
#include <stdint.h>

struct idt_entry {
	uint16_t base_low;
	uint16_t sel;
	uint8_t  ist;
	uint8_t  flags;
	uint16_t base_mid;
	uint32_t base_high;
	uint32_t reserved;
} __attribute__((packed));

struct idt_pointer {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));

typedef struct idt_entry idt_entry_t;
typedef struct idt_pointer idt_pointer_t;

static idt_entry_t idt[256];
static bool idt_installed;

static void serial_out_debug(char c) {
	while (!(inb(0x3F8 + 5) & 0x20));
	outb(0x3F8, c);
}

static void serial_print_debug(const char *s) {
	while (*s) serial_out_debug(*s++);
}

void isr0(void);  void isr1(void);  void isr2(void);  void isr3(void);
void isr4(void);  void isr5(void);  void isr6(void);  void isr7(void);
void isr8(void);  void isr9(void);  void isr10(void); void isr11(void);
void isr12(void); void isr13(void); void isr14(void); void isr15(void);
void isr16(void); void isr17(void); void isr18(void); void isr19(void);
void isr20(void); void isr21(void); void isr22(void); void isr23(void);
void isr24(void); void isr25(void); void isr26(void); void isr27(void);
void isr28(void); void isr29(void); void isr30(void); void isr31(void);

void irq0(void);  void irq1(void);  void irq2(void);  void irq3(void);
void irq4(void);  void irq5(void);  void irq6(void);  void irq7(void);
void irq8(void);  void irq9(void);  void irq10(void); void irq11(void);
void irq12(void); void irq13(void); void irq14(void); void irq15(void);

void syscall_entry(void);

static void idt_set_entry(int num, void *handler,
			  unsigned short sel, unsigned char flags)
{
	uint64_t base = (uint64_t)handler;
	idt[num].base_low  = base & 0xFFFF;
	idt[num].sel       = sel;
	idt[num].ist       = 0;
	idt[num].flags     = flags;
	idt[num].base_mid  = (base >> 16) & 0xFFFF;
	idt[num].base_high = (uint32_t)(base >> 32);
	idt[num].reserved  = 0;
}

static void *ex_handlers[] = {
	isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
	isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
	isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
	isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
};

static void *irq_handlers[] = {
	irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
	irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
};

static const char *exception_names[] = {
	"Division by zero",
	"Debug",
	"Non-maskable interrupt",
	"Breakpoint",
	"Overflow",
	"Bound range exceeded",
	"Invalid opcode",
	"Device not available",
	"Double fault",
	"Coprocessor segment overrun",
	"Invalid TSS",
	"Segment not present",
	"Stack-segment fault",
	"General protection fault",
	"Page fault",
	"Reserved",
	"x87 FPU error",
	"Alignment check",
	"Machine check",
	"SIMD FPU exception",
	"Virtualization exception",
	"Control protection exception",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Security exception",
	"Reserved"
};

struct isr_regs {
	uint64_t ds;
	uint64_t r11, r10, r9, r8;
	uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
	uint64_t num, err;
};

static unsigned long read_cr2(void)
{
	unsigned long v;
	__asm__ volatile("mov %%cr2, %0" : "=r"(v));
	return v;
}

void exception_handler(struct isr_regs *r)
{
	console_clear_color(0x4F);
	console_print_color("+========================================+\n", VGA_ATTR(VGA_WHITE, VGA_RED));
	console_print_color("|       CRITICAL CPU EXCEPTION            |\n", VGA_ATTR(VGA_WHITE, VGA_RED));
	console_print_color("+========================================+\n\n", VGA_ATTR(VGA_WHITE, VGA_RED));
	if (r->num < 32) {
		console_print("Exception: ");
		console_print(exception_names[r->num]);
		console_print("\n");
	} else {
		console_print("Unknown interrupt\n");
	}
	if (r->num == 14) {
		unsigned long cr2 = read_cr2();
		console_print("Fault address: 0x");
		char buf[17];
		kxtoa(cr2, buf, sizeof(buf));
		console_print(buf);
		console_print("\n");
	}
	unsigned long regs[16];
	regs[0] = r->rax;
	regs[1] = r->rbx;
	regs[2] = r->rcx;
	regs[3] = r->rdx;
	regs[4] = r->rsi;
	regs[5] = r->rdi;
	regs[6] = r->rbp;
	regs[7] = 0;
	const char *exc_name = (r->num < 32) ? exception_names[r->num] : "Unknown exception";
	kernel_panic_ex(exc_name, (unsigned int)r->num,
			(unsigned int)r->err, (unsigned int *)regs);
	halt_cpu();
}

void irq_handler(struct isr_regs *r)
{
	unsigned char irq = (unsigned char)(r->num - 32);

	switch (irq) {
	case 0:
		timer_handler();
		break;
	case 1:
		keyboard_irq_handler();
		break;
	case 12:
		mouse_handler();
		break;
	default:
		break;
	}

	pic_eoi(irq);
}

void idt_init(void)
{
	serial_print_debug("idt_init entry\n");
	if (idt_installed) {
		serial_print_debug("already installed\n");
		return;
	}
	serial_print_debug("installing...\n");

	unsigned short code_sel = 0x08;
	unsigned char flags = 0x8E;
	serial_print_debug("setting exceptions\n");

	for (int i = 0; i < 32; ++i)
		idt_set_entry(i, ex_handlers[i], code_sel, flags);

	serial_print_debug(" exceptions done\n");

	serial_print_debug(" setting irqs\n");
	for (int i = 0; i < 16; ++i)
		idt_set_entry(32 + i, irq_handlers[i], code_sel, flags);

	serial_print_debug(" irqs done\n");

	/* Syscall interrupt INT 0x80 */
	idt_set_entry(0x80, syscall_entry, code_sel, 0xEE);
	serial_print_debug(" syscall handler set\n");

	idt_pointer_t idtp;
	idtp.limit = (unsigned short)(sizeof(idt_entry_t) * 256 - 1);
	idtp.base  = (uint64_t)&idt;
	serial_print_debug("loading idt\n");

	__asm__ volatile("lidt %0" : : "m"(idtp));
	serial_print_debug("lidt done\n");

	idt_installed = true;
	serial_print_debug("idt installed, printing\n");
	console_print("[ OK ] IDT installed (32 exceptions + 16 IRQs + syscall)\n");
	serial_print_debug("idt_init done\n");
}
