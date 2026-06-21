#include "gui.h"
#include "console.h"
#include "keyboard.h"
#include "vfs.h"
#include "systemd.h"

static void gui_draw_box(int x, int y, int w, int h, unsigned char attr) {
    for (int row = y; row < y + h; ++row) {
        for (int col = x; col < x + w; ++col) {
            console_putxy(col, row, ' ', attr);
        }
    }
}

static void gui_draw_frame(int x, int y, int w, int h, unsigned char attr) {
    for (int col = x; col < x + w; ++col) {
        console_putxy(col, y, 0xCD, attr);
        console_putxy(col, y + h - 1, 0xCD, attr);
    }
    for (int row = y; row < y + h; ++row) {
        console_putxy(x, row, 0xBA, attr);
        console_putxy(x + w - 1, row, 0xBA, attr);
    }
    console_putxy(x, y, 0xC9, attr);
    console_putxy(x + w - 1, y, 0xBB, attr);
    console_putxy(x, y + h - 1, 0xC8, attr);
    console_putxy(x + w - 1, y + h - 1, 0xBC, attr);
}

static void gui_draw_text(int x, int y, const char* text, unsigned char attr) {
    int col = x;
    while (*text) {
        console_putxy(col++, y, *text++, attr);
    }
}

static void gui_draw_title(const char* title) {
    gui_draw_text(8, 4, title, 0x1E);
}

static void gui_draw_menu() {
    gui_draw_text(10, 7, "[S] Shell   [F] Filesystem   [M] Status   [Q] Salir", 0x1F);
    gui_draw_text(10, 9, "[C] Crear directorio  [T] Touch  [D] Delete", 0x1F);
    gui_draw_text(10, 11, "[R] Lectura de archivo  [G] Buscar  [U] Unmount", 0x1F);
    gui_draw_text(10, 13, "Presiona la tecla correspondiente para seleccionar.", 0x1F);
}

static void gui_draw_status() {
    console_clear_color(0x1F);
    gui_draw_box(5, 3, 70, 18, 0x1F);
    gui_draw_frame(5, 3, 70, 18, 0x17);
    gui_draw_title("Estado del sistema CrisOS");
    gui_draw_text(10, 7, "Kernel: CrisOS i386 en modo protegido", 0x1F);
    gui_draw_text(10, 9, "Comandos: modo texto Linux-like con VFS", 0x1F);
    gui_draw_text(10, 11, "Sistema de archivos: virtual CRFS con soporte Linux", 0x1F);
    gui_draw_text(10, 13, "Servicios: systemd-like y bootctl integrados", 0x1F);
    gui_draw_text(10, 15, "Presiona cualquier tecla para regresar al menu.", 0x1F);
    keyboard_read_char();
}

static void gui_show_filesystem() {
    console_clear_color(0x1F);
    gui_draw_box(5, 3, 70, 18, 0x1F);
    gui_draw_frame(5, 3, 70, 18, 0x17);
    gui_draw_title("Archivo actual y contenido del directorio");
    gui_draw_text(10, 7, "Ruta actual: ", 0x1F);
    gui_draw_text(24, 7, vfs_pwd(), 0x1F);
    gui_draw_text(10, 9, "Contenido:", 0x1F);
    vfs_list(".");
    gui_draw_text(10, 17, "Presiona cualquier tecla para regresar.", 0x1F);
    keyboard_read_char();
}

void gui_show() {
    console_clear_color(0x1F);
    gui_draw_box(5, 3, 70, 18, 0x1F);
    gui_draw_frame(5, 3, 70, 18, 0x17);
    gui_draw_title("Bienvenido a CrisOS GUI");
    gui_draw_text(10, 6, "Interfaz ligera con opciones de shell y filesystem.", 0x1F);
    gui_draw_menu();

    while (1) {
        char c = keyboard_read_char();
        if (!c) continue;
        switch (c) {
            case 'q': case 'Q':
                console_clear();
                return;
            case 's': case 'S':
                console_clear();
                console_print("Iniciando shell desde GUI...\n");
                return;
            case 'm': case 'M':
                gui_draw_status();
                break;
            case 'f': case 'F':
                gui_show_filesystem();
                break;
            case 'c': case 'C':
                console_clear();
                console_print("Use mkdir <ruta> desde shell para crear directorios.\n");
                console_print("Presiona cualquier tecla para volver al GUI.\n");
                keyboard_read_char();
                break;
            case 't': case 'T':
                console_clear();
                console_print("Use touch <ruta> desde shell para crear archivos.\n");
                console_print("Presiona cualquier tecla para volver al GUI.\n");
                keyboard_read_char();
                break;
            case 'd': case 'D':
                console_clear();
                console_print("Use rm <ruta> desde shell para eliminar archivos.\n");
                console_print("Presiona cualquier tecla para volver al GUI.\n");
                keyboard_read_char();
                break;
            case 'r': case 'R':
                console_clear();
                console_print("Use cat <archivo> desde shell para leer contenido.\n");
                console_print("Presiona cualquier tecla para volver al GUI.\n");
                keyboard_read_char();
                break;
            case 'g': case 'G':
                console_clear();
                console_print("Use grep <patron> <archivo> desde shell para buscar.\n");
                console_print("Presiona cualquier tecla para volver al GUI.\n");
                keyboard_read_char();
                break;
            case 'u': case 'U':
                console_clear();
                console_print("Unmount no disponible. Usa cd / o reinicia si es necesario.\n");
                console_print("Presiona cualquier tecla para volver al GUI.\n");
                keyboard_read_char();
                break;
            default:
                break;
        }
        console_clear_color(0x1F);
        gui_draw_box(5, 3, 70, 18, 0x1F);
        gui_draw_frame(5, 3, 70, 18, 0x17);
        gui_draw_title("Bienvenido a CrisOS GUI");
        gui_draw_menu();
    }
}
