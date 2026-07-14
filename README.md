# NucleOS

![Architecture](https://img.shields.io/badge/architecture-x86__64-blue?style=flat-square)
![Language](https://img.shields.io/badge/language-C%2BASM-orange?style=flat-square)
![Bootloader](https://img.shields.io/badge/boot-GRUB%20Multiboot-purple?style=flat-square)
![License](https://img.shields.io/badge/license-GPLv3-green?style=flat-square)
![Status](https://img.shields.io/badge/status-experimental-red?style=flat-square)

> **Anteriormente conocido como [cris-os-v2](https://github.com/CRISTOP-bot/cris-os-v2).**

**NucleOS** es un sistema operativo modular experimental para **x86_64** que arranca mediante el estándar **Multiboot** con GRUB. Desarrollado en C y ensamblador x86_64, proporciona una base para el estudio de arquitectura de sistemas, gestión de recursos y desarrollo de kernels.

---

## Tabla de Contenidos
- [Características](#características)
- [Arquitectura](#arquitectura)
- [Requisitos](#requisitos)
- [Compilación y Ejecución](#compilación-y-ejecución)
- [Depuración](#depuración)
- [Estructura del Proyecto](#estructura-del-proyecto)
- [Comandos del Shell](#comandos-del-shell)
- [Referencia de Make](#referencia-de-make)
- [Licencia](#licencia)

---

## Características

- **Kernel freestanding x86_64** — 64 bits, sin libc, compilado con `-ffreestanding -m64`.
- **GRUB Multiboot** — Arranque conforme al estándar Multiboot v1.
- **GDT/IDT 64-bit** — Segmentación (L=1 code segments) y tabla de interrupciones con 16-byte entries completamente configuradas.
- **PIC (8259A)** — Remapeo de IRQ y enmascaramiento.
- **PIT (8253)** — Timer programable a 100 Hz.
- **Teclado PS/2** — Controlador con soporte de interrupciones y layout QWERTY/ES/DE intercambiable.
- **Mouse PS/2** — Controlador con aceleración y scroll wheel.
- **Consola VGA** — Modo texto 80×25 con soporte completo de colores (16 colores VGA).
- **Shell interactivo** — Prompt coloreado (`nucleos@nucleos:/path$`), historial, comandos nativos.
- **fastfetch** — Información del sistema: CPU (vía CPUID), memoria, uptime, archivos, layout.
- **GUI estilo KDE Plasma** — Escritorio con íconos, panel inferior con reloj y lanzador de aplicaciones.
- **VFS** — Sistema de archivos virtual con soporte para subdirectorios.
- **Rootfs** — Sistema de archivos raíz cargado como módulo Multiboot.
- **OpenRC** — Gestor de servicios tipo init con arranque y detención.
- **LCP (Package Manager)** — Gestor de paquetes con repositorios y dependencias.
- **Calculadora** — Evaluador de expresiones aritméticas en C y ensamblador.
- **CPUID** — Detección de vendor, familia y características del procesador.
- **PMM** — Allocador de memoria física con bitmap.
- **VMM** — Gestión de memoria virtual con soporte de mapeo de páginas.
- **Heap** — Allocador de memoria con free-list, splitting y coalescing.
- **PCI** — Enumeración de dispositivos PCI.
- **TUI Apps** — nano (editor), hexview, sysinfo, filemanager, htop, calc-tui.
- **8 Juegos** — Con menú de selección y branding NucleOS.
- **Salida serial** — Debug por puerto serie (COM1, 115200 baud).

---

## Arquitectura

```
┌──────────────────────────────────────────────┐
│              GRUB (Multiboot v1)             │
├──────────────────────────────────────────────┤
│  boot.S — 32→64 long mode trampoline        │
│    Temp GDT → far jump → 64-bit GDT         │
│    PML4 → PDP → PD (2MB pages, 1GB map)     │
├──────────────────────────────────────────────┤
│  kmain() ─── init ─── shell / gui           │
│    ├── gdt_init()    (64-bit GDT)           │
│    ├── idt_init()    (256 entries, 16-byte)  │
│    ├── pic_init() / pic_mask()              │
│    ├── pmm_init()    (bitmap allocator)      │
│    ├── vmm_init()    (identity map)          │
│    ├── memory_init() (heap allocator)        │
│    ├── pci_init()    (device enumeration)    │
│    ├── timer_init()  (100 Hz PIT)           │
│    ├── keyboard_init()                       │
│    ├── mouse_init()                          │
│    ├── boot_init()                           │
│    ├── openrc_init() (service manager)       │
│    └── lcp_init()    (package manager)       │
├──────────────────────────────────────────────┤
│  Shell:  fastfetch, ls, cat, grep, calc     │
│  TUI:    nano, hexview, htop, sysinfo       │
│  Games:  8 juegos con menú                  │
│  VGA:    Colores (16), ASCII art, marcos    │
│  CPUID:  Detección de vendor CPU            │
├──────────────────────────────────────────────┤
│  Drivers:    console, keyboard, mouse       │
│              pic, timer                     │
│  Memory:     PMM (bitmap), VMM, heap        │
│  Bus:        PCI enumeration                │
│  Services:   openrc, lcp, shell             │
│  Utilities:  calc, kstring, cpuid           │
└──────────────────────────────────────────────┘
```

El flujo de arranque comienza en `boot/boot.S` (punto de entrada `start`), que configura un GDT temporal de 32 bits, cambia a modo largo 64 bits via far jump, carga un GDT de 64 bits, establece las page tables de arranque (2MB large pages, 1GB identity map) y salta a `kmain()` en C. `kmain` inicializa los subsistemas en orden, carga el rootfs vía Multiboot y lanza el shell interactivo.

---

## Requisitos

| Herramienta        | Propósito                                     |
|--------------------|-----------------------------------------------|
| `gcc`              | Compilador C para x86_64 (freestanding)       |
| `ld`               | Linker para x86_64                            |
| `make`             | Automatización de build                       |
| `qemu-system-x86_64` | Emulación y pruebas                        |
| `grub-mkrescue`    | Generación de ISO de arranque                 |
| `xorriso`          | Dependencia de `grub-mkrescue`                |
| `python`           | Construcción del rootfs                        |
| `mtools`           | Soporte FAT para `grub-mkrescue`              |

Ejecuta `./install-deps.sh` para instalar todas las dependencias automáticamente (soporta Arch, Debian, Fedora, SUSE, Void, Alpine, Gentoo, NixOS, FreeBSD, macOS).

---

## Compilación y Ejecución

```bash
# Instalar dependencias (opcional)
./install-deps.sh

# Compilar el kernel
make clean && make

# Generar ISO de arranque
make echo-iso

# Ejecutar con QEMU
qemu-system-x86_64 -cdrom os.iso -m 256M

# Limpiar artefactos de compilación
make clean
```

### Flags de compilación clave

```
-m64                        ← compilación x86_64
-mno-red-zone               ← obligatorio para kernels
-mcmodel=kernel              ← código/kernel en direcciones bajas
-mno-sse -mno-sse2          ← SSE deshabilitado
-mno-mmx -mno-3dnow         ← MMX/3DNow! deshabilitados
-fno-strict-aliasing        ← necesario para -O2
-std=c99                    ← C99 con __asm__ en vez de asm
```

---

## Depuración

```bash
# Con serial output
qemu-system-x86_64 -cdrom os.iso -m 256M -serial stdio

# Con registro de interrupciones
qemu-system-x86_64 -cdrom os.iso -m 256M -serial stdio -d int -no-reboot

# Sin reinicio en triple fault
qemu-system-x86_64 -cdrom os.iso -m 256M -no-reboot
```

---

## Estructura del Proyecto

```text
├── boot/
│   └── boot.S              # Entry: 32→64 long mode, page tables, jump to kmain
├── src/
│   ├── kernel.c            # kmain(): inicialización y lanzamiento
│   ├── asm_utils.S         # ISR/IRQ stubs x86_64, outb/inb, cpuid
│   ├── math_asm.S          # Operaciones aritméticas en ASM
│   ├── asm.h               # Declaraciones de funciones assembly
│   ├── gdt.c / gdt.h       # Tabla de descriptores globales (64-bit)
│   ├── idt.c / idt.h       # Tabla de descriptores de interrupción (16-byte)
│   ├── kstring.c / kstring.h    # Funciones de cadena (kitoa, kxtoa, kutoa, etc.)
│   ├── fs.c / fs.h         # Sistema de archivos rootfs
│   ├── vfs.c / vfs.h       # Capa de abstracción VFS
│   ├── shell.c / shell.h   # Shell interactivo con TUI apps y juegos
│   ├── openrc.c / openrc.h # Gestor de servicios OpenRC
│   ├── lcp.c / lcp.h       # Gestor de paquetes
│   ├── boot.c / boot.h     # Helper de arranque
│   ├── gui.c / gui.h       # Interfaz gráfica Plasma-like
│   ├── memory.c / memory.h # Heap allocator (free-list, coalescing)
│   ├── pmm.c / pmm.h       # Physical Memory Manager (bitmap)
│   ├── vmm.c / vmm.h       # Virtual Memory Manager (page tables)
│   ├── pci.c / pci.h       # PCI device enumeration
│   ├── calc.c / calc.h     # Calculadora en C
│   ├── calc_app.c          # Calculadora en ensamblador
│   ├── apps.c / apps.h     # Framework TUI (ventanas, menús)
│   ├── app_nano.c          # Editor de texto
│   ├── app_hexview.c       # Visor hexadecimal
│   ├── app_sysinfo.c       # Información del sistema
│   ├── app_filemanager.c   # Administrador de archivos
│   ├── app_htop.c          # Monitor de procesos
│   ├── app_calc.c          # Calculadora TUI
│   ├── games.c / games.h   # 8 juegos con branding NucleOS
│   ├── console.h           # Constantes VGA
│   ├── keyboard.h          # Scancode set completo
│   ├── mouse.h             # Definiciones del mouse PS/2
│   ├── pic.h               # Definiciones del PIC 8259A
│   └── timer.h             # Definiciones del PIT 8253
├── drivers/
│   ├── console.c           # Controlador VGA + kernel_panic_ex()
│   ├── keyboard.c          # Controlador de teclado PS/2
│   ├── mouse.c             # Controlador de mouse PS/2
│   ├── pic.c               # Controlador de PIC 8259A
│   └── timer.c             # Controlador de PIT 8253
├── tools/
│   ├── build_rootfs.py     # Genera el rootfs a partir de rootfs/
│   ├── mkiso.py            # Script auxiliar para crear ISO
│   ├── lcp_main_repo.json  # Repositorio LCP (29 paquetes, 6 repos)
│   └── mkiso.py            # Generador de ISO
├── rootfs/                 # Archivos del sistema de archivos raíz
├── install-deps.sh         # Instalador multiplataforma de dependencias
├── install-deps.bat        # Instalador Windows de dependencias
├── linker.ld               # Script de enlace (elf64-x86-64)
├── Makefile                # Build system (x86_64)
└── os.iso                  # ISO de arranque (generada con make echo-iso)
```

---

## Comandos del Shell

| Comando       | Descripción                                         |
|---------------|-----------------------------------------------------|
| `help`        | Muestra ayuda categorizada                          |
| `fastfetch`   | Información del sistema (CPU, RAM, uptime, etc.)    |
| `gui`         | Inicia la interfaz gráfica estilo KDE Plasma        |
| `ls`          | Lista contenido del directorio                      |
| `pwd`         | Muestra el directorio actual                        |
| `cd`          | Cambia de directorio                                |
| `mkdir`       | Crea un directorio                                  |
| `rmdir`       | Elimina un directorio                               |
| `touch`       | Crea un archivo vacío                               |
| `rm`          | Elimina un archivo                                  |
| `cp`          | Copia un archivo                                    |
| `mv`          | Mueve o renombra un archivo                         |
| `cat`         | Muestra contenido de un archivo                     |
| `grep`        | Busca texto en un archivo                           |
| `echo`        | Imprime texto o escribe a archivo                   |
| `stat`        | Muestra información de un archivo                   |
| `df`          | Muestra uso del sistema de archivos                 |
| `clear`       | Limpia la pantalla                                  |
| `uname`       | Muestra información del sistema (x86_64)            |
| `whoami`      | Muestra el usuario actual                           |
| `kblayout`    | Cambia el layout del teclado (us/es/de)             |
| `mouse`       | Muestra el estado del mouse                         |
| `hexdump`     | Volcado hexadecimal de un archivo                   |
| `wc`          | Cuenta líneas, palabras y caracteres                |
| `head`        | Muestra las primeras líneas de un archivo           |
| `tail`        | Muestra las últimas líneas de un archivo            |
| `calc`        | Calculadora de expresiones                          |
| `asm`         | Operaciones aritméticas en ensamblador              |
| `openrc`      | Gestor de servicios                                 |
| `lcp`         | Gestor de paquetes (29 paquetes, 6 repos)          |
| `games`       | Menú de juegos (8 juegos)                           |
| `nano`        | Editor de texto TUI                                 |
| `hexview`     | Visor hexadecimal TUI                               |
| `sysinfo`     | Panel de información del sistema                    |
| `files`       | Administrador de archivos TUI                       |
| `htop`        | Monitor de procesos                                 |
| `calc-tui`    | Calculadora TUI                                     |
| `reboot`      | Reinicia el sistema                                 |
| `panic`       | Falla el kernel (debug)                             |

---

## Referencia de Make

| Target       | Descripción                                         |
|-------------|-----------------------------------------------------|
| `all`       | Compila el kernel (`build/kernel.bin`)              |
| `iso`       | Compila el kernel + genera rootfs                   |
| `echo-iso`  | `iso` + genera imagen ISO (`os.iso`)                |
| `run`       | `echo-iso` + ejecuta con QEMU                       |
| `clean`     | Elimina `build/`, `os.iso` y restos ISO             |

---

## Historial

Este proyecto fue originalmente llamado **CrisOS** (repo: [cris-os-v2](https://github.com/CRISTOP-bot/cris-os-v2)), desarrollado para arquitectura i386. Fue renombrado a **NucleOS** y portado a **x86_64** con soporte completo de modo largo.

---

## Licencia

Este proyecto se distribuye bajo la licencia **GNU General Public License v3**. Consulte el archivo [LICENSE](LICENSE) para más detalles.
