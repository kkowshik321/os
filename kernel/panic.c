/* panic.c - unrecoverable error handling. */
#include "panic.h"
#include "console.h"
#include "riscv.h"

void panic_halt(void)
{
    csr_clear(mstatus, MSTATUS_MIE);        /* no more interrupts */
    for (;;)
        __asm__ volatile("wfi");            /* sleep; never resume */
}

void panic(const char *msg)
{
    console_puts("\nPANIC: ");
    console_puts(msg);
    console_puts("\nSystem halted.\n");
    panic_halt();
}
