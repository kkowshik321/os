/* trap.c - C side of machine-mode trap handling and diagnostics. */
#include "types.h"
#include "console.h"
#include "panic.h"
#include "platform.h"
#include "riscv.h"
#include "string.h"
#include "timer.h"
#include "trap.h"

extern void trap_entry(void);               /* arch/riscv/trap.S */

_Static_assert(sizeof(struct trap_frame) == TF_SIZE, "trap frame size mismatch");
_Static_assert(__builtin_offsetof(struct trap_frame, mepc) == TF_MEPC, "mepc offset");
_Static_assert(__builtin_offsetof(struct trap_frame, mstatus) == TF_MSTATUS, "mstatus offset");
_Static_assert(__builtin_offsetof(struct trap_frame, mcause) == TF_MCAUSE, "mcause offset");
_Static_assert(__builtin_offsetof(struct trap_frame, mtval) == TF_MTVAL, "mtval offset");

static volatile u32 trap_depth;             /* >0 while inside trap_handler */

static const char *const reg_names[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
    "s0",   "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7",
    "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

static const char *exception_name(u64 code)
{
    switch (code) {
    case EXC_INST_MISALIGNED:  return "instruction address misaligned";
    case EXC_INST_ACCESS:      return "instruction access fault";
    case EXC_ILLEGAL_INST:     return "illegal instruction";
    case EXC_BREAKPOINT:       return "breakpoint";
    case EXC_LOAD_MISALIGNED:  return "load address misaligned";
    case EXC_LOAD_ACCESS:      return "load access fault";
    case EXC_STORE_MISALIGNED: return "store/AMO address misaligned";
    case EXC_STORE_ACCESS:     return "store/AMO access fault";
    case EXC_ECALL_U:          return "environment call from U-mode";
    case EXC_ECALL_S:          return "environment call from S-mode";
    case EXC_ECALL_M:          return "environment call from M-mode";
    case EXC_INST_PAGE_FAULT:  return "instruction page fault";
    case EXC_LOAD_PAGE_FAULT:  return "load page fault";
    case EXC_STORE_PAGE_FAULT: return "store/AMO page fault";
    default:                   return "unknown exception";
    }
}

static const char *interrupt_name(u64 code)
{
    switch (code) {
    case IRQ_M_SOFT:     return "machine software interrupt";
    case IRQ_M_TIMER:    return "machine timer interrupt";
    case IRQ_M_EXTERNAL: return "machine external interrupt";
    default:             return "unknown interrupt";
    }
}

static void trap_report(const struct trap_frame *tf)
{
    int is_irq = (tf->mcause & MCAUSE_INTERRUPT) != 0;
    u64 code = tf->mcause & MCAUSE_CODE_MASK;

    console_puts("\n*** NanoBoot TRAP ***\n");
    console_puts("mcause : "); console_put_hex(tf->mcause, 16);
    console_puts(is_irq ? "  (interrupt: " : "  (exception: ");
    console_puts(is_irq ? interrupt_name(code) : exception_name(code));
    console_puts(")\n");
    console_puts("mepc   : "); console_put_hex(tf->mepc, 16);    console_putc('\n');
    console_puts("mtval  : "); console_put_hex(tf->mtval, 16);   console_putc('\n');
    console_puts("mstatus: "); console_put_hex(tf->mstatus, 16); console_putc('\n');

    console_puts("registers:\n");
    for (int i = 1; i < 32; i++) {
        console_puts("  ");
        console_puts(reg_names[i]);
        for (usize n = strlen(reg_names[i]); n < 4; n++)
            console_putc(' ');
        console_put_hex(tf->regs[i], 16);
        console_puts((i % 2 == 0) ? "\n" : "   ");   /* two registers per line */
    }
    console_putc('\n');
}

void trap_init(void)
{
    /* Interrupts must be fully off while we install the vector. */
    csr_clear(mstatus, MSTATUS_MIE);
    csr_write(mie, 0);

    csr_write(mtvec, (u64)trap_entry);      /* low 2 bits 0 = direct mode */

    if (csr_read(mtvec) != (u64)trap_entry)
        panic("mtvec did not accept trap_entry");
}

int trap_in_handler(void)
{
    return trap_depth != 0;
}

void trap_handler(struct trap_frame *tf)
{
    trap_depth++;

    /* Recoverable case: machine timer interrupt. Rearm and return via mret. */
    if ((tf->mcause & MCAUSE_INTERRUPT) &&
        (tf->mcause & MCAUSE_CODE_MASK) == IRQ_M_TIMER) {
        timer_irq();
        trap_depth--;
        return;
    }

    /* Everything else is fatal: report and halt. */
    trap_report(tf);
    panic("unrecoverable trap");
}

/* ---- Deliberate traps for testing (shell: traptest) ---- */

int trap_test(const char *kind)
{
    if (*kind == '\0' || kind[0] == 'e') {
        console_puts("executing ebreak...\n");
        __asm__ volatile("ebreak");
    } else if (kind[0] == 'i') {
        console_puts("executing an illegal instruction...\n");
        __asm__ volatile(".word 0x00000000");       /* all-zero is always illegal */
    } else if (kind[0] == 'l') {
        console_puts("loading from unmapped address past end of RAM...\n");
        volatile u64 *bad = (volatile u64 *)(RAM_BASE + RAM_SIZE);
        (void)*bad;
    } else {
        return -1;
    }
    return 0;       /* not reached: every test trap halts the system */
}
