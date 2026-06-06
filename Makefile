GAS       = as
GAS_FLAGS = --32

LD        = ld
LD_FLAGS  = -m i386pe -s -nostdlib --image-base 0

OBJCOPY   = objcopy

CC        = gcc
CC_FLAGS  = -m32 -march=i386 -fno-pic -static -fno-asynchronous-unwind-tables \
            -fno-stack-protector -ffreestanding -nostdlib -O2

PS        = powershell -NoProfile -Command
QEMU      ?= C:/Program Files/qemu/qemu-system-i386.exe

STAGE2_SECTORS = 127
IMAGE_SIZE_MB = 64

BUILD_DIR = ./build
SRC_DIR = ./src


# not files
.PHONY: all clean run install check-stage2-size


# default option: make .img file
all: $(BUILD_DIR)/bootloader.img


# get .img from stages
$(BUILD_DIR)/bootloader.img: $(BUILD_DIR)/fstage.bin $(BUILD_DIR)/sstage.bin
	$(PS) "$$fs=[IO.File]::Open('$@',[IO.FileMode]::Create,[IO.FileAccess]::ReadWrite); $$fs.SetLength($(IMAGE_SIZE_MB)MB); $$fs.Close()"
	$(PS) "$$b=[IO.File]::ReadAllBytes('$(BUILD_DIR)/fstage.bin'); $$fs=[IO.File]::Open('$@',[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite); $$fs.Write($$b,0,446); $$fs.Seek(510,[IO.SeekOrigin]::Begin) > $$null; $$fs.Write($$b,510,2); $$fs.Close()"
	$(PS) "$$b=[IO.File]::ReadAllBytes('$(BUILD_DIR)/sstage.bin'); $$fs=[IO.File]::Open('$@',[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite); $$fs.Seek(512,[IO.SeekOrigin]::Begin) > $$null; $$fs.Write($$b,0,$$b.Length); $$fs.Close()"


# link asm code to .bin
$(BUILD_DIR)/fstage.bin: $(BUILD_DIR)/fstage.o
	$(LD) $(LD_FLAGS) -Ttext 0x7c00 -e _start -o $(BUILD_DIR)/fstage.pe $<
	$(OBJCOPY) -O binary -j .text $(BUILD_DIR)/fstage.pe $@


# link stage 2 asm and C code to .bin
$(BUILD_DIR)/sstage.bin: $(BUILD_DIR)/sstage.o $(BUILD_DIR)/sstagec.o
	$(LD) $(LD_FLAGS) -Ttext 0x7e00 -e stage2_start -o $(BUILD_DIR)/sstage.pe $^
	$(OBJCOPY) -O binary $(BUILD_DIR)/sstage.pe $@
	powershell -NoProfile -Command "if ((Get-Item '$@').Length -gt ($(STAGE2_SECTORS) * 512)) { throw 'Stage 2 is larger than $(STAGE2_SECTORS) sectors' }"


# compile C code to .o
$(BUILD_DIR)/sstagec.o: $(SRC_DIR)/stage-2/sstagec.c | $(BUILD_DIR)
	$(CC) $(CC_FLAGS) -c $< -o $@


# compile asm code to .o
$(BUILD_DIR)/%.o: $(SRC_DIR)/*/%.asm | $(BUILD_DIR)
	$(GAS) $(GAS_FLAGS) -o $@ $<


# create ./build folder if not found
$(BUILD_DIR):
	if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"


# install loader into an existing raw disk image with an ext2 Linux partition.
# Usage: make install DISK=debian-ext2.img
install: $(BUILD_DIR)/fstage.bin $(BUILD_DIR)/sstage.bin
	@if "$(DISK)" == "" (echo DISK is not set. Usage: make install DISK=debian-ext2.img & exit /b 1)
	$(PS) "$$b=[IO.File]::ReadAllBytes('$(BUILD_DIR)/fstage.bin'); $$fs=[IO.File]::Open('$(DISK)',[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite); $$fs.Write($$b,0,446); $$fs.Seek(510,[IO.SeekOrigin]::Begin) > $$null; $$fs.Write($$b,510,2); $$fs.Close()"
	$(PS) "$$b=[IO.File]::ReadAllBytes('$(BUILD_DIR)/sstage.bin'); $$fs=[IO.File]::Open('$(DISK)',[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite); $$fs.Seek(512,[IO.SeekOrigin]::Begin) > $$null; $$fs.Write($$b,0,$$b.Length); $$fs.Close()"


# run bootloader.img via qemu
run: $(BUILD_DIR)/bootloader.img
	"$(QEMU)" -m 256M -drive format=raw,file=$< -serial stdio


# cleanup build artifacts
clean:
	if exist "$(BUILD_DIR)" rmdir /s /q "$(BUILD_DIR)"
