# Detecta toolchain cruzada; si no existe, usa gcc/g++ nativo con -m32
ifeq ($(shell command -v i686-elf-g++ >/dev/null 2>&1 && echo yes),yes)
	CXX = i686-elf-g++
	CC  = i686-elf-gcc
	AS  = i686-elf-gcc
	LD  = i686-elf-ld
else
	CXX = g++
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

COMMON_CXXFLAGS = -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -fno-stack-protector -m32
COMMON_CFLAGS   = -ffreestanding -O2 -Wall -Wextra -m32
COMMON_ASFLAGS  = -m32 -ffreestanding
LDFLAGS         = -m elf_i386 -nostdlib

all: $(KERNEL)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ISO_DIR)/boot/grub:
	mkdir -p $(ISO_DIR)/boot/grub

$(BUILD_DIR)/boot.o: boot/boot.S | $(BUILD_DIR)
	$(AS) $(COMMON_ASFLAGS) -c boot/boot.S -o $(BUILD_DIR)/boot.o

$(BUILD_DIR)/kernel.o: src/kernel.cpp | $(BUILD_DIR)
	$(CXX) $(COMMON_CXXFLAGS) -c src/kernel.cpp -o $(BUILD_DIR)/kernel.o

$(BUILD_DIR)/console.o: src/console.cpp | $(BUILD_DIR)
	$(CXX) $(COMMON_CXXFLAGS) -c src/console.cpp -o $(BUILD_DIR)/console.o

$(BUILD_DIR)/keyboard.o: src/keyboard.cpp | $(BUILD_DIR)
	$(CXX) $(COMMON_CXXFLAGS) -c src/keyboard.cpp -o $(BUILD_DIR)/keyboard.o

$(BUILD_DIR)/fs.o: src/fs.cpp | $(BUILD_DIR)
	$(CXX) $(COMMON_CXXFLAGS) -c src/fs.cpp -o $(BUILD_DIR)/fs.o

$(BUILD_DIR)/vfs.o: src/vfs.cpp | $(BUILD_DIR)
	$(CXX) $(COMMON_CXXFLAGS) -c src/vfs.cpp -o $(BUILD_DIR)/vfs.o

$(BUILD_DIR)/shell.o: src/shell.cpp | $(BUILD_DIR)
	$(CXX) $(COMMON_CXXFLAGS) -c src/shell.cpp -o $(BUILD_DIR)/shell.o

$(BUILD_DIR)/bootmgr.o: src/boot.cpp | $(BUILD_DIR)
	$(CXX) $(COMMON_CXXFLAGS) -c src/boot.cpp -o $(BUILD_DIR)/bootmgr.o

$(BUILD_DIR)/systemd.o: src/systemd.cpp | $(BUILD_DIR)
	$(CXX) $(COMMON_CXXFLAGS) -c src/systemd.cpp -o $(BUILD_DIR)/systemd.o

$(BUILD_DIR)/asm_utils.o: src/asm_utils.S | $(BUILD_DIR)
	$(CXX) $(COMMON_CXXFLAGS) -c src/asm_utils.S -o $(BUILD_DIR)/asm_utils.o

$(BUILD_DIR)/gui.o: src/gui.cpp | $(BUILD_DIR)
	$(CXX) $(COMMON_CXXFLAGS) -c src/gui.cpp -o $(BUILD_DIR)/gui.o

$(BUILD_DIR)/lcp.o: src/lcp.cpp | $(BUILD_DIR)
	$(CXX) $(COMMON_CXXFLAGS) -c src/lcp.cpp -o $(BUILD_DIR)/lcp.o

$(BUILD_DIR)/memory.o: src/memory.cpp | $(BUILD_DIR)
	$(CXX) $(COMMON_CXXFLAGS) -c src/memory.cpp -o $(BUILD_DIR)/memory.o

$(BUILD_DIR)/calc.o: src/calc.c | $(BUILD_DIR)
	$(CC) $(COMMON_CFLAGS) -c src/calc.c -o $(BUILD_DIR)/calc.o

$(BUILD_DIR)/calc_app.o: src/calc_app.cpp | $(BUILD_DIR)
	$(CXX) $(COMMON_CXXFLAGS) -c src/calc_app.cpp -o $(BUILD_DIR)/calc_app.o

$(BUILD_DIR)/math_asm.o: src/math_asm.S | $(BUILD_DIR)
	$(AS) $(COMMON_ASFLAGS) -c src/math_asm.S -o $(BUILD_DIR)/math_asm.o

$(KERNEL): $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/console.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/shell.o $(BUILD_DIR)/systemd.o $(BUILD_DIR)/lcp.o $(BUILD_DIR)/gui.o $(BUILD_DIR)/asm_utils.o $(BUILD_DIR)/bootmgr.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/fs.o $(BUILD_DIR)/vfs.o $(BUILD_DIR)/calc.o $(BUILD_DIR)/calc_app.o $(BUILD_DIR)/math_asm.o linker.ld | $(BUILD_DIR)
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
		$(BUILD_DIR)/math_asm.o

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
