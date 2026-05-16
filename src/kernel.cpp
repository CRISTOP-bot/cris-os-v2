// Kernel top-level now delegates to modules: console, keyboard, shell
extern "C" void kmain();

#include "console.h"
#include "shell.h"

extern "C" int add_asm(int a, int b);

extern "C" void kmain() {
    console_clear();
    console_print("MiniOS - modularized\n");
    console_print("Type 'help' for commands.\n\n");

    // demo assembly call
    int s = add_asm(7,5);
    // print simple demo
    char buf[32]; int p=0; int sx = s; if (sx==0) buf[p++]='0'; int neg=0; if (sx<0) { neg=1; sx=-sx; }
    char rev[32]; int rp=0; while (sx>0) { rev[rp++]= '0' + (sx%10); sx/=10; }
    if (neg) buf[p++]='-'; for (int i=rp-1;i>=0;--i) buf[p++]=rev[i]; buf[p]=0;
    console_print("[asm add 7+5= "); console_print(buf); console_print("]\n\n");

    shell_run();
}
