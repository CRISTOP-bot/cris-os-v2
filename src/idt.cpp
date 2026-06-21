#include "idt.h"
#include "console.h"
#include "asm.h"

struct IDTEntry {
    unsigned short base_low;
    unsigned short sel;
    unsigned char zero;
    unsigned char flags;
    unsigned short base_high;
} __attribute__((packed));

struct IDTPointer {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

static IDTEntry idt[256];
static bool idt_installed = false;

extern "C" void isr0();  extern "C" void isr1();  extern "C" void isr2();
extern "C" void isr3();  extern "C" void isr4();  extern "C" void isr5();
extern "C" void isr6();  extern "C" void isr7();  extern "C" void isr8();
extern "C" void isr9();  extern "C" void isr10(); extern "C" void isr11();
extern "C" void isr12(); extern "C" void isr13(); extern "C" void isr14();
extern "C" void isr15(); extern "C" void isr16(); extern "C" void isr17();
extern "C" void isr18(); extern "C" void isr19(); extern "C" void isr20();
extern "C" void isr21(); extern "C" void isr22(); extern "C" void isr23();
extern "C" void isr24(); extern "C" void isr25(); extern "C" void isr26();
extern "C" void isr27(); extern "C" void isr28(); extern "C" void isr29();
extern "C" void isr30(); extern "C" void isr31();

static void idt_set_entry(int num, void* handler, unsigned short sel, unsigned char flags) {
    unsigned int base = (unsigned int)handler;
    idt[num].base_low  = base & 0xFFFF;
    idt[num].sel       = sel;
    idt[num].zero      = 0;
    idt[num].flags     = flags;
    idt[num].base_high = (base >> 16) & 0xFFFF;
}

static void* handlers[] = {
    isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
    isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
};

static const char* exception_names[] = {
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

struct ISRRegs {
    unsigned int gs, fs, es, ds;
    unsigned int edi, esi, ebp, old_esp, ebx, edx, ecx, eax;
    unsigned int num, err;
};

extern "C" void exception_handler(ISRRegs* r) {
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

void idt_init() {
    if (idt_installed) return;

    unsigned short code_sel = 0x08;
    unsigned char flags = 0x8E;

    for (int i = 0; i < 32; ++i) {
        idt_set_entry(i, handlers[i], code_sel, flags);
    }

    IDTPointer idtp;
    idtp.limit = (unsigned short)(sizeof(IDTEntry) * 256 - 1);
    idtp.base  = (unsigned int)&idt;

    asm volatile("lidt (%0)" : : "p" (&idtp));

    idt_installed = true;
    console_print("[ OK ] IDT installed (32 exception handlers)\n");
}
