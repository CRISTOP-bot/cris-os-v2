#include "idt.h"
#include "console.h"
#include "asm.h"
#include <stdbool.h>

struct idt_entry {
	unsigned short base_low;
	unsigned short sel;
	unsigned char zero;
	unsigned char flags;
	unsigned short base_high;
} __attribute__((packed));

struct idt_pointer {
	unsigned short limit;
	unsigned int base;
} __attribute__((packed));

typedef struct idt_entry idt_entry_t;
typedef struct idt_pointer idt_pointer_t;

static idt_entry_t idt[256];
static bool idt_installed;

void isr0(void);  void isr1(void);  void isr2(void);  void isr3(void);
void isr4(void);  void isr5(void);  void isr6(void);  void isr7(void);
void isr8(void);  void isr9(void);  void isr10(void); void isr11(void);
void isr12(void); void isr13(void); void isr14(void); void isr15(void);
void isr16(void); void isr17(void); void isr18(void); void isr19(void);
void isr20(void); void isr21(void); void isr22(void); void isr23(void);
void isr24(void); void isr25(void); void isr26(void); void isr27(void);
void isr28(void); void isr29(void); void isr30(void); void isr31(void);

static void idt_set_entry(int num, void *handler,
			  unsigned short sel, unsigned char flags)
{
	unsigned int base = (unsigned int)handler;
	idt[num].base_low  = base & 0xFFFF;
	idt[num].sel       = sel;
	idt[num].zero      = 0;
	idt[num].flags     = flags;
	idt[num].base_high = (base >> 16) & 0xFFFF;
}

static void *handlers[] = {
	isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
	isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
	isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
	isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
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
	unsigned int gs, fs, es, ds;
	unsigned int edi, esi, ebp, old_esp, ebx, edx, ecx, eax;
	unsigned int num, err;
};

void exception_handler(struct isr_regs *r)
{
	(void)r;
	console_clear_color(0x4F);
	console_print("=== CRITICAL EXCEPTION ===\n\n");
	if (r->num < 32) {
		console_print("Exception: ");
		console_print(exception_names[r->num]);
		console_print("\n");
	} else {
		console_print("Unknown interrupt\n");
	}
	console_print("The system has been halted.\n");
	halt_cpu();
}

void idt_init(void)
{
	if (idt_installed)
		return;

	unsigned short code_sel = 0x08;
	unsigned char flags = 0x8E;

	for (int i = 0; i < 32; ++i)
		idt_set_entry(i, handlers[i], code_sel, flags);

	idt_pointer_t idtp;
	idtp.limit = (unsigned short)(sizeof(idt_entry_t) * 256 - 1);
	idtp.base  = (unsigned int)&idt;

	asm volatile("lidt (%0)" : : "p" (&idtp));

	idt_installed = true;
	console_print("[ OK ] IDT installed (32 exception handlers)\n");
}
