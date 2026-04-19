################################################################################
#
#   fstage.asm
#
#   OS CW26-1. Polytech Simple Linux Bootloader. Stage 1.
#
#
#   Recommended build command:
#       $ as --32 -o fstage.o fstage.asm
#       $ ld -Ttext 0x7c00 --oformat binary -m elf_i386 -o fstage.bin fstage.o
#
################################################################################

.code16
.att_syntax

# CHECK ADDRESS ON BUILD: -Ttext 0x7C00

start:
    # ==== init
    cli
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $0x7C00, %sp
    sti

    mov %dl, boot_drive # saving boot drive number

    # ==== check extensions present
    mov $0x41, %ah
    mov boot_drive, %dl
    mov $0x55AA, %bx
    int $0x13
    jc disk_error

    # ==== loading Stage 2 from boot drive
    lea dap, %si
    mov $0x42, %ah
    mov boot_drive, %dl
    int $0x13
    jc disk_error

    ljmp $0x0000, $0x7E00 # passing control to Stage 2

disk_error:
    mov $err_msg, %si
.loop:
    lodsb
    or %al, %al
    jz hang
    xor %bh, %bh
    mov $0x0E, %ah
    int $0x10
    jmp .loop
hang:
    hlt
    jmp hang


dap: # Disk Address Packet for int 0x13 (ah = 0x42)
    .byte 0x10     # struct size
    .byte 0x00     # reserved
    .word 16       # sector amt
    .word 0x7E00   # offset
    .word 0x0000   # segment
    .quad 1        # LBA-address

boot_drive: .byte 0
err_msg: .asciz "Disk error!"

.zero (446 - (. - start)) # padding before partition table

.zero 64    # partition table (manual filling)

.word 0xAA55    # bootsect signature
