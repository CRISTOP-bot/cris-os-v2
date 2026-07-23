ifeq ($(shell command -v i686-elf-gcc >/dev/null 2>&1 && echo yes),yes)
	CC  = i686-elf-gcc
	AS  = i686-elf-gcc
	LD  = i686-elf-ld
else
	CC  = gcc
	AS  = gcc
	LD  = ld
endif

BUILD_DIR = build
ISO_DIR   = iso
PYTHON    ?= python

KERNEL = $(BUILD_DIR)/kernel.bin
ROOTFS = $(ISO_DIR)/boot/rootfs.bin
QEMU   = qemu-system-x86_64

CFLAGS  = -ffreestanding -O2 -Wall -Wextra -m64 -nostdlib -std=c99 -I src -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -fno-strict-aliasing -mno-red-zone -mcmodel=kernel -fno-pic -fno-pie
ASFLAGS = -m64 -ffreestanding
LDFLAGS = -m elf_x86_64 -nostdlib

RUSTC      = rustc
RUST_TARGET = x86_64-unknown-linux-gnu
RUSTFLAGS  = -C no-redzone=yes -C code-model=kernel -C relocation-model=static
RUSTFLAGS += -C panic=abort -C debuginfo=0 -C opt-level=2
RUST_SRC   = src/rust/rust_kernel.rs
RUST_OBJ   = $(BUILD_DIR)/rust_kernel.o

SRC_DIR  = src
DRV_DIR  = drivers

SRCS  = $(SRC_DIR)/kernel.c \
        $(SRC_DIR)/kstring.c \
        $(SRC_DIR)/gdt.c \
        $(SRC_DIR)/idt.c \
        $(SRC_DIR)/fs.c \
        $(SRC_DIR)/vfs.c \
        $(SRC_DIR)/shell.c \
        $(SRC_DIR)/boot.c \
        $(SRC_DIR)/openrc.c \
        $(SRC_DIR)/lcp.c \
        $(SRC_DIR)/gui.c \
        $(SRC_DIR)/memory.c \
        $(SRC_DIR)/calc.c \
        $(SRC_DIR)/calc_app.c \
        $(SRC_DIR)/pci.c \
        $(SRC_DIR)/games.c \
        $(SRC_DIR)/pmm.c \
        $(SRC_DIR)/vmm.c \
        $(SRC_DIR)/apps.c \
        $(SRC_DIR)/app_nano.c \
        $(SRC_DIR)/app_hexview.c \
        $(SRC_DIR)/app_sysinfo.c \
        $(SRC_DIR)/app_filemanager.c \
        $(SRC_DIR)/app_htop.c \
        $(SRC_DIR)/app_calc.c \
        $(SRC_DIR)/tss.c \
        $(SRC_DIR)/syscall.c \
        $(SRC_DIR)/process.c \
        $(SRC_DIR)/serial.c \
        $(SRC_DIR)/persist.c \
        $(SRC_DIR)/net.c \
        $(SRC_DIR)/ata.c \
        $(SRC_DIR)/part.c \
        $(SRC_DIR)/ext2.c \
        $(SRC_DIR)/installer.c

DRV_SRCS = $(DRV_DIR)/console.c \
           $(DRV_DIR)/keyboard.c \
           $(DRV_DIR)/mouse.c \
           $(DRV_DIR)/timer.c \
           $(DRV_DIR)/pic.c

ASMS  = $(SRC_DIR)/asm_utils.S \
        $(SRC_DIR)/math_asm.S \
        $(SRC_DIR)/ctx_switch.S \
        boot/boot.S

OBJS  = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
OBJS += $(patsubst $(DRV_DIR)/%.c,$(BUILD_DIR)/drv_%.o,$(DRV_SRCS))
OBJS += $(BUILD_DIR)/boot_entry.o \
        $(BUILD_DIR)/asm_utils.o \
        $(BUILD_DIR)/math_asm.o \
        $(BUILD_DIR)/ctx_switch.o \
        $(RUST_OBJ)

all: $(KERNEL)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ISO_DIR)/boot/grub:
	mkdir -p $(ISO_DIR)/boot/grub

$(BUILD_DIR)/boot_entry.o: boot/boot.S | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c boot/boot.S -o $@

$(BUILD_DIR)/asm_utils.o: src/asm_utils.S | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c src/asm_utils.S -o $@

$(BUILD_DIR)/math_asm.o: src/math_asm.S | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c src/math_asm.S -o $@

$(BUILD_DIR)/ctx_switch.o: src/ctx_switch.S | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c src/ctx_switch.S -o $@

$(RUST_OBJ): $(RUST_SRC) | $(BUILD_DIR)
	$(RUSTC) --target $(RUST_TARGET) --crate-type staticlib $(RUSTFLAGS) --emit obj -o $@ $(RUST_SRC)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/drv_%.o: $(DRV_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS) linker.ld | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -T linker.ld -o $(KERNEL) $(OBJS)

$(ISO_DIR)/installer: tools/nucleos-install tools/installer/__init__.py tools/installer/ui.py tools/installer/disk.py tools/installer/config.py tools/installer/install.py | $(ISO_DIR)/boot/grub
	mkdir -p $@
	cp -r tools/installer $(ISO_DIR)/
	cp tools/nucleos-install $@/
	cp tools/lcp.py tools/lcp_main_repo.json $@/ 2>/dev/null || true
	rm -rf $@/__pycache__
	@echo "  Instalador copiado al ISO"

$(ROOTFS): tools/build_rootfs.py rootfs/README.txt rootfs/info.txt | $(ISO_DIR)/boot/grub
	$(PYTHON) tools/build_rootfs.py rootfs $(ROOTFS)

iso: all $(ROOTFS) $(ISO_DIR)/installer
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.bin

echo-iso: iso
	@if command -v grub-mkrescue >/dev/null 2>&1; then \
		grub-mkrescue -o os.iso $(ISO_DIR); \
	else \
		echo "Instala grub-mkrescue para crear os.iso"; \
	fi

run: echo-iso
	$(QEMU) -cdrom os.iso -m 512M

installer:
	@echo ""
	@echo "  Para ejecutar el instalador DESDE EL ISO/USB:"
	@echo "    1. Monta el ISO/USB:  mount /dev/sdX1 /mnt"
	@echo "    2. Ejecuta:           sudo python /mnt/installer/nucleos-install"
	@echo ""
	@echo "  Para ejecutar el instalador LOCALMENTE:"
	@echo "    sudo tools/nucleos-install"
	@echo ""
	@echo "  Requiere: grub-install, grub-mkrescue, python3, sfdisk, mkfs.*"
	@echo "  Debes compilar primero: make iso"
	@echo ""

installer-usb:
	@echo ""
	@echo "  Crear USB booteable con instalador:"
	@echo "    sudo bash tools/make-usb.sh /dev/sdX"
	@echo ""
	@echo "  ADVERTENCIA: Esto BORRA todos los datos del dispositivo"
	@echo ""

clean:
	rm -rf $(BUILD_DIR) os.iso $(ISO_DIR)/boot/kernel.bin $(ISO_DIR)/boot/rootfs.bin $(ISO_DIR)/installer

.PHONY: all iso echo-iso run clean installer installer-usb
