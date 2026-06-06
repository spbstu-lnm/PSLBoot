#===============================================================================
#
#   sstage.asm
#
#   OS CW26-1. Polytech Simple Linux Bootloader. Stage 2.
#
#   ENSURE THAT CODE STARTS AT 0x7E00 !!!
#
#===============================================================================

.code16
.att_syntax

.text
.globl stage2_start
.globl jump_linux
.extern stage2_main

stage2_start:
    cli # interrupts atp will cause triple fault

    # ==== Enable A20
    in $0x92, %al
    or $0x02, %al
    and $0xFE, %al # protect System Reset bit
    out %al, $0x92

    # check if A20 is actually enabled??

    lgdt gdt_descriptor # load gdt

    # ==== Switch to protected mode
    mov %cr0, %eax
    or $1, %eax
    mov %eax, %cr0
    ljmp $CODE_SEG, $protected_mode

.code32
protected_mode:
    mov $DATA_SEG, %ax
    mov %ax, %ds
    mov %ax, %ss
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov $0x70000, %esp

    call stage2_main
    hlt

jump_linux:
    mov 4(%esp), %eax
    mov 8(%esp), %esi
    mov $DATA_SEG, %dx
    mov %dx, %ds
    mov %dx, %es
    mov %dx, %fs
    mov %dx, %gs
    mov %dx, %ss
    mov $0x80000, %esp
    xor %ebp, %ebp
    xor %edi, %edi
    xor %ebx, %ebx
    cld
    jmp *%eax

# ==== GDT
# Linux 32-bit boot protocol expects __BOOT_CS = 0x10 and __BOOT_DS = 0x18.
# So we add a dummy entry to push code to offset 0x10 and data to 0x18.
gdt_start:
    .quad 0 # null dsc (0x00)
    .quad 0 # dummy   (0x08)
gdt_code:   # Code Segment (0x10)
    .word 0xFFFF, 0x0000
    .byte 0x00, 0b10011010, 0b11001111, 0x00
gdt_data:   # Data Segment (0x18)
    .word 0xFFFF, 0x0000
    .byte 0x00, 0b10010010, 0b11001111, 0x00
gdt_end:

gdt_descriptor:
    .word gdt_end - gdt_start - 1
    .long gdt_start

CODE_SEG = gdt_code - gdt_start
DATA_SEG = gdt_data - gdt_start