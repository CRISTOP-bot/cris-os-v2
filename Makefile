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
QEMU   = qemu-system-i386

CFLAGS  = -ffreestanding -O2 -Wall -Wextra -m32 -nostdlib -std=c99 -I src
ASFLAGS = -m32 -ffreestanding
LDFLAGS = -m elf_i386 -nostdlib

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
        $(SRC_DIR)/systemd.c \
        $(SRC_DIR)/lcp.c \
        $(SRC_DIR)/gui.c \
        $(SRC_DIR)/memory.c \
        $(SRC_DIR)/calc.c \
        $(SRC_DIR)/calc_app.c

DRV_SRCS = $(DRV_DIR)/console.c \
           $(DRV_DIR)/keyboard.c \
           $(DRV_DIR)/mouse.c \
           $(DRV_DIR)/timer.c \
           $(DRV_DIR)/pic.c

ASMS  = $(SRC_DIR)/asm_utils.S \
        $(SRC_DIR)/math_asm.S \
        boot/boot.S

OBJS  = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
OBJS += $(patsubst $(DRV_DIR)/%.c,$(BUILD_DIR)/drv_%.o,$(DRV_SRCS))
OBJS += $(BUILD_DIR)/boot.o \
        $(BUILD_DIR)/asm_utils.o \
        $(BUILD_DIR)/math_asm.o

all: $(KERNEL)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ISO_DIR)/boot/grub:
	mkdir -p $(ISO_DIR)/boot/grub

$(BUILD_DIR)/boot.o: boot/boot.S | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c boot/boot.S -o $@

$(BUILD_DIR)/asm_utils.o: src/asm_utils.S | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c src/asm_utils.S -o $@

$(BUILD_DIR)/math_asm.o: src/math_asm.S | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c src/math_asm.S -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/drv_%.o: $(DRV_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS) linker.ld | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -T linker.ld -o $(KERNEL) $(OBJS)

$(ROOTFS): tools/build_rootfs.py rootfs/README.txt rootfs/info.txt | $(ISO_DIR)/boot/grub
	$(PYTHON) tools/build_rootfs.py rootfs $(ROOTFS)

iso: all $(ROOTFS)
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.bin

echo-iso: iso
	@if command -v grub-mkrescue >/dev/null 2>&1; then \
		grub-mkrescue -o os.iso $(ISO_DIR); \
	else \
		echo "Instala grub-mkrescue para crear os.iso"; \
	fi

run: echo-iso
	$(QEMU) -cdrom os.iso -m 512M

clean:
	rm -rf $(BUILD_DIR) os.iso $(ISO_DIR)/boot/kernel.bin $(ISO_DIR)/boot/rootfs.bin

.PHONY: all iso echo-iso run clean
