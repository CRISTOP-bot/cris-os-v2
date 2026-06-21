#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int add_asm(int a, int b);
int sub_asm(int a, int b);
int mul_asm(int a, int b);
int div_asm(int a, int b);
void halt_cpu();
void reboot_cpu();
unsigned char inb(unsigned short port);
unsigned char outb(unsigned short port, unsigned char value);
void console_putxy(int x, int y, char c, unsigned char attr);
void memsetw_asm(void* dest, unsigned short value, unsigned int count);

#ifdef __cplusplus
}
#endif
