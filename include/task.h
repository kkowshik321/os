/*
 * task.h - cooperative multitasking (Module 6).
 *
 * Tasks switch ONLY when a task calls task_yield() (or task_exit()).
 * Nothing in interrupt context ever switches tasks.
 */
#ifndef NANOBOOT_TASK_H
#define NANOBOOT_TASK_H

#include "types.h"

#define MAX_TASKS        6          /* including task 0, the shell */
#define TASK_NAME_MAX    16
#define TASK_STACK_SIZE  8192       /* bytes per task, 16-byte aligned */

enum task_state {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_FINISHED,
};

/*
 * Callee-saved context, exactly what the RISC-V calling convention requires
 * a function to preserve. Layout is used by arch/riscv/context.S:
 *   ra @0, sp @8, s0..s11 @16..104  (112 bytes)
 */
struct context {
    u64 ra;
    u64 sp;
    u64 s[12];
};

struct task {
    int             id;
    enum task_state state;
    char            name[TASK_NAME_MAX];
    struct context  ctx;
    usize           stack_base;     /* lowest address of the stack */
    usize           stack_end;      /* one past the highest address (initial sp) */
};

typedef void (*task_entry_t)(void *arg);

void task_init(void);                       /* adopt the boot context as task 0 "shell" */
int  task_create(const char *name, task_entry_t entry, void *arg);   /* id, or -1 */
void task_yield(void);                      /* run the next READY task, if any */
void task_exit(void) __attribute__((noreturn));
void scheduler_start(void);                 /* yield until no other task is READY */
void task_list(void);                       /* print the task table */
int  task_is_active(const char *name);      /* READY or RUNNING task with this name? */

/* Demo tasks (kernel/taskdemo.c). */
void taskdemo_run(void);

/* Implemented in arch/riscv/context.S. */
void context_switch(struct context *old, struct context *next);

#endif /* NANOBOOT_TASK_H */
