CC      = gcc
AS      = nasm
LD      = ld
OBJCOPY = objcopy
QEMU    = qemu-system-i386

# Semua artefak build (object, dependensi, kernel) dikumpulkan di build/
BUILD   = build

CFLAGS  = -m32 -ffreestanding -fno-stack-protector -fno-builtin -Wall -Wextra -Ikernel/include -pipe -MMD -MP
ASFLAGS = -f win32
LDFLAGS = -m i386pe --image-base 0x100000 -T boot/linker.ld

TARGET_PE  = $(BUILD)/kernel.exe
TARGET_ELF = $(BUILD)/kernel.bin

ASM_SOURCES = boot/boot.asm \
              kernel/process/switch.asm

C_SOURCES   = drivers/vga.c \
              drivers/keyboard.c \
              drivers/timer.c \
              kernel/main.c \
              kernel/stubs.c \
              kernel/cli/cli.c \
              kernel/finance/finance.c \
              kernel/logbook/logbook.c \
              kernel/memory/memory.c \
              kernel/vfs/vfs.c \
              kernel/process/task.c \
              kernel/process/thread.c \
              kernel/sync/sync.c \
              kernel/protection/protect.c \
              kernel/tests/tests.c
              
OBJS = $(addprefix $(BUILD)/,$(ASM_SOURCES:.asm=.o) $(C_SOURCES:.c=.o))
DEPS = $(OBJS:.o=.d)

all: $(TARGET_ELF)

$(TARGET_PE): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(TARGET_ELF): $(TARGET_PE)
	$(OBJCOPY) -O elf32-i386 $< $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

run: $(TARGET_ELF)
	$(QEMU) -kernel $(TARGET_ELF)

clean:
	rm -rf $(BUILD)

-include $(DEPS)

.PHONY: all run clean