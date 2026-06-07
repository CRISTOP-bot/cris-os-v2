# CrisOS v2

![Architecture](https://img.shields.io/badge/architecture-i386-blue?style=flat-square)
![Language](https://img.shields.io/badge/language-C%2FC%2B%2B%2FASM-orange?style=flat-square)
![Bootloader](https://img.shields.io/badge/boot-GRUB-purple?style=flat-square)
![License](https://img.shields.io/badge/license-GPLv3-green?style=flat-square)
![Status](https://img.shields.io/badge/status-experimental-red?style=flat-square)

**CrisOS v2** es un sistema operativo modular experimental diseñado para la arquitectura **i386**. Desarrollado en C, C++ y ensamblador, su objetivo es proporcionar un entorno base para el estudio de arquitecturas de sistemas, gestión de recursos y desarrollo de kernels bajo el estándar Multiboot.

---

## 📋 Tabla de Contenidos
* [Arquitectura](#-arquitectura)
* [Características Principales](#-características-principales)
* [Requisitos](#-requisitos)
* [Estructura del Proyecto](#-estructura-del-proyecto)
* [Instrucciones de Compilación y Ejecución](#-compilación-y-ejecución)
* [Roadmap](#-roadmap)
* [Licencia](#-licencia)

---

## 🏗️ Arquitectura
CrisOS v2 sigue un diseño modular para garantizar la escalabilidad. El flujo de control se gestiona mediante un kernel que carga módulos al inicio a través del bootloader GRUB.



---

## 🚀 Características Principales
* **Kernel Freestanding:** Entorno de 32 bits sin dependencias de la biblioteca estándar (libc).
* **Gestión de Archivos:** Implementación de un VFS (Virtual File System) con soporte para directorios y carga de `rootfs` vía módulo Multiboot.
* **Interfaz de Usuario:** Consola basada en texto (VGA) con un shell interactivo que incluye comandos nativos de gestión de archivos y diagnóstico.
* **Gestión de Servicios:** Implementación de un gestor de servicios básico tipo *init*.
* **Driver de Entrada:** Controlador de teclado PS/2 con soporte para interrupciones.
* **Utilidades:** Calculadora de expresiones e intérprete de operaciones aritméticas en ensamblador.

---

## ⚙️ Requisitos
* **Compilador:** GCC/G++ (cross-compiler para i386 recomendado).
* **Build System:** `make`.
* **Emulación:** QEMU (para pruebas y depuración).
* **Bootloader:** GRUB (para generación de la imagen ISO).

---

## 🧱 Estructura del Proyecto
```text
├── boot/           # Punto de entrada y cabecera Multiboot
├── src/            # Código fuente del Kernel (C/C++/ASM)
├── tools/          # Scripts de automatización y generación de rootfs
├── rootfs/         # Contenido del sistema de archivos raíz
└── linker.ld       # Script de enlace para el layout de memoria
