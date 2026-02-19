// Minimal integer types (no std headers because we compile freestanding)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

extern "C" void kmain();

// I/O
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile ("inb %1, %0" : "=a" (ret) : "Nd" (port));
    return ret;
}

static inline void outb(unsigned short port, unsigned char data) {
    asm volatile ("outb %0, %1" : : "a" (data), "Nd" (port));
}

// VGA text mode simple writer
volatile unsigned short* VGA = (unsigned short*)0xB8000;
int cursor_x = 0, cursor_y = 0;

static void putchar(char c) {
    const unsigned short attr = (unsigned short)0x07;
    if (c == '\n') {
        cursor_x = 0; cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0) cursor_x--;
        VGA[cursor_y * 80 + cursor_x] = (unsigned short)(' ') | (attr << 8);
    } else {
        VGA[cursor_y * 80 + cursor_x] = (unsigned short)c | (attr << 8);
        cursor_x++;
        if (cursor_x >= 80) { cursor_x = 0; cursor_y++; }
    }
    if (cursor_y >= 25) {
        for (int y = 1; y < 25; ++y)
            for (int x = 0; x < 80; ++x)
                VGA[(y-1)*80 + x] = VGA[y*80 + x];
        cursor_y = 24; cursor_x = 0;
        for (int x = 0; x < 80; ++x) VGA[cursor_y*80 + x] = (unsigned short) ' ' | (attr << 8);
    }
}

static void print(const char* s) {
    while (*s) putchar(*s++);
}

// PS/2 scancode set 1 map (partial); we'll handle shift state manually
static const char scancode_map[128] = {
  0,  27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
 ' ','q','w','e','r','t','y','u','i','o','p','[',']','\n',
 0,'a','s','d','f','g','h','j','k','l',';','\'', '`', 0,
 '\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',
};

// Shifted symbols for common keys
static const char shift_map[128] = {
  0,  0,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
 ' ', 'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
 0,'A','S','D','F','G','H','J','K','L',':','"','~',0,
 '|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ',
};

// Read raw scancode and produce a character, handling shift
static char read_scancode_char() {
    static bool shift = false;
    // wait for output buffer
    while (!(inb(0x64) & 1)) ;
    unsigned char sc = inb(0x60);
    if (sc == 0) return 0;
    if (sc == 0x2A || sc == 0x36) { shift = true; return 0; } // shift down
    if (sc == 0xAA || sc == 0xB6) { shift = false; return 0; } // shift up
    if (sc & 0x80) return 0; // key release for other keys
    if (sc < 128) {
        if (shift) return shift_map[sc];
        return scancode_map[sc];
    }
    return 0;
}

// Read a line into buffer, returns length
static int readline(char* buf, int maxlen) {
    int pos = 0;
    while (1) {
        char c = read_scancode_char();
        if (!c) continue;
        if (c == '\n') { putchar('\n'); buf[pos]=0; return pos; }
        if (c == '\b') {
            if (pos>0) { pos--; putchar('\b'); }
            continue;
        }
        if (pos < maxlen-1) {
            buf[pos++] = c;
            putchar(c);
        }
    }
}

// Calculator implementation moved to C file
extern "C" long calc_eval(const char* s);

// Small assembly demo (adds two ints) implemented in src/math_asm.S
extern "C" int add_asm(int a, int b);

extern "C" void kmain();

extern "C" void kmain();

