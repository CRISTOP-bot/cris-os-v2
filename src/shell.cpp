#include "shell.h"
#include "console.h"
#include "keyboard.h"

extern "C" long calc_eval(const char* s);

void shell_run() {
    char buf[256];
    while (1) {
        console_print("> ");
        int n = keyboard_readline(buf, sizeof(buf));
        if (n == 0) continue;
        // support '=' as '+' for convenience
        for (int i=0; buf[i]; ++i) if (buf[i]=='=') buf[i] = '+';
        if (buf[0]=='h' && buf[1]=='e' && buf[2]=='l' && buf[3]=='p' && buf[4]==0) {
            console_print("help: help clear calc reboot\n");
            continue;
        }
        if (buf[0]=='c' && buf[1]=='l' && buf[2]=='e' && buf[3]=='a' && buf[4]=='r') {
            console_clear();
            continue;
        }
        if (buf[0]=='r' && buf[1]=='e' && buf[2]=='b' && buf[3]=='o' && buf[4]=='o' && buf[5]=='t') {
            console_print("Rebooting...\n");
            asm volatile ("cli;hlt");
        }
        // default: try calculate
        long res = calc_eval(buf);
        // itoa
        char out[32]; int p=0; long x = res; if (x==0) out[p++]='0';
        int neg=0; if (x<0) { neg=1; x=-x; }
        char rev[32]; int rp=0; while (x>0) { rev[rp++]= '0' + (x%10); x/=10; }
        if (neg) out[p++]='-';
        for (int i=rp-1;i>=0;--i) out[p++]=rev[i]; out[p]=0;
        console_print("= "); console_print(out); console_print("\n");
    }
}
