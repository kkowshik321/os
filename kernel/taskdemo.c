/*
 * taskdemo.c - two cooperative demo tasks.
 *
 * Each prints a counter and yields, for a finite number of iterations, then
 * returns (task_bootstrap turns the return into task_exit). The shell hands
 * them the CPU with scheduler_start() and gets it back once both finish.
 */
#include "types.h"
#include "console.h"
#include "task.h"

#define DEMO_ITERATIONS 5

static void demo_line(const char *name, u64 i)
{
    console_puts(name);
    console_puts(": iteration ");
    console_put_dec(i);
    console_putc('\n');
}

static void task_a(void *arg)
{
    (void)arg;
    for (u64 i = 0; i < DEMO_ITERATIONS; i++) {
        demo_line("task-a", i);
        if (i == 1) {
            console_puts("task-a: task table while I am running:\n");
            task_list();                /* shows shell READY, a RUNNING, b READY */
        }
        task_yield();                   /* the ONLY place task A gives up the CPU */
    }
}

static void task_b(void *arg)
{
    (void)arg;
    for (u64 i = 0; i < DEMO_ITERATIONS; i++) {
        demo_line("task-b", i);
        task_yield();
    }
}

void taskdemo_run(void){
    if (task_is_active("task-a") || task_is_active("task-b")) {
        console_puts("demo tasks are already running\n");
        return;
    }
    int a = task_create("task-a", task_a, NULL);
    if (a < 0) {
        console_puts("cannot create demo tasks: task table full\n");
        return;
    }

    int b = task_create("task-b", task_b, NULL);
    if (b < 0) {
        console_puts("cannot create demo tasks: task table full\n");
        return;
    }
    console_puts("started demo tasks\n");
    scheduler_start();                  /* returns when both have finished */
    console_puts("demo tasks finished\n");
}
