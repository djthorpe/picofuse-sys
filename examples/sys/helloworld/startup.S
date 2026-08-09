.syntax unified
.thumb

.section .reset, "ax"
.align 2

.global _entry_point
.type _entry_point, %function
.thumb_func
_entry_point:
_reset_handler:
    ldr r0, =__etext
    ldr r1, =__data_start__
    ldr r2, =__data_end__

copy_data:
    cmp r1, r2
    bcs clear_bss
    ldr r3, [r0]
    adds r0, #4
    str r3, [r1]
    adds r1, #4
    b copy_data

clear_bss:
    ldr r1, =__bss_start__
    ldr r2, =__bss_end__
    movs r0, #0

zero_bss:
    cmp r1, r2
    bcs run_main
    str r0, [r1]
    adds r1, #4
    b zero_bss

run_main:
    bl main

hang:
    b hang