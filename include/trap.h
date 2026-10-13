/*
 * trap.h - trap frame layout shared by arch/riscv/trap.S and kernel/trap.c.
 *
 * trap_entry builds this structure on the stack, so the assembly offsets
 * below MUST match struct trap_frame (checked by _Static_assert in trap.c).
 *
 *   offset  field
 *   0       regs[0]  unused slot (x0 is always 0; keeps regs[n] == xn)
 *   8..248  regs[1..31]  x1..x31 as they were when the trap happened
 *   256     mepc     PC of the trapping instruction (or next, for interrupts)
 *   264     mstatus
 *   272     mcause
 *   280     mtval    faulting address / instruction bits (cause-dependent)
 *   288     total size - a multiple of 16 so sp stays ABI-aligned
 */
#ifndef NANOBOOT_TRAP_H
#define NANOBOOT_TRAP_H

#define TF_REG(n)   ((n) * 8)
#define TF_MEPC     256
#define TF_MSTATUS  264
#define TF_MCAUSE   272
#define TF_MTVAL    280
#define TF_SIZE     288

#ifndef __ASSEMBLER__
#include "types.h"

struct trap_frame {
    u64 regs[32];       /* regs[n] = xn; regs[2] is the pre-trap sp */
    u64 mepc;
    u64 mstatus;
    u64 mcause;
    u64 mtval;
};

void trap_init(void);                       /* install mtvec (interrupts stay off) */
void trap_handler(struct trap_frame *tf);   /* called from trap_entry */
int  trap_test(const char *kind);           /* deliberate trap; -1 if kind unknown */
int  trap_in_handler(void);                 /* non-zero while inside trap_handler */
#endif

#endif /* NANOBOOT_TRAP_H */
