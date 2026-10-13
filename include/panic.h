/* panic.h - unrecoverable error reporting. */
#ifndef NANOBOOT_PANIC_H
#define NANOBOOT_PANIC_H

void panic(const char *msg) __attribute__((noreturn));
void panic_halt(void) __attribute__((noreturn));   /* interrupts off, wfi forever */

#endif /* NANOBOOT_PANIC_H */
