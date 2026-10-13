/* timer.h - CLINT machine timer: periodic tick and uptime. */
#ifndef NANOBOOT_TIMER_H
#define NANOBOOT_TIMER_H

#include "types.h"

void timer_init(void);      /* arm the first tick, then enable the interrupt */
void timer_irq(void);       /* called by trap_handler on a machine timer IRQ */
u64  timer_ticks(void);     /* ticks since timer_init() */

#endif /* NANOBOOT_TIMER_H */
