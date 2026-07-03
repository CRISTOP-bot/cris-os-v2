#ifndef ASM_H
#define ASM_H

int add_asm(int a, int b);
int sub_asm(int a, int b);
int mul_asm(int a, int b);
int div_asm(int a, int b);
void sti(void);
void halt_cpu(void);
void reboot_cpu(void);
unsigned char inb(unsigned short port);
void outb(unsigned short port, unsigned char value);
void console_putxy(int x, int y, char c, unsigned char attr);
void memsetw_asm(void *dest, unsigned short value, unsigned int count);
void cpuid(unsigned int code, unsigned int *result);

#endif /* ASM_H */
