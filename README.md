CrisOS v2

> CrisOS v2 es un sistema operativo experimental i386 escrito en C / C++ / ensamblador, diseñado para arrancar como una ISO booteable con GRUB.



<p align="center">
  <!-- Reemplaza estas imágenes por tus GIFs reales -->
  <img src="docs/gifs/boot.gif" alt="CrisOS v2 boot" width="48%" />
  <img src="docs/gifs/shell.gif" alt="CrisOS v2 shell" width="48%" />
</p><p align="center">
  <img src="https://img.shields.io/badge/architecture-i386-blue?style=for-the-badge" />
  <img src="https://img.shields.io/badge/language-C%2FC%2B%2B%2FASM-orange?style=for-the-badge" />
  <img src="https://img.shields.io/badge/boot-GRUB-purple?style=for-the-badge" />
  <img src="https://img.shields.io/badge/status-experimental-red?style=for-the-badge" />
</p>
---

✨ Características

Kernel freestanding 32-bit con salida por VGA texto.

Shell interactiva con comandos estilo Linux:

pwd, cd, ls, mkdir, rmdir, rm, touch, cp, mv, cat

grep, echo, uname, whoami, df, stat, reboot

systemctl, bootctl, gui

asm add|sub|mul|div <a> <b>

calc <expresión>


VFS/CRFS con rutas anidadas, directorios y soporte de árbol de archivos.

Rootfs cargado como módulo Multiboot desde la ISO.

Gestor de servicios estilo systemd.

Mini gestor de arranque bootctl.

GUI de texto con menú interactivo y vista de estado.

Driver de teclado PS/2 con soporte para Shift, Caps Lock, Ctrl y Alt.

Operaciones en ensamblador y calculadora separada para expresiones.

Arquitectura pensada para crecer hacia un sistema más completo, modular y mantenible.



---

📸 Vista previa

> Todavía no hay capturas estáticas. Puedes arrancar con GIFs y un banner para que el repo ya se vea vivo y potente 😎




---

🎬 GIFs recomendados

Guarda tus animaciones en docs/gifs/ y enlázalas aquí:

docs/gifs/boot.gif → arranque del sistema

docs/gifs/shell.gif → shell interactiva

docs/gifs/fs.gif → navegación del VFS/CRFS

docs/gifs/gui.gif → menú y estado del sistema

docs/gifs/systemctl.gif → gestión de servicios


Si no tienes GIFs todavía, puedes empezar con capturas y luego convertir un video corto en GIF.


---

🧱 Estructura del proyecto

boot/boot.S             # Multiboot header y arranque inicial
linker.ld               # Script del enlazador
src/kernel.cpp          # Entrada del kernel y ciclo principal
src/console.cpp/.h      # Consola VGA y utilidades de texto
src/keyboard.cpp/.h     # Driver de teclado PS/2
src/fs.cpp/.h           # Carga de rootfs desde la imagen
src/vfs.cpp/.h          # Capa virtual de sistema de archivos
src/shell.cpp/.h        # Shell interactiva con comandos Linux-like
src/gui.cpp/.h          # GUI de texto y menú
src/systemd.cpp/.h      # Gestor de servicios básico
src/boot.cpp/.h         # Soporte para bootctl
src/asm_utils.S         # Operaciones aritméticas en ensamblador
src/calc.c              # Evaluador de expresiones en C
tools/build_rootfs.py   # Generador de imagen rootfs
rootfs/                 # Archivos que se empaquetan en el sistema


---

⚙️ Requisitos

gcc / g++ con soporte -m32 o toolchain cruzada:

i686-elf-gcc

i686-elf-g++

i686-elf-ld


grub-mkrescue para crear la ISO booteable.

qemu-system-i386 para probar el sistema en emulación.

make.


> Si no tienes toolchain cruzada, el Makefile intenta usar gcc / g++ localmente cuando sea posible.




---

🛠️ Compilación

make
make iso

Si quieres limpiar artefactos:

make clean


---

▶️ Ejecución

Con Make

make run

Manualmente con QEMU

make iso
qemu-system-i386 -cdrom os.iso -m 512M -serial stdio


---

🧭 Shell: comandos disponibles

Navegación y archivos

pwd

cd

ls

mkdir

rmdir

rm

touch

cp

mv

cat


Texto y búsqueda

grep

echo


Sistema

uname

whoami

df

stat

reboot


Servicios y arranque

systemctl

bootctl


Interfaz gráfica

gui


ASM y matemáticas

asm add <a> <b>

asm sub <a> <b>

asm mul <a> <b>

asm div <a> <b>

calc <expresión>


Ejemplos:

asm add 7 5
calc (12+3)*4
ls /
cd /bin
systemctl status


---

🔌 Cómo funciona el arranque

1. GRUB carga el kernel y el módulo rootfs.


2. kmain() inicializa la consola.


3. Se activa el teclado PS/2.


4. Se monta el rootfs desde Multiboot.


5. Se inicializa el VFS.


6. Se levantan servicios y el gestor de arranque.


7. Se lanza la shell interactiva.




---

🗂️ Rootfs

El rootfs se genera con tools/build_rootfs.py y empaqueta el contenido de rootfs/.

Esto permite que el sistema arranque con archivos, comandos y estructura propia sin depender de un sistema operativo anfitrión.


---

🧠 Ideas de desarrollo futuro

Multitarea cooperativa o preventiva

Scheduler real

Soporte de mouse PS/2

Framebuffer gráfico

Ventanas y UI más avanzada

Red básica TCP/IP

Reproducción de sonido

Cargador ELF

Package manager real

Editor de texto nativo

Scripting para la shell

Mejoras en permisos y usuarios



---

🐛 Estado actual

CrisOS v2 es un proyecto experimental. Puede contener funciones incompletas, partes en desarrollo o comportamiento cambiante mientras evoluciona.

Aun así, el objetivo es claro: construir un sistema operativo pequeño, modular y con identidad propia.


---

📌 Notas

El sistema está pensado para ejecutarse en i386.

Se usa una arquitectura inspirada en flujos clásicos de UNIX/Linux.

El proyecto está orientado a aprendizaje, experimentación y crecimiento progresivo.



---

🤝 Contribuciones

Las ideas, pruebas, mejoras, reportes de errores y sugerencias son bienvenidas.

Si vas a colaborar, intenta mantener:

estilo consistente,

módulos pequeños,

código legible,

y cambios bien documentados.



---

📄 Licencia

Actualmente el proyecto no incluye una licencia explícita.

Si planeas publicarlo en GitHub, añade una licencia para dejar claro cómo se puede usar, modificar y compartir.


---

<p align="center">
  Hecho con ❤️, C, C++ y un poco de caos controlado.
</p>
