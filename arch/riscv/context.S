/*
 * context.S - cooperative context switch.
 *
 * void context_switch(struct context *old, struct context *next);
 *                       (old in a0, next in a1)
 *
 * Because this is an ordinary function call from C's point of view, the
 * caller has already saved every caller-saved register it cares about. We
 * only need to preserve what the ABI says a callee must preserve:
 * ra, sp and s0-s11. Offsets must match struct context in include/task.h.
 *
 * We save into *old, load from *next, and `ret` - which jumps to next->ra.
 * For a task that has run before, that is the instruction after its own call
 * to context_switch. For a brand-new task, it is task_bootstrap.
 */
    .section .text
    .globl context_switch
context_switch:
    sd      ra,   0(a0)
    sd      sp,   8(a0)
    sd      s0,  16(a0)
    sd      s1,  24(a0)
    sd      s2,  32(a0)
    sd      s3,  40(a0)
    sd      s4,  48(a0)
    sd      s5,  56(a0)
    sd      s6,  64(a0)
    sd      s7,  72(a0)
    sd      s8,  80(a0)
    sd      s9,  88(a0)
    sd      s10, 96(a0)
    sd      s11, 104(a0)

    ld      ra,   0(a1)
    ld      sp,   8(a1)
    ld      s0,  16(a1)
    ld      s1,  24(a1)
    ld      s2,  32(a1)
    ld      s3,  40(a1)
    ld      s4,  48(a1)
    ld      s5,  56(a1)
    ld      s6,  64(a1)
    ld      s7,  72(a1)
    ld      s8,  80(a1)
    ld      s9,  88(a1)
    ld      s10, 96(a1)
    ld      s11, 104(a1)
    ret

/*
 * First code a new task runs. task_create() put the entry function in s0 and
 * its argument in s1 (callee-saved, so context_switch restored them), and
 * made ra point here. sp is the 16-byte-aligned top of the task's stack.
 *
 * If the entry function returns, we fall through to task_exit(), which
 * marks the task FINISHED and never returns.
 */
    .globl task_bootstrap
task_bootstrap:
    mv      a0, s1              /* argument */
    jalr    s0                  /* entry(arg) */
    call    task_exit           /* entry returned: finish the task */
1:  wfi                         /* unreachable safety net */
    j       1b
