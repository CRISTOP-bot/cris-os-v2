# MiniOS — Calculadora (C / Assembly)

MiniOS es un pequeño kernel de ejemplo escrito en C++/C y ensamblador x86. Contiene una calculadora de enteros (implementada en `src/calc.c`) y una rutina de ejemplo en ensamblador (`src/math_asm.S`). El proyecto está diseñado para compilarse en modo freestanding y generarse como una imagen ISO booteable con GRUB.

Características
- Escrita en C++/C y ensamblador x86 (freestanding).
- Interfaz por consola y un modo gráfico muy simple (texto en VGA).
- Evaluador de expresiones aritméticas (soporta + - * / y paréntesis).

Archivos importantes
- `boot/boot.S` — Multiboot header y arranque inicial.
- `linker.ld` — Script del enlazador (dirección de carga).
- `src/kernel.cpp` — Lógica principal del kernel, entrada, VGA, teclado.
- `src/calc.c` / `src/calc.h` — Evaluador de expresiones (C).
- `src/math_asm.S` — Rutina `add_asm` en ensamblador.
- `Makefile` — Reglas para compilar y crear `iso`.

Cómo compilar

```bash
make
make iso
```

Cómo ejecutar en QEMU

```bash
make iso
qemu-system-i386 -cdrom os.iso -m 512M -serial stdio
```

Descargas

- [Descargar os.iso](./os.iso) — imagen ISO booteable (incluida en este repositorio).

Licencia

Este proyecto está disponible sin licencia explícita; añade una licencia si quieres usarlo públicamente.
# MiniOS (ASM + C++)

Proyecto mínimo de un sistema operativo en ensamblador y C++ con una calculadora integrada.

Requisitos (Linux):
- Herramientas para compilación cruzada: `i686-elf-gcc`, `i686-elf-g++`, `i686-elf-ld` (o equivalente)
- `grub-mkrescue` (para crear ISO) y `xorriso`/`grub` paquetes
- `qemu-system-i386` para probar en emulación

Construcción:

1. En la raíz del proyecto ejecutar:

```bash
make
make echo-iso   # crea os.iso si tiene grub-mkrescue
make run        # arranca en QEMU
```

Notas:
- El kernel es un ejemplo muy básico 32-bit (Multiboot) que imprime en pantalla usando VGA text mode.
- El driver de teclado es muy simple y usa polling del controlador PS/2 (puede no funcionar en todas las máquinas virtuales, pero sí en QEMU).
- Para introducir la suma puede usarse `=` si el mapeo de teclado no permite `+` directamente.

Estructura:
- `boot/boot.S` - cabecera multiboot y entry
- `src/kernel.cpp` - kernel en C++ con consola y calculadora
- `linker.ld` - script de enlace
- `iso/boot/grub/grub.cfg` - configuración GRUB
- `Makefile` - reglas para compilar y generar ISO

Si quieres, puedo:
- Añadir soporte para más operadores y paréntesis
- Hacer manejo de teclado más completo (mayúsculas/shift)
- Poner el proyecto en Git y crear un script de instalación de toolchain
# cris-os-v2
