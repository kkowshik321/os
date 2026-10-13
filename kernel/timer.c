/*
 * timer.c - periodic 100 Hz tick from the CLINT machine timer.
 *
 * Order of operations in timer_init() matters:
 *   1. check that a trap vector is installed (trap_init() ran),
 *   2. zero the shared tick counter,
 *   3. program mtimecmp for the first deadline,
 *   4. only then enable the timer source (mie.MTIE), and
 *   5. finally enable interrupts globally (mstatus.MIE).
 * Otherwise a stray interrupt could arrive with nowhere safe to go.
 */
#include "timer.h"
#include "panic.h"
#include "platform.h"
#include "riscv.h"

/* Written by the interrupt handler, read by shell code: must be volatile. */
static volatile u64 ticks;

static volatile u64 *const mtime    = (volatile u64 *)CLINT_MTIME;
static volatile u64 *const mtimecmp = (volatile u64 *)CLINT_MTIMECMP0;

void timer_init(void)
{
    if (csr_read(mtvec) == 0)
        panic("timer_init called before trap_init");

    ticks = 0;
    *mtimecmp = *mtime + TIMER_INTERVAL;    /* first deadline: one tick from now */

    csr_set(mie, MIE_MTIE);                 /* enable machine timer interrupt */
    csr_set(mstatus, MSTATUS_MIE);          /* enable interrupts globally     */
}

/* Runs inside trap_handler with interrupts disabled. Must stay short. */
void timer_irq(void)
{
    ticks = ticks + 1;

    /* Next deadline = previous deadline + interval (no drift over time).
     * If we fell far behind (e.g. paused in a debugger), resync to "now"
     * instead of firing a burst of catch-up interrupts. */
    u64 next = *mtimecmp + TIMER_INTERVAL;
    u64 now  = *mtime;
    if (next <= now)
        next = now + TIMER_INTERVAL;

    *mtimecmp = next;       /* clears the pending interrupt and rearms */
}

u64 timer_ticks(void)
{
    return ticks;           /* aligned 64-bit load: atomic on RV64 */
}
