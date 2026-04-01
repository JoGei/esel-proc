/* picorv32.ld */
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    ROM (x)  : ORIGIN = 0x00000, LENGTH = 64K /* maximum real size determined afterwards */
    RAM (rw) : ORIGIN = 0x10000, LENGTH = 1K
}

__stack_top = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    /* ROM-resident code/data */
    . = ORIGIN(ROM);

    .text : ALIGN(4)
    {
        KEEP(*(.init))
        KEEP(*(.text.startup))
        KEEP(*(.text.startup.*))
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4);
    } > ROM

    /* RAM-resident initialized data; load image packed into ROM */
    .data : ALIGN(4)
    {
        __data_start = .;
        *(.data)
        *(.data.*)
        *(.sdata)
        *(.sdata.*)
        . = ALIGN(4);
        __data_end = .;
    } > RAM AT> ROM

    __data_load_start = LOADADDR(.data);
    __data_load_end   = LOADADDR(.data) + SIZEOF(.data);

    /* RAM-resident zero-init data */
    .bss (NOLOAD) : ALIGN(4)
    {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(.sbss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.eh_frame.*)
        *(.note*)
        *(.riscv.attributes)
    }
}