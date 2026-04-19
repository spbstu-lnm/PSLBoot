all:
	as --32 -o fstage.o fstage.asm
	ld -Ttext 0x7c00 --oformat binary -m elf_i386 -o fstage.bin fstage.o
