/* riscv.h - RISC-V CSR access helpers and architectural bit definitions. */
#ifndef NANOBOOT_RISCV_H
#define NANOBOOT_RISCV_H

/* Read a CSR by name, e.g. csr_read(mcause). */
#define csr_read(csr)                                           \
    ({ unsigned long __v;                                       \
       __asm__ volatile("csrr %0, " #csr : "=r"(__v));          \
       __v; })

/* Write / set bits / clear bits in a CSR. */
#define csr_write(csr, val)                                     \
    __asm__ volatile("csrw " #csr ", %0" :: "rK"((unsigned long)(val)) : "memory")
#define csr_set(csr, val)                                       \
    __asm__ volatile("csrs " #csr ", %0" :: "rK"((unsigned long)(val)) : "memory")
#define csr_clear(csr, val)                                     \
    __asm__ volatile("csrc " #csr ", %0" :: "rK"((unsigned long)(val)) : "memory")

/* mstatus bits */
#define MSTATUS_MIE     (1UL << 3)      /* global machine interrupt enable   */
#define MSTATUS_MPIE    (1UL << 7)      /* MIE value saved at trap entry     */

/* mie / mip bits */
#define MIE_MSIE        (1UL << 3)      /* machine software interrupt enable */
#define MIE_MTIE        (1UL << 7)      /* machine timer interrupt enable    */
#define MIE_MEIE        (1UL << 11)     /* machine external interrupt enable */

/* mcause: top bit set = interrupt, remaining bits = cause code */
#define MCAUSE_INTERRUPT (1UL << 63)
#define MCAUSE_CODE_MASK (~MCAUSE_INTERRUPT)

/* Synchronous exception codes */
#define EXC_INST_MISALIGNED   0
#define EXC_INST_ACCESS       1
#define EXC_ILLEGAL_INST      2
#define EXC_BREAKPOINT        3
#define EXC_LOAD_MISALIGNED   4
#define EXC_LOAD_ACCESS       5
#define EXC_STORE_MISALIGNED  6
#define EXC_STORE_ACCESS      7
#define EXC_ECALL_U           8
#define EXC_ECALL_S           9
#define EXC_ECALL_M           11
#define EXC_INST_PAGE_FAULT   12
#define EXC_LOAD_PAGE_FAULT   13
#define EXC_STORE_PAGE_FAULT  15

/* Interrupt codes */
#define IRQ_M_SOFT            3
#define IRQ_M_TIMER           7
#define IRQ_M_EXTERNAL        11

#endif /* NANOBOOT_RISCV_H */
