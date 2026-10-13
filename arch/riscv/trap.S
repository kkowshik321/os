/*
 * trap.S - machine-mode trap entry.
 *
 * Every trap (exception or interrupt) lands at trap_entry because mtvec is
 * in direct mode. We build a struct trap_frame (see include/trap.h) on the
 * CURRENT stack, call trap_handler(frame) in C, restore, and mret.
 *
 * The frame is 288 bytes (a multiple of 16), so if sp was 16-byte aligned
 * when the trap happened it is still aligned when we call C.
 *
 * mstatus is saved for diagnostics but NOT written back: hardware already
 * restores MIE from MPIE on mret, and rewriting it could undo changes.
 */
#include "trap.h"

    .section .text
    .align 4                        /* mtvec base must be 4-byte aligned */
    .globl trap_entry
trap_entry:
    addi    sp, sp, -TF_SIZE

    /* Save x1, x3..x31 (x2 = sp is handled separately below). */
    .irp n, 1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    sd      x\n, TF_REG(\n)(sp)
    .endr

    addi    t0, sp, TF_SIZE         /* pre-trap sp */
    sd      t0, TF_REG(2)(sp)

    csrr    t0, mepc
    sd      t0, TF_MEPC(sp)
    csrr    t0, mstatus
    sd      t0, TF_MSTATUS(sp)
    csrr    t0, mcause
    sd      t0, TF_MCAUSE(sp)
    csrr    t0, mtval
    sd      t0, TF_MTVAL(sp)

    mv      a0, sp                  /* argument: struct trap_frame * */
    call    trap_handler

    /* Return path (only reached for recoverable traps, e.g. timer). */
    ld      t0, TF_MEPC(sp)
    csrw    mepc, t0

    .irp n, 1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    ld      x\n, TF_REG(\n)(sp)
    .endr

    addi    sp, sp, TF_SIZE
    mret
