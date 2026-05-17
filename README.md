# CrisOS v2

CrisOS v2 es un sistema operativo experimental i386 escrito en C/C++ y ensamblador, diseñado para ejecutarse como una imagen ISO booteable con GRUB.

## Características principales

- Kernel freestanding en 32-bit con soporte VGA texto.
- Shell interactiva con comandos estilo Linux:
  - `pwd`, `cd`, `ls`, `mkdir`, `rmdir`, `rm`, `touch`, `cp`, `mv`, `cat`, `grep`, `echo`, `uname`, `whoami`, `df`, `stat`
- Sistema de archivos virtual VFS/CRFS con rutas anidadas y directorios.
- Gestor de servicios estilo `systemd` con `systemctl` y unidades.
- Mini gestor de arranque `bootctl` para gestionar entradas de boot.
- GUI de texto mejorada con menú interactivo y vista de estado.
- Driver de teclado PS/2 con soporte de Shift, Caps Lock, Ctrl y Alt.
- Soporte de operaciones en ensamblador (`asm add|sub|mul|div`) y aplicación de calculadora separada con `calc <expresión>`.

## Estructura del proyecto

- `boot/boot.S` — Multiboot header y arranque inicial.
- `linker.ld` — Script del enlazador.
- `src/kernel.cpp` — Entrada del kernel y ciclo principal.
- `src/console.cpp` / `src/console.h` — Consola VGA y utilidades de texto.
- `src/keyboard.cpp` / `src/keyboard.h` — Driver de teclado PS/2.
- `src/fs.cpp` / `src/fs.h` — Carga de rootfs desde la imagen.
- `src/vfs.cpp` / `src/vfs.h` — Capa virtual de sistema de archivos.
- `src/shell.cpp` / `src/shell.h` — Shell interactiva con comandos Linux-like.
- `src/gui.cpp` / `src/gui.h` — GUI de texto y menú.
- `src/systemd.cpp` / `src/systemd.h` — Gestor de servicios básico.
- `src/boot.cpp` / `src/boot.h` — Soporte para `bootctl`.
- `src/asm_utils.S` — Operaciones aritméticas en ensamblador.
- `src/calc.c` — Evaluador de expresiones en C.
- `tools/build_rootfs.py` — Generador de imagen rootfs.

## Requisitos

- `gcc` / `g++` con soporte `-m32` o toolchain cruzada `i686-elf-gcc`, `i686-elf-g++`, `i686-elf-ld`.
- `grub-mkrescue` para generar la ISO booteable.
- `qemu-system-i386` para emulación y prueba.

## Compilación

```bash
make
make iso
```

Si tu sistema no tiene toolchain cruzada, el `Makefile` busca `i686-elf-g++` / `i686-elf-gcc` y usa `g++` / `gcc` en su lugar.

## Ejecución

```bash
make run
```

O bien:

```bash
make iso
qemu-system-i386 -cdrom os.iso -m 512M -serial stdio
```

## Comandos disponibles en la shell

- Navegación y archivos: `pwd`, `cd`, `ls`, `mkdir`, `rmdir`, `rm`, `touch`, `cp`, `mv`, `cat`
- Búsqueda y texto: `grep`, `echo`
- Sistema: `uname`, `whoami`, `df`, `stat`, `reboot`
- Servicios: `systemctl`, `bootctl`
- GUI: `gui`
- ASM: `asm add|sub|mul|div <a> <b>`

## Notas adicionales

- El rootfs se genera con `tools/build_rootfs.py` y empaqueta el contenido de `rootfs/`.
- El sistema de archivos soporta rutas absolutas y relativas con `.` y `..`.
- La GUI es una interfaz de texto simple con acceso rápido a la shell y estado del sistema.

## Licencia

El proyecto actualmente no incluye una licencia explícita. Añade una licencia si quieres compartirlo públicamente.
