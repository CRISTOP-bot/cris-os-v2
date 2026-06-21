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

COMMON_CFLAGS   = -ffreestanding -O2 -Wall -Wextra -m32 -nostdlib -std=c99
COMMON_ASFLAGS  = -m32 -ffreestanding
LDFLAGS         = -m elf_i386 -nostdlib

all: $(KERNEL)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ISO_DIR)/boot/grub:
	mkdir -p $(ISO_DIR)/boot/grub

$(BUILD_DIR)/boot.o: boot/boot.S | $(BUILD_DIR)
	$(AS) $(COMMON_ASFLAGS) -c boot/boot.S -o $(BUILD_DIR)/boot.o

$(BUILD_DIR)/kernel.o: src/kernel.c | $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) -c src/kernel.c -o $(BUILD_DIR)/kernel.o

$(BUILD_DIR)/console.o: src/console.c | $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) -c src/console.c -o $(BUILD_DIR)/console.o

$(BUILD_DIR)/keyboard.o: src/keyboard.c | $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) -c src/keyboard.c -o $(BUILD_DIR)/keyboard.o

$(BUILD_DIR)/fs.o: src/fs.c | $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) -c src/fs.c -o $(BUILD_DIR)/fs.o

$(BUILD_DIR)/vfs.o: src/vfs.c | $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) -c src/vfs.c -o $(BUILD_DIR)/vfs.o

$(BUILD_DIR)/shell.o: src/shell.c | $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) -c src/shell.c -o $(BUILD_DIR)/shell.o

$(BUILD_DIR)/bootmgr.o: src/boot.c | $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) -c src/boot.c -o $(BUILD_DIR)/bootmgr.o

$(BUILD_DIR)/systemd.o: src/systemd.c | $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) -c src/systemd.c -o $(BUILD_DIR)/systemd.o

$(BUILD_DIR)/asm_utils.o: src/asm_utils.S | $(BUILD_DIR)
	$(AS) $(COMMON_ASFLAGS) -c src/asm_utils.S -o $(BUILD_DIR)/asm_utils.o

$(BUILD_DIR)/gui.o: src/gui.c | $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) -c src/gui.c -o $(BUILD_DIR)/gui.o

$(BUILD_DIR)/lcp.o: src/lcp.c | $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) -c src/lcp.c -o $(BUILD_DIR)/lcp.o

$(BUILD_DIR)/memory.o: src/memory.c | $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) -c src/memory.c -o $(BUILD_DIR)/memory.o

$(BUILD_DIR)/calc.o: src/calc.c | $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) -c src/calc.c -o $(BUILD_DIR)/calc.o

$(BUILD_DIR)/calc_app.o: src/calc_app.c | $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) -c src/calc_app.c -o $(BUILD_DIR)/calc_app.o

$(BUILD_DIR)/idt.o: src/idt.c | $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) -c src/idt.c -o $(BUILD_DIR)/idt.o

$(BUILD_DIR)/math_asm.o: src/math_asm.S | $(BUILD_DIR)
	$(AS) $(COMMON_ASFLAGS) -c src/math_asm.S -o $(BUILD_DIR)/math_asm.o

$(KERNEL): $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/console.o \
           $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/shell.o $(BUILD_DIR)/systemd.o \
           $(BUILD_DIR)/lcp.o $(BUILD_DIR)/gui.o $(BUILD_DIR)/asm_utils.o \
           $(BUILD_DIR)/bootmgr.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/fs.o \
           $(BUILD_DIR)/vfs.o $(BUILD_DIR)/calc.o $(BUILD_DIR)/calc_app.o \
           $(BUILD_DIR)/math_asm.o $(BUILD_DIR)/idt.o linker.ld | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -T linker.ld -o $(KERNEL) \
		$(BUILD_DIR)/boot.o \
		$(BUILD_DIR)/kernel.o \
		$(BUILD_DIR)/console.o \
		$(BUILD_DIR)/keyboard.o \
		$(BUILD_DIR)/shell.o \
		$(BUILD_DIR)/systemd.o \
		$(BUILD_DIR)/lcp.o \
		$(BUILD_DIR)/gui.o \
		$(BUILD_DIR)/asm_utils.o \
		$(BUILD_DIR)/bootmgr.o \
		$(BUILD_DIR)/memory.o \
		$(BUILD_DIR)/fs.o \
		$(BUILD_DIR)/vfs.o \
		$(BUILD_DIR)/calc.o \
		$(BUILD_DIR)/calc_app.o \
		$(BUILD_DIR)/math_asm.o \
		$(BUILD_DIR)/idt.o

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
