/*
 * task.c - fixed-size cooperative task table and round-robin scheduler.
 *
 * Task 0 is the shell: it runs on the boot stack and is "adopted" by
 * task_init(). Other tasks run on static 8 KiB stacks in .bss.
 *
 * Scheduling is purely cooperative and round-robin: task_yield() picks the
 * next READY task after the current one (wrapping around) and switches to it.
 * The timer interrupt never calls into this file.
 */
#include "types.h"
#include "console.h"
#include "panic.h"
#include "string.h"
#include "task.h"
#include "trap.h"

extern void task_bootstrap(void);       /* arch/riscv/context.S */
extern char __stack_bottom[];           /* boot stack, from linker.ld */
extern char __stack_top[];

_Static_assert(sizeof(struct context) == 112, "context layout used by context.S");
_Static_assert(__builtin_offsetof(struct context, sp) == 8, "context.sp offset");
_Static_assert(__builtin_offsetof(struct context, s) == 16, "context.s offset");
_Static_assert(TASK_STACK_SIZE % 16 == 0, "stack size must keep 16-byte alignment");

#define STACK_CANARY 0x4E414E4F424F4F54UL   /* "NANOBOOT" */

static struct task tasks[MAX_TASKS];
/* Slot 0 (the shell) uses the boot stack, so only MAX_TASKS-1 stacks exist. */
static u8 task_stacks[MAX_TASKS - 1][TASK_STACK_SIZE] __attribute__((aligned(16)));
static int current;                     /* index of the running task */

static const char *state_name(enum task_state s)
{
    switch (s) {
    case TASK_UNUSED:   return "UNUSED";
    case TASK_READY:    return "READY";
    case TASK_RUNNING:  return "RUNNING";
    case TASK_FINISHED: return "FINISHED";
    }
    return "?";
}

static void check_canary(const struct task *t)
{
    if (*(volatile u64 *)t->stack_base != STACK_CANARY) {
        console_puts("stack overflow in task '");
        console_puts(t->name);
        console_puts("'\n");
        panic("task stack canary clobbered");
    }
}

void task_init(void)
{
    memset(tasks, 0, sizeof(tasks));

    struct task *shell = &tasks[0];
    shell->id = 0;
    shell->state = TASK_RUNNING;
    memcpy(shell->name, "shell", 6);
    shell->stack_base = (usize)__stack_bottom;
    shell->stack_end  = (usize)__stack_top;
    *(volatile u64 *)shell->stack_base = STACK_CANARY;
    current = 0;
}

int task_create(const char *name, task_entry_t entry, void *arg)
{
    /* Find a slot we can (re)use: never used, or finished. Slot 0 is the shell. */
    for (int i = 1; i < MAX_TASKS; i++) {
        struct task *t = &tasks[i];
        if (t->state != TASK_UNUSED && t->state != TASK_FINISHED)
            continue;

        memset(t, 0, sizeof(*t));
        t->id = i;
        for (int n = 0; n < TASK_NAME_MAX - 1 && name[n]; n++)
            t->name[n] = name[n];

        t->stack_base = (usize)task_stacks[i - 1];
        t->stack_end  = t->stack_base + TASK_STACK_SIZE;    /* 16-byte aligned */
        *(volatile u64 *)t->stack_base = STACK_CANARY;

        /* First context_switch() into this task "returns" into task_bootstrap,
         * which calls entry(arg) using s0/s1 as set here. */
        t->ctx.ra   = (u64)task_bootstrap;
        t->ctx.sp   = t->stack_end;
        t->ctx.s[0] = (u64)entry;
        t->ctx.s[1] = (u64)arg;

        t->state = TASK_READY;
        return i;
    }
    return -1;
}

void task_cancel(int id)
{
    if (id <= 0 || id >= MAX_TASKS)
        return;

    struct task *t = &tasks[id];
    if (t->state != TASK_READY)
        return;

    /* The task has never run, so its stack/context can be discarded safely. */
    memset(t, 0, sizeof(*t));
}

/* Next READY task after `from`, scanning round-robin; -1 if none other than
 * `from` itself is READY. */
static int pick_next(int from)
{
    for (int step = 1; step < MAX_TASKS; step++) {
        int i = (from + step) % MAX_TASKS;
        if (tasks[i].state == TASK_READY)
            return i;
    }
    return -1;
}

static void switch_to(int next)
{
    struct task *old = &tasks[current];
    struct task *new = &tasks[next];

    if (old->state == TASK_RUNNING)     /* a FINISHED task stays FINISHED */
        old->state = TASK_READY;
    new->state = TASK_RUNNING;
    current = next;
    context_switch(&old->ctx, &new->ctx);
}

void task_yield(void)
{
    if (trap_in_handler())
        panic("task_yield called from interrupt context");

    check_canary(&tasks[current]);

    int next = pick_next(current);
    if (next < 0)
        return;                         /* nobody else wants the CPU */
    switch_to(next);
}

void task_exit(void)
{
    if (trap_in_handler())
        panic("task_exit called from interrupt context");
    if (current == 0)
        panic("the shell task cannot exit");

    check_canary(&tasks[current]);
    tasks[current].state = TASK_FINISHED;

    int next = pick_next(current);
    if (next < 0)
        panic("no runnable task after task_exit");
    switch_to(next);                    /* never returns to this task */
    panic("finished task was resumed");
}

void scheduler_start(void)
{
    /* Called from the shell: let every other READY task run (taking turns)
     * until each has finished or there is nothing left to run. */
    while (pick_next(current) >= 0)
        task_yield();
}

static void put_padded(const char *s, usize width)
{
    usize n = strlen(s);
    console_puts(s);
    do {
        console_putc(' ');
    } while (++n < width);
}

void task_list(void)
{
    console_puts("id  name       state      stack\n");
    for (int i = 0; i < MAX_TASKS; i++) {
        const struct task *t = &tasks[i];
        if (t->state == TASK_UNUSED)
            continue;
        console_put_dec((u64)t->id);
        console_puts("   ");
        put_padded(t->name, 11);
        put_padded(state_name(t->state), 11);
        console_put_hex(t->stack_base, 16);
        console_puts(" - ");
        console_put_hex(t->stack_end, 16);
        console_putc('\n');
    }
}

int task_is_active(const char *name)
{
    for (int i = 1; i < MAX_TASKS; i++)
        if ((tasks[i].state == TASK_READY || tasks[i].state == TASK_RUNNING) &&
            strcmp(tasks[i].name, name) == 0)
            return 1;
    return 0;
}
