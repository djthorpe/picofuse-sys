ENTRY(_entry_point)

MEMORY
{
    RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 256k
}

SECTIONS
{
    .vectors :
    {
        KEEP(*(.vectors))
    } > RAM

    .binary_info_header :
    {
        KEEP(*(.binary_info_header))
        __binary_info_header_end = .;
    } > RAM

    .embedded_block :
    {
        KEEP(*(.embedded_block))
        __embedded_block_end = .;
    } > RAM

    .reset :
    {
        KEEP(*(.reset))
    } > RAM

    .text :
    {
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);

        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(SORT(.preinit_array.*)))
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);

        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN(__init_array_end = .);

        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN(__fini_array_end = .);

        . = ALIGN(4);
    } > RAM

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > RAM

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > RAM
    __exidx_end = .;

    .binary_info :
    {
        __binary_info_start = .;
        KEEP(*(.binary_info.keep.*))
        *(.binary_info.*)
        __binary_info_end = .;
        . = ALIGN(4);
    } > RAM

    .data :
    {
        __data_start__ = .;
        *(.data*)

        PROVIDE_HIDDEN(__mutex_array_start = .);
        KEEP(*(SORT(.mutex_array.*)))
        KEEP(*(.mutex_array))
        PROVIDE_HIDDEN(__mutex_array_end = .);

        . = ALIGN(4);
        __data_end__ = .;
    } > RAM AT > RAM
    __etext = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM

    .scratch_x :
    {
        __scratch_x_start__ = .;
        *(.scratch_x.*)
        . = ALIGN(4);
        __scratch_x_end__ = .;
    } > RAM
    __scratch_x_source__ = LOADADDR(.scratch_x);

    .scratch_y :
    {
        __scratch_y_start__ = .;
        *(.scratch_y.*)
        . = ALIGN(4);
        __scratch_y_end__ = .;
    } > RAM
    __scratch_y_source__ = LOADADDR(.scratch_y);

    .stack1_dummy (NOLOAD) :
    {
        KEEP(*(.stack1*))
    } > RAM

    .stack_dummy (NOLOAD) :
    {
        KEEP(*(.stack*))
    } > RAM

    .heap (NOLOAD) :
    {
        __end__ = .;
        end = __end__;
    } > RAM

    __HeapLimit = ORIGIN(RAM) + LENGTH(RAM);
    __StackLimit = __HeapLimit;
    __StackOneTop = __StackLimit;
    __StackTop = __HeapLimit;
    __StackOneBottom = __StackOneTop - SIZEOF(.stack1_dummy);
    __StackBottom = __StackTop - SIZEOF(.stack_dummy);
    PROVIDE(__stack = __StackTop);

    ASSERT(__StackBottom >= __end__, "region RAM overflowed")
}