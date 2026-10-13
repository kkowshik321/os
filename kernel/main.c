/* main.c - kernel entry point called from boot.S. */
#include "uart.h"
#include "console.h"
#include "shell.h"
#include "memory.h"
#include "task.h"
#include "timer.h"
#include "trap.h"

void kernel_main(void)
{
    uart_init();
    trap_init();            /* install mtvec first; interrupts stay disabled */
    memory_init();          /* heap bounds come from linker.ld */
    task_init();            /* the boot context becomes task 0, "shell" */
    timer_init();           /* only now arm the timer and enable interrupts */
    console_puts("NanoBoot: RV64 kernel booted successfully\n");

    shell_run();            /* never returns */
}
