#===============================================================================
# PSLBoot Makefile (cross-platform: Windows cmd + Linux/Unix shell)
#
# Targets:
#   make            build build/bootloader.img (a 64 MiB bootable disk image)
#   make install DISK=<img>   write the loader into an existing raw disk image
#   make run        build the image and launch it in QEMU
#   make clean      remove the build directory
#===============================================================================

GAS       = as
GAS_FLAGS = --32

LD        = ld
OBJCOPY   = objcopy

CC        = gcc
CC_FLAGS  = -m32 -march=i386 -fno-pic -static -fno-asynchronous-unwind-tables \
            -fno-stack-protector -ffreestanding -nostdlib -O2

STAGE2_SECTORS = 127
IMAGE_SIZE_MB  = 64

BUILD_DIR = ./build
SRC_DIR   = ./src

#-------------------------------------------------------------------------------
# OS detection. On Windows the OS variable is "Windows_NT"; otherwise assume a
# POSIX shell. Each platform gets its own linker format, helper commands and a
# default QEMU path. This is what lets the same Makefile build under Windows
# (cmd) and on a Linux CI runner.
#-------------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
    LD_FLAGS  = -m i386pe -s -nostdlib --image-base 0
    QEMU     ?= C:/Program Files/qemu/qemu-system-i386.exe
    MKDIR     = if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
    RMDIR     = if exist "$(BUILD_DIR)" rmdir /s /q "$(BUILD_DIR)"
    # On Windows the disk image is assembled with PowerShell.
    PS        = powershell -NoProfile -Command
else
    LD_FLAGS  = -m elf_i386 -s -nostdlib
    QEMU     ?= qemu-system-i386
    MKDIR     = mkdir -p $(BUILD_DIR)
    RMDIR     = rm -rf $(BUILD_DIR)
endif

.PHONY: all clean run install

# default option: make .img file
all: $(BUILD_DIR)/bootloader.img

#-------------------------------------------------------------------------------
# Assemble the final bootable image from the two stages.
#   - Windows: PowerShell creates the file and writes the stages.
#   - Linux:   dd creates a zeroed image and writes the stages.
# Layout: stage 1 -> MBR boot code (446 B) + boot signature (bytes 510..511),
#         stage 2 -> starting at sector 1.
#-------------------------------------------------------------------------------
$(BUILD_DIR)/bootloader.img: $(BUILD_DIR)/fstage.bin $(BUILD_DIR)/sstage.bin
ifeq ($(OS),Windows_NT)
	$(PS) "$$fs=[IO.File]::Open('$@',[IO.FileMode]::Create,[IO.FileAccess]::ReadWrite); $$fs.SetLength($(IMAGE_SIZE_MB)MB); $$fs.Close()"
	$(PS) "$$b=[IO.File]::ReadAllBytes('$(BUILD_DIR)/fstage.bin'); $$fs=[IO.File]::Open('$@',[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite); $$fs.Write($$b,0,446); $$fs.Seek(510,[IO.SeekOrigin]::Begin) > $$null; $$fs.Write($$b,510,2); $$fs.Close()"
	$(PS) "$$b=[IO.File]::ReadAllBytes('$(BUILD_DIR)/sstage.bin'); $$fs=[IO.File]::Open('$@',[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite); $$fs.Seek(512,[IO.SeekOrigin]::Begin) > $$null; $$fs.Write($$b,0,$$b.Length); $$fs.Close()"
else
	dd if=/dev/zero of=$@ bs=1M count=$(IMAGE_SIZE_MB) status=none
	dd if=$(BUILD_DIR)/fstage.bin of=$@ bs=1 count=446 conv=notrunc status=none
	dd if=$(BUILD_DIR)/fstage.bin of=$@ bs=1 seek=510 skip=510 count=2 conv=notrunc status=none
	dd if=$(BUILD_DIR)/sstage.bin of=$@ bs=512 seek=1 conv=notrunc status=none
endif

# link stage 1 asm to .bin
$(BUILD_DIR)/fstage.bin: $(BUILD_DIR)/fstage.o
	$(LD) $(LD_FLAGS) -Ttext 0x7c00 -e _start -o $(BUILD_DIR)/fstage.pe $<
	$(OBJCOPY) -O binary -j .text $(BUILD_DIR)/fstage.pe $@

# link stage 2 asm and C code to .bin
$(BUILD_DIR)/sstage.bin: $(BUILD_DIR)/sstage.o $(BUILD_DIR)/sstagec.o
	$(LD) $(LD_FLAGS) -Ttext 0x7e00 -e stage2_start -o $(BUILD_DIR)/sstage.pe $^
	$(OBJCOPY) -O binary $(BUILD_DIR)/sstage.pe $@

# compile C code to .o
$(BUILD_DIR)/sstagec.o: $(SRC_DIR)/stage-2/sstagec.c | $(BUILD_DIR)
	$(CC) $(CC_FLAGS) -c $< -o $@

# assemble asm code to .o
$(BUILD_DIR)/%.o: $(SRC_DIR)/*/%.asm | $(BUILD_DIR)
	$(GAS) $(GAS_FLAGS) -o $@ $<

# create ./build folder if it does not exist
$(BUILD_DIR):
	$(MKDIR)

#-------------------------------------------------------------------------------
# Install the loader into an existing raw disk image that already contains an
# ext2 Linux partition.  Usage: make install DISK=debian-ext2.img
#-------------------------------------------------------------------------------
install: $(BUILD_DIR)/fstage.bin $(BUILD_DIR)/sstage.bin
ifeq ($(OS),Windows_NT)
	@if "$(DISK)" == "" (echo DISK is not set. Usage: make install DISK=debian-ext2.img & exit /b 1)
	$(PS) "$$b=[IO.File]::ReadAllBytes('$(BUILD_DIR)/fstage.bin'); $$fs=[IO.File]::Open('$(DISK)',[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite); $$fs.Write($$b,0,446); $$fs.Seek(510,[IO.SeekOrigin]::Begin) > $$null; $$fs.Write($$b,510,2); $$fs.Close()"
	$(PS) "$$b=[IO.File]::ReadAllBytes('$(BUILD_DIR)/sstage.bin'); $$fs=[IO.File]::Open('$(DISK)',[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite); $$fs.Seek(512,[IO.SeekOrigin]::Begin) > $$null; $$fs.Write($$b,0,$$b.Length); $$fs.Close()"
else
	@if [ -z "$(DISK)" ]; then echo "DISK is not set. Usage: make install DISK=debian-ext2.img"; exit 1; fi
	dd if=$(BUILD_DIR)/fstage.bin of=$(DISK) bs=1 count=446 conv=notrunc status=none
	dd if=$(BUILD_DIR)/fstage.bin of=$(DISK) bs=1 seek=510 skip=510 count=2 conv=notrunc status=none
	dd if=$(BUILD_DIR)/sstage.bin of=$(DISK) bs=512 seek=1 conv=notrunc status=none
endif

# run bootloader.img via qemu
run: $(BUILD_DIR)/bootloader.img
	"$(QEMU)" -m 256M -drive format=raw,file=$< -serial stdio

# cleanup build artifacts
clean:
	$(RMDIR)
