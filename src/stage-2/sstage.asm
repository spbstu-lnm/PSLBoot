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
    mov $0x90000, %esp

    call stage2_main
    hlt


# ==== GDT
gdt_start:
    .quad 0 # null dsc
gdt_code:   # Code Segment
    .word 0xFFFF, 0x0000
    .byte 0x00, 0b10011010, 0b11001111, 0x00
gdt_data:   # Data Segment
    .word 0xFFFF, 0x0000
    .byte 0x00, 0b10010010, 0b11001111, 0x00
gdt_end:

gdt_descriptor:
    .word gdt_end - gdt_start - 1
    .long gdt_start


CODE_SEG = gdt_code - gdt_start
DATA_SEG = gdt_data - gdt_start
