all:
	as --32 -o ./build/fstage.o fstage.asm
	ld -Ttext 0x7c00 --oformat binary -m elf_i386 -o ./build/fstage.bin ./build/fstage.o

clean:
	rm -rf ./build
