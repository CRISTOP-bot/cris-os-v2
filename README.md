CrisOS v2

<p align="center">
  <img src="docs/images/banner.png" alt="CrisOS v2 banner" width="100%" />
</p><p align="center">
  <img src="https://img.shields.io/badge/architecture-i386-blue?style=for-the-badge" />
  <img src="https://img.shields.io/badge/language-C%2FC%2B%2B%2FASM-orange?style=for-the-badge" />
  <img src="https://img.shields.io/badge/boot-GRUB-purple?style=for-the-badge" />
  <img src="https://img.shields.io/badge/license-GPLv3-green?style=for-the-badge" />
  <img src="https://img.shields.io/badge/status-experimental-red?style=for-the-badge" />
</p>> CrisOS v2 is a modular experimental operating system for i386, written in C / C++ / Assembly, designed to boot as a GRUB-based ISO.




---

📚 Table of Contents

Overview

Features

Demo

GIFs

Architecture Overview

Performance / Hardware Requirements

Project Structure

Build

Run

Shell Commands

Boot Flow

Rootfs

Roadmap

Known Issues / Limitations

Credits / Acknowledgements

About the Author

License



---

✨ Overview

CrisOS v2 is an experimental operating system focused on modularity, readability, and a classic UNIX/Linux-inspired workflow. It includes a text-mode interface, a custom shell, a virtual filesystem, a basic service manager, and support for boot-time modules loaded through Multiboot.

The goal is simple: build a small but real OS with personality, structure, and room to grow.


---

🔥 Features

Freestanding 32-bit kernel with VGA text output.

Interactive shell with Linux-like commands:

pwd, cd, ls, mkdir, rmdir, rm, touch, cp, mv, cat

grep, echo, uname, whoami, df, stat, reboot

systemctl, bootctl, gui

asm add|sub|mul|div <a> <b>

calc <expression>


VFS / CRFS with nested paths, directories, and tree-based file management.

Rootfs loaded as a Multiboot module from the ISO.

Basic service manager inspired by systemd.

Mini boot manager with bootctl support.

Text-based GUI with menu and system status view.

PS/2 keyboard driver with Shift, Caps Lock, Ctrl, and Alt support.

Assembly helpers for arithmetic operations.

Standalone calculator for expression evaluation.

Designed to grow into a more complete, modular, maintainable OS.



---

🎬 Demo

================================
        CrisOS Kernel
================================

[ OK ] Console initialized
[ OK ] Keyboard initialized
[ OK ] Boot manager initialized
[ OK ] Service manager initialized
[ OK ] LCP package manager initialized
[ OK ] Rootfs mounted
[ OK ] VFS initialized

Launching shell...

crisOS:/# ls /
bin  dev  home  root  tmp

crisOS:/# asm add 7 5
12

crisOS:/# calc (12+3)*4
60


---

🎬 GIFs

> Add your animated demos in docs/gifs/ to make the repo feel alive and polished.



Recommended files:

docs/gifs/boot.gif → system boot

docs/gifs/shell.gif → interactive shell

docs/gifs/fs.gif → VFS / CRFS navigation

docs/gifs/gui.gif → menu and status view

docs/gifs/systemctl.gif → service manager


Example layout:

<p align="center">
  <img src="docs/gifs/boot.gif" alt="CrisOS v2 boot" width="48%" />
  <img src="docs/gifs/shell.gif" alt="CrisOS v2 shell" width="48%" />
</p>
---

🧠 Architecture Overview

Simple view of the system:

GRUB
  └── Multiboot
       └── kernel.cpp
            ├── console
            ├── keyboard
            ├── fs / vfs
            ├── systemd
            ├── bootctl
            └── shell
                 ├── commands
                 ├── gui
                 └── calc / asm

This keeps the kernel modular and makes each subsystem easier to evolve independently.


---

⚙️ Performance / Hardware Requirements

Minimum

CPU: i386-compatible processor

RAM: 64 MB

Display: VGA text mode supported

Boot: GRUB / Multiboot-compatible environment


Recommended

CPU: i386 or x86 emulator/VM with stable virtualization support

RAM: 256 MB or more

Emulator: QEMU recommended for development and testing

Boot media: ISO image


Development notes

qemu-system-i386 is the best option for fast iteration.

Real hardware support may vary depending on bootloader, keyboard, and device behavior.



---

🧱 Project Structure

boot/boot.S             # Multiboot header and early boot
linker.ld               # Linker script
src/kernel.cpp          # Kernel entry point and main loop
src/console.cpp/.h      # VGA console and text utilities
src/keyboard.cpp/.h     # PS/2 keyboard driver
src/fs.cpp/.h           # Rootfs loader
src/vfs.cpp/.h          # Virtual filesystem layer
src/shell.cpp/.h        # Linux-like interactive shell
src/gui.cpp/.h          # Text GUI and menu
src/systemd.cpp/.h      # Basic service manager
src/boot.cpp/.h         # Bootctl support
src/asm_utils.S         # Assembly arithmetic helpers
src/calc.c              # Expression evaluator
tools/build_rootfs.py   # Rootfs image generator
rootfs/                 # Files packed into the OS image


---

🛠️ Build

make
make iso

To clean build artifacts:

make clean


---

▶️ Run

With Make

make run

Manually with QEMU

make iso
qemu-system-i386 -cdrom os.iso -m 512M -serial stdio


---

🧭 Shell Commands

Files and navigation

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


Text and search

grep

echo


System

uname

whoami

df

stat

reboot


Services and boot

systemctl

bootctl


Interface

gui


ASM and math

asm add <a> <b>

asm sub <a> <b>

asm mul <a> <b>

asm div <a> <b>

calc <expression>


Examples:

asm add 7 5
calc (12+3)*4
ls /
cd /bin
systemctl status


---

🔌 Boot Flow

1. GRUB loads the kernel and the rootfs module.


2. kmain() initializes the console.


3. The PS/2 keyboard is activated.


4. The rootfs is mounted from Multiboot.


5. The VFS is initialized.


6. Services and boot management are brought up.


7. The interactive shell starts.




---

🗂️ Rootfs

The root filesystem is generated with tools/build_rootfs.py and packages the contents of rootfs/.

That lets the OS boot with its own files, commands, and directory structure without depending on the host operating system.


---

🚀 Roadmap

[ ] Cooperative or preemptive multitasking

[ ] Real scheduler

[ ] PS/2 mouse support

[ ] Framebuffer graphics

[ ] More advanced windows / UI

[ ] Basic TCP/IP networking

[ ] Sound playback

[ ] ELF loader

[ ] Real package manager

[ ] Native text editor

[ ] Shell scripting

[ ] Better permissions and user support



---

⚠️ Known Issues / Limitations

Experimental code may still contain incomplete or changing behavior.

Real hardware behavior can differ from QEMU.

VGA text mode is intentionally simple and not meant to be a full GUI system.

File system and service features are intentionally minimal compared to a full Linux environment.

Some commands and subsystems are still evolving as the project grows.



---

🛠️ Credits / Acknowledgements

Built with the help of the following tools and platforms:

GRUB

GCC / G++

QEMU

GNU Make

Multiboot

OSDev-style bare metal development workflows



---

👨‍💻 About the Author

GitHub: CRISTOP-bot
Discord: cristopher078140
Display name: Cristopher Borjas
TikTok: @cristopherborjas1


---

📄 License

This project is licensed under the GNU General Public License v3.0 (GPLv3).


---

<p align="center">
  Made with ❤️, C, C++, Assembly, and a little controlled chaos.
</p>
