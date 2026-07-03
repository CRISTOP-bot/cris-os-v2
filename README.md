# CrisOS v3

![Architecture](https://img.shields.io/badge/architecture-i386-blue?style=flat-square)
![Language](https://img.shields.io/badge/language-C%2BASM-orange?style=flat-square)
![Bootloader](https://img.shields.io/badge/boot-GRUB%20Multiboot-purple?style=flat-square)
![License](https://img.shields.io/badge/license-GPLv3-green?style=flat-square)
![Status](https://img.shields.io/badge/status-experimental-red?style=flat-square)

**CrisOS v3** es un sistema operativo modular experimental para **i386** que arranca mediante el estándar **Multiboot** con GRUB. Desarrollado en C y ensamblador x86, proporciona una base para el estudio de arquitectura de sistemas, gestión de recursos y desarrollo de kernels.

---

## 📋 Tabla de Contenidos
- [Características](#-características)
- [Arquitectura](#-arquitectura)
- [Requisitos](#-requisitos)
- [Compilación y Ejecución](#-compilación-y-ejecución)
- [Depuración](#-depuración)
- [Estructura del Proyecto](#-estructura-del-proyecto)
- [Referencia de Make](#-referencia-de-make)
- [Licencia](#-licencia)

---

## 🚀 Características

- **Kernel freestanding** — 32 bits, sin libc, compilado con `-ffreestanding`.
- **GRUB Multiboot** — Arranque conforme al estándar Multiboot v1.
- **GDT/IDT** — Segmentación y tabla de interrupciones completamente configuradas.
- **PIC (8259A)** — Remapeo de IRQ y enmascaramiento.
- **PIT (8253)** — Timer programable a 100 Hz.
- **Teclado PS/2** — Controlador con soporte de interrupciones y layout QWERTY/ES/DE intercambiable.
- **Mouse PS/2** — Controlador con soporte de interrupciones.
- **Consola VGA** — Modo texto 80×25 con soporte completo de colores (16 colores VGA).
- **Shell interactivo** — Prompt coloreado (`cris@crisos:/path$`), historial, comandos nativos.
- **fastfetch** — Información del sistema: CPU (vía CPUID), memoria, uptime, archivos, layout.
- **GUI estilo KDE Plasma** — Escritorio con íconos, panel inferior con reloj y lanzador de aplicaciones.
- **VFS** — Sistema de archivos virtual con soporte para subdirectorios.
- **Rootfs** — Sistema de archivos raíz cargado como módulo Multiboot.
- **Systemd** — Gestor de servicios tipo *init* con arranque y detención básicos.
- **LCP (Process Manager)** — Carga y gestión de procesos desde repositorios locales.
- **Calculadora** — Evaluador de expresiones aritméticas en C y ensamblador.
- **CPUID** — Detección de vendor, familia y características del procesador.
- **Salida serial** — Debug por puerto serie (COM1, 115200 baud).

---

## 🏗️ Arquitectura

```
┌────────────────────────────────────────────┐
│              GRUB (Multiboot)              │
├────────────────────────────────────────────┤
│  kmain() ─── init ─── shell / gui         │
│    ├── gdt_init()                         │
│    ├── idt_init()                         │
│    ├── pic_init() / pic_mask()            │
│    ├── timer_init()                       │
│    ├── keyboard_init()                    │
│    ├── mouse_init()                       │
│    ├── boot_init()                        │
│    ├── systemd_init()                     │
│    └── lcp_init()                         │
├────────────────────────────────────────────┤
│  Shell:  fastfetch, ls, cat, grep, calc   │
│  GUI:    Plasma-like desktop + panel      │
│  VGA:    Colores (16), ASCII art, marcos  │
│  CPUID:  Detección de vendor CPU          │
├────────────────────────────────────────────┤
│  Drivers:    console, keyboard, mouse     │
│              pic, timer                   │
│  Servicios:  systemd, lcp, shell         │
│  Utilidades: calc, kstring, cpuid        │
└────────────────────────────────────────────┘
```

El flujo de arranque comienza en `boot/boot.S` (punto de entrada `start`), que configura la pila, limpia segmentos y salta a `kmain()` en C. `kmain` inicializa los subsistemas en orden, carga el rootfs vía Multiboot y lanza el shell interactivo (o la GUI Plasma escribiendo `gui`).

---

## ⚙️ Requisitos

| Herramienta     | Propósito                                     |
|-----------------|-----------------------------------------------|
| `gcc` / `i686-elf-gcc` | Compilador C para i386 (freestanding)    |
| `ld` / `i686-elf-ld`   | Linker para i386                        |
| `make`          | Automatización de build                       |
| `qemu-system-i386` | Emulación y pruebas                       |
| `grub-mkrescue` | Generación de ISO de arranque                 |
| `xorriso`       | Dependencia de `grub-mkrescue`                |
| `python`        | Construcción del rootfs                        |

Si no tienes un cross-compiler, el `Makefile` detecta `i686-elf-gcc` automáticamente; si no está disponible, usa el `gcc` nativo.

---

## 🔧 Compilación y Ejecución

```bash
# Compilar el kernel
make

# Generar ISO de arranque
make echo-iso

# Ejecutar con QEMU
make run

# Limpiar artefactos de compilación
make clean
```

**Compilación manual paso a paso:**

```bash
make                     # genera build/kernel.bin
make iso                 # genera build/kernel.bin + iso/boot/rootfs.bin
cp build/kernel.bin iso/boot/
grub-mkrescue -o os.iso iso
qemu-system-i386 -cdrom os.iso -m 512M
```

También puedes ejecutar el kernel directamente con QEMU usando el soporte Multiboot:

```bash
qemu-system-i386 -kernel build/kernel.bin -m 512M
```

---

## 🐛 Depuración

El kernel envía mensajes de depuración por el puerto serie (COM1, 115200 baud, 8N1).

```bash
# Con ISO
qemu-system-i386 -cdrom os.iso -m 512M -serial stdio

# Con kernel directo
qemu-system-i386 -kernel build/kernel.bin -m 512M -serial stdio

# Con registro de interrupciones (para diagnosticar faults)
qemu-system-i386 -cdrom os.iso -m 512M -serial stdio -d int -no-reboot

# Con monitor QEMU (Ctrl+Alt+2) y puerto serie en TCP
qemu-system-i386 -cdrom os.iso -m 512M -serial tcp::4444,server -monitor stdio
```

### Flags de compilación clave

```
-fno-strict-aliasing  ← necesario para evitar código incorrecto con -O2
-mno-sse -mno-sse2    ← SSE deshabilitado (el kernel no configura CR4.OSFXSR)
-mno-mmx -mno-3dnow   ← MMX/3DNow! también deshabilitados
```

---

## 🧱 Estructura del Proyecto

```text
├── boot/
│   ├── boot.S              # Punto de entrada (start) y cabecera Multiboot
│   └── grub.cfg            # Configuración de GRUB para el ISO
├── src/
│   ├── kernel.c            # kmain(): inicialización y lanzamiento
│   ├── asm_utils.S         # ISR/IRQ stubs en ensamblador
│   ├── math_asm.S          # Operaciones aritméticas en ASM
│   ├── asm.h               # Constantes y macros para ISR/IRQ
│   ├── gdt.c / gdt.h       # Tabla de descriptores globales
│   ├── idt.c / idt.h       # Tabla de descriptores de interrupción
│   ├── kstring.c / kstring.h    # Funciones de cadena (strcpy, strcmp, etc.)
│   ├── fs.c / fs.h         # Sistema de archivos rootfs
│   ├── vfs.c / vfs.h       # Capa de abstracción VFS
│   ├── shell.c / shell.h   # Shell interactivo
│   ├── systemd.c / systemd.h     # Gestor de servicios init
│   ├── lcp.c / lcp.h       # Gestor de procesos
│   ├── boot.c / boot.h     # Helper de arranque (carga de módulos)
│   ├── gui.c / gui.h       # Interfaz gráfica (WIP)
│   ├── memory.c / memory.h # Gestión de memoria (WIP)
│   ├── calc.c / calc.h     # Calculadora en C
│   ├── calc_app.c / calc_app.h   # Calculadora en ensamblador
│   ├── console.h           # Constantes VGA (colores, puertos)
│   ├── keyboard.h          # Definiciones del teclado PS/2
│   ├── mouse.h             # Definiciones del mouse PS/2
│   ├── pic.h               # Definiciones del PIC 8259A
│   └── timer.h             # Definiciones del PIT 8253
├── drivers/
│   ├── console.c           # Controlador de pantalla VGA (modo texto)
│   ├── keyboard.c          # Controlador de teclado PS/2
│   ├── mouse.c             # Controlador de mouse PS/2
│   ├── pic.c               # Controlador de PIC 8259A
│   └── timer.c             # Controlador de PIT 8253
├── tools/
│   ├── build_rootfs.py     # Genera el rootfs a partir de rootfs/
│   ├── mkiso.py            # Script auxiliar para crear ISO
│   ├── lcp.py              # Script para gestionar repositorios LCP
│   └── lcp_main_repo.json  # Repositorio principal LCP
├── rootfs/
│   ├── bin/                # Binarios incluidos en rootfs
│   ├── boot/               # Archivos de arranque en rootfs
│   ├── share/              # Datos compartidos
│   ├── systemd/            # Servicios systemd
│   ├── info.txt            # Información del sistema de archivos
│   ├── lcp_repo.txt        # Configuración del repositorio LCP
│   └── README.txt          # Documentación del rootfs
├── build/                  # Artefactos de compilación (generado)
├── iso/                    # Árbol para la ISO (generado)
├── linker.ld               # Script de enlace
├── Makefile                # Build system
└── os.iso                  # ISO de arranque (opcional, generada con make run)
```

---

## 📟 Comandos del Shell

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
| `uname`       | Muestra información del sistema                     |
| `whoami`      | Muestra el usuario actual                           |
| `kblayout`    | Cambia el layout del teclado (us/es/de)             |
| `mouse`       | Muestra el estado del mouse                         |
| `calc`        | Calculadora de expresiones                          |
| `asm`         | Operaciones aritméticas en ensamblador              |
| `systemctl`   | Gestor de servicios                                 |
| `bootctl`     | Gestor de arranque                                  |
| `lcp`         | Gestor de paquetes                                  |
| `reboot`      | Reinicia el sistema                                 |
| `panic`       | Falla el kernel (debug)                             |

---

## 📌 Referencia de Make

| Target       | Descripción                                         |
|-------------|-----------------------------------------------------|
| `all`       | Compila el kernel (`build/kernel.bin`)              |
| `iso`       | Compila el kernel + genera rootfs                   |
| `echo-iso`  | `iso` + genera imagen ISO (`os.iso`)                |
| `run`       | `echo-iso` + ejecuta con QEMU                       |
| `clean`     | Elimina `build/`, `os.iso` y restos ISO             |

---

## 📞 Soporte

Para soporte técnico, dudas o reportar problemas, contacta al desarrollador principal:

- **Teléfono:** [+52 833 436 6888](tel:+528334366888)

---

## 📄 Licencia

Este proyecto se distribuye bajo la licencia **GNU General Public License v3**. Consulte el archivo [LICENSE](LICENSE) para más detalles.
