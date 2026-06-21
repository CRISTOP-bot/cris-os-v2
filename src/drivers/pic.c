#include "pic.h"
#include "asm.h"

#define PIC_MASTER_CMD  0x20
#define PIC_MASTER_DATA 0x21
#define PIC_SLAVE_CMD   0xA0
#define PIC_SLAVE_DATA  0xA1

#define ICW1_ICW4      0x01
#define ICW1_INIT      0x10
#define ICW4_8086      0x01

#define IRQ_BASE       0x20

void pic_init(void)
{
	unsigned char m1 = inb(PIC_MASTER_DATA);
	unsigned char m2 = inb(PIC_SLAVE_DATA);

	outb(PIC_MASTER_CMD,  ICW1_INIT | ICW1_ICW4);
	outb(PIC_SLAVE_CMD,   ICW1_INIT | ICW1_ICW4);
	outb(PIC_MASTER_DATA, IRQ_BASE);
	outb(PIC_SLAVE_DATA,  IRQ_BASE + 8);
	outb(PIC_MASTER_DATA, 4);
	outb(PIC_SLAVE_DATA,  2);
	outb(PIC_MASTER_DATA, ICW4_8086);
	outb(PIC_SLAVE_DATA,  ICW4_8086);
	outb(PIC_MASTER_DATA, m1);
	outb(PIC_SLAVE_DATA,  m2);
}

void pic_mask(unsigned char mask_master, unsigned char mask_slave)
{
	outb(PIC_MASTER_DATA, mask_master);
	outb(PIC_SLAVE_DATA,  mask_slave);
}

void pic_eoi(unsigned char irq)
{
	if (irq >= 8)
		outb(PIC_SLAVE_CMD, 0x20);
	outb(PIC_MASTER_CMD, 0x20);
}
