CC      = gcc
AS      = nasm
LD      = ld
OBJCOPY = objcopy
QEMU    = qemu-system-i386

CFLAGS  = -m32 -ffreestanding -fno-stack-protector -fno-builtin -Wall -Wextra -Ikernel/include
ASFLAGS = -f win32
LDFLAGS = -m i386pe --image-base 0x100000 -T boot/linker.ld

TARGET_PE  = kernel.exe
TARGET_ELF = kernel.bin

ASM_SOURCES = boot/boot.asm
C_SOURCES   = drivers/vga.c drivers/keyboard.c kernel/src/thread.c kernel/src/cli.c kernel/src/stubs.c kernel/src/main.c

OBJS = $(ASM_SOURCES:.asm=.o) $(C_SOURCES:.c=.o)

all: $(TARGET_ELF)

$(TARGET_PE): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(TARGET_ELF): $(TARGET_PE)
	$(OBJCOPY) -O elf32-i386 $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

run: $(TARGET_ELF)
	$(QEMU) -kernel $(TARGET_ELF)

clean:
	rm -f $(OBJS) $(TARGET_PE) $(TARGET_ELF)

.PHONY: all run clean