extern "C" void kmain() {
    // Clear screen
    const unsigned short attr = (unsigned short)0x07;
    for (int y=0;y<25;y++) for (int x=0;x<80;x++) VGA[y*80 + x] = (unsigned short) ' ' | (attr << 8);
    cursor_x = 0; cursor_y = 0;
    print("MiniOS - C++/ASM minimal\n");
    print("Calculadora: escribe expresiones con + - * / y Enter (usa '=' para '+').\n");
    print("Ejemplo: 12=34 (equivale a 12+34)\n\n");

    // demo: call assembly add
    int s = add_asm(7, 5);
    char s_out[32]; int sp=0; int sx = s; if (sx==0) s_out[sp++]='0'; int sneg=0; if (sx<0){ sneg=1; sx=-sx; }
    char srev[32]; int sr=0; while (sx>0){ srev[sr++]= '0' + (sx%10); sx/=10; }
    if (sneg) s_out[sp++]='-'; for (int i=sr-1;i>=0;--i) s_out[sp++]=srev[i]; s_out[sp]=0;
    print("[asm add 7+5= "); print(s_out); print("]\n");

    char buf[256];
    while (1) {
        print("> ");
        int n = readline(buf, sizeof(buf));
        if (n==0) continue;
        // allow '=' as '+' (teclas más accesibles en PS/2)
        for (int i=0; buf[i]; ++i) if (buf[i] == '=') buf[i] = '+';
        // commands
        if (buf[0]=='h' && buf[1]=='i' && buf[2]=='\0') { print("Hola! MiniOS.\n"); continue; }
        if (buf[0]=='q' && buf[1]=='u' && buf[2]=='i' && buf[3]=='t') { print("Apagando...\n"); asm volatile("cli;hlt"); }
        if (buf[0]=='g' && buf[1]=='u' && buf[2]=='i' && buf[3]=='\0') { 
            // launch simple text-mode GUI
            // draw window
            for (int y=0;y<25;y++) for (int x=0;x<80;x++) VGA[y*80 + x] = (unsigned short) ' ' | (0x07 << 8);
            // titlebar
            for (int x=10;x<70;x++) VGA[3*80 + x] = (unsigned short) ' ' | (0x1F << 8);
            const char* title = " MiniOS - Calculadora (modo gráfico) ";
            int tx = 12; for (const char* p=title; *p; ++p) VGA[3*80 + tx++] = (unsigned short)*p | (0x1F << 8);
            // input box
            for (int x=14;x<66;x++) for (int y=8;y<11;y++) VGA[y*80 + x] = (unsigned short) ' ' | (0x70 << 8);
            // label
            const char* lab = "Expr:"; int lx=12; for (const char* p=lab; *p; ++p) VGA[8*80 + lx++] = (unsigned short)*p | (0x70 << 8);
            // result area
            for (int x=14;x<66;x++) for (int y=12;y<15;y++) VGA[y*80 + x] = (unsigned short) ' ' | (0x70 << 8);
            const char* lab2 = "Resultado:"; lx=12; for (const char* p=lab2; *p; ++p) VGA[12*80 + lx++] = (unsigned short)*p | (0x70 << 8);
            // instruction footer
            const char* foot = "Enter = calcular, ESC = salir GUI"; int fx=20; for (const char* p=foot; *p; ++p) VGA[22*80 + fx++] = (unsigned short)*p | (0x07 << 8);

            // GUI input loop
            char gbuf[128]; int gpos=0; for (int i=0;i<128;i++) gbuf[i]=0;
            while (1) {
                char c = read_scancode_char();
                if (!c) continue;
                if (c == 27) { // ESC
                    // clear and return to CLI
                    for (int y=0;y<25;y++) for (int x=0;x<80;x++) VGA[y*80 + x] = (unsigned short) ' ' | (0x07 << 8);
                    break;
                }
                if (c == '\n') {
                    // evaluate
                    gbuf[gpos]=0;
                    long gres = calc_eval(gbuf);
                    // write result to result area (12,14..)
                    char out[64]; int op=0; long xx=gres; if (xx==0) out[op++]='0'; int neg=0; if (xx<0) { neg=1; xx=-xx; }
                    char revv[64]; int rp2=0; while (xx>0) { revv[rp2++]= '0' + (xx%10); xx/=10; }
                    int w=0; if (neg) out[w++]='-'; for (int i=rp2-1;i>=0;--i) out[w++]=revv[i]; out[w]=0;
                    // clear result box
                    for (int x=14;x<66;x++) for (int y=12;y<15;y++) VGA[y*80 + x] = (unsigned short) ' ' | (0x70 << 8);
                    // print out at (13,13)
                    int px=14; for (int i=0; out[i]; ++i) VGA[13*80 + px++] = (unsigned short)out[i] | (0x70 << 8);
                    // reset input
                    for (int x=14;x<66;x++) VGA[8*80 + x] = (unsigned short) ' ' | (0x70 << 8);
                    gpos=0; for (int i=0;i<128;i++) gbuf[i]=0;
                    continue;
                }
                if (c == '\b') { if (gpos>0) { gpos--; gbuf[gpos]=0; VGA[8*80 + 14 + gpos] = (unsigned short) ' ' | (0x70 << 8); } continue; }
                if (gpos < 50) {
                    gbuf[gpos++] = c;
                    VGA[8*80 + 14 + gpos-1] = (unsigned short)c | (0x70 << 8);
                }
            }
            continue;
        }

        long res = calc_eval(buf);
        // print result (simple itoa)
        char out[32]; int p=0; long x = res; if (x==0) out[p++]='0';
        int neg = 0; if (x<0) { neg=1; x=-x; }
        char rev[32]; int rp=0; while (x>0) { rev[rp++]= '0' + (x%10); x/=10; }
        if (neg) out[p++]='-';
        for (int i=rp-1;i>=0;--i) out[p++]=rev[i];
        out[p]=0;
        print("= "); print(out); print("\n");
    }
}
