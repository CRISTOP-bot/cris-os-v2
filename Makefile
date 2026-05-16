## Detect cross-toolchain; fall back to native gcc/g++ with -m32 if missing
ifeq ($(shell which i686-elf-gcc 2>/dev/null),)
	CC = g++
	AS = gcc
	LD = ld
	CFLAGS = -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -m32 -nostdinc
	LDFLAGS = -m elf_i386 -nostdlib
else
	CC = i686-elf-g++
	AS = i686-elf-gcc
	LD = i686-elf-ld
	CFLAGS = -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti -m32 -nostdinc
	LDFLAGS = -m elf_i386 -nostdlib
endif

BUILD_DIR = build
ISO_DIR = iso

KERNEL = $(BUILD_DIR)/kernel.bin

all: $(KERNEL)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(ISO_DIR)/boot/grub

$(BUILD_DIR)/boot.o: boot/boot.S | $(BUILD_DIR)
	$(AS) -m32 -ffreestanding -c boot/boot.S -o $(BUILD_DIR)/boot.o

$(BUILD_DIR)/kernel.o: src/kernel.cpp | $(BUILD_DIR)
    $(CC) $(CFLAGS) -c src/kernel.cpp -o $(BUILD_DIR)/kernel.o

$(BUILD_DIR)/console.o: src/console.cpp | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c src/console.cpp -o $(BUILD_DIR)/console.o

$(BUILD_DIR)/keyboard.o: src/keyboard.cpp | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c src/keyboard.cpp -o $(BUILD_DIR)/keyboard.o

$(BUILD_DIR)/shell.o: src/shell.cpp | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c src/shell.cpp -o $(BUILD_DIR)/shell.o

$(BUILD_DIR)/memory.o: src/memory.cpp | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c src/memory.cpp -o $(BUILD_DIR)/memory.o

$(BUILD_DIR)/calc.o: src/calc.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c src/calc.c -o $(BUILD_DIR)/calc.o

$(BUILD_DIR)/math_asm.o: src/math_asm.S | $(BUILD_DIR)
	$(AS) -m32 -ffreestanding -c src/math_asm.S -o $(BUILD_DIR)/math_asm.o

$(KERNEL): $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/console.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/shell.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/calc.o $(BUILD_DIR)/math_asm.o linker.ld | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -T linker.ld -o $(KERNEL) $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/console.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/shell.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/calc.o $(BUILD_DIR)/math_asm.o

iso: all
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.bin

echo-iso: iso
	grub-mkrescue -o os.iso $(ISO_DIR) 2>/dev/null || echo "Install grub-mkrescue to create ISO"

run: echo-iso
	qemu-system-i386 -cdrom os.iso -m 512M

clean:
	rm -rf $(BUILD_DIR) os.iso $(ISO_DIR)/boot/kernel.bin

.PHONY: all iso echo-iso run clean
