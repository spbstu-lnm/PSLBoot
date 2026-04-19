GAS       = as
GAS_FLAGS = --32

LD        = ld
LD_FLAGS  = -Ttext 0x7c00 --oformat binary -m elf_i386 -s -nostdlib

CC        = gcc
CC_FLAGS  = -m32 -march=i386 -fno-pic -static -fno-asynchronous-unwind-tables \
            -fno-stack-protector -ffreestanding -nostdlib -O2


BUILD_DIR = ./build
SRC_DIR = ./src


# not files
.PHONY: all clean run


# default option: make .img file
all: $(BUILD_DIR)/bootloader.img


# get .img from stages
$(BUILD_DIR)/bootloader.img: $(BUILD_DIR)/fstage.bin
	cat $^ > $@
	truncate -s 10240 $@


# link asm code to .bin
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.o
	$(LD) $(LD_FLAGS) -o $@ $<


# compile asm code to .o
$(BUILD_DIR)/%.o: $(SRC_DIR)/*/%.asm | $(BUILD_DIR)
	$(GAS) $(GAS_FLAGS) -o $@ $<


# link stage 2 asm and C code to .bin
$(BUILD_DIR)/sstage.bin: $(BUILD_DIR)/sstage.o $(BUILD_DIR)/sstagec.o
	$(LD) $(LD_FLAGS) -Ttext 0x8000 -o $@ $^


# compile C code to .o
$(BUILD_DIR)/sstagec.o: $(BUILD_DIR)/stage-2/sstagec.c | $(BUILD_DIR)
	$(CC) $(CC_FLAGS) -c $< -o $@


# create ./build folder if not found
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)


# run bootloader.img via qemu
run: $(BUILD_DIR)/bootloader.img
	qemu-system-i386 -drive format=raw,file=$<


# cleanup build artifacts
clean:
	rm -rf $(BUILD_DIR)
