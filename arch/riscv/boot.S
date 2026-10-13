/*
 * boot.S - first code executed on the CPU.
 *
 * With "-bios none -kernel kernel.elf", QEMU's tiny reset ROM jumps to the
 * ELF entry point (0x80000000) in machine mode, with a0 = hart id.
 */

    .section .text.boot, "ax"
    .globl _start
_start:
    /* Only hart 0 runs the kernel; any other hart parks forever. */
    csrr    t0, mhartid
    bnez    t0, park

    /* Global pointer: needed for linker relaxation. Must not be relaxed itself. */
    .option push
    .option norelax
    la      gp, __global_pointer$
    .option pop

    /* Valid stack (16-byte aligned, defined by linker.ld). */
    la      sp, __stack_top

    /* Clear .bss one 64-bit word at a time (linker aligns both ends to 8). */
    la      t0, __bss_start
    la      t1, __bss_end
1:  bgeu    t0, t1, 2f
    sd      zero, 0(t0)
    addi    t0, t0, 8
    j       1b
2:
    call    kernel_main

    /* kernel_main should never return; if it does, sleep safely. */
park:
3:  wfi
    j       3b
