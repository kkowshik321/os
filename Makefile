# NanoBoot Makefile
CROSS   ?= riscv64-unknown-elf-
CC      := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump
QEMU    ?= qemu-system-riscv64

BUILD   := build
TARGET  := $(BUILD)/kernel.elf

ARCHFLAGS := -march=rv64imac_zicsr -mabi=lp64 -mcmodel=medany
# -fno-tree-loop-distribute-patterns: stop GCC turning our memset/memcpy loops
# into calls to memset/memcpy (infinite recursion).
# -MMD -MP: generate header dependency files so header edits trigger rebuilds.
CFLAGS    := $(ARCHFLAGS) -std=c11 -O2 -g -Wall -Wextra -Werror \
             -ffreestanding -fno-builtin -fno-stack-protector -fno-pic \
             -fno-tree-loop-distribute-patterns -MMD -MP \
             -Iinclude
ASFLAGS   := $(ARCHFLAGS) -g -MMD -MP -Iinclude
LDFLAGS   := -nostdlib -static -T linker.ld -Wl,--no-warn-rwx-segments

# New .c/.S files in these directories are picked up automatically.
SRCS_C := $(wildcard kernel/*.c)
SRCS_S := $(wildcard arch/riscv/*.S)
OBJS   := $(SRCS_S:%.S=$(BUILD)/%.o) $(SRCS_C:%.c=$(BUILD)/%.o)

QEMU_FLAGS := -machine virt -nographic -bios none -kernel $(TARGET)

.PHONY: all clean run debug test
all: $(TARGET)

$(TARGET): $(OBJS) linker.ld
	$(CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)/*

# Run the kernel. Exit QEMU with Ctrl-A then X.
run: $(TARGET)
	$(QEMU) $(QEMU_FLAGS)

# Start QEMU paused, waiting for GDB on tcp::1234 (same as QEMU -s -S).
#   riscv64-unknown-elf-gdb build/kernel.elf -ex 'target remote :1234'
debug: $(TARGET)
	$(QEMU) $(QEMU_FLAGS) -S -gdb tcp::1234

# Automated QEMU tests (drive the serial shell from Python).
test: $(TARGET)
	python3 tests/qemu_test.py

-include $(OBJS:.o=.d)
