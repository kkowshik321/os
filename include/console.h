/* console.h - text console layered on the UART. */
#ifndef NANOBOOT_CONSOLE_H
#define NANOBOOT_CONSOLE_H

void console_putc(char c);          /* '\n' is sent as "\r\n" */
void console_puts(const char *s);
char console_getc(void);            /* blocking read of one byte */

#include "types.h"
void console_put_hex(u64 v, int digits);   /* "0x" + exactly `digits` hex digits */
void console_put_dec(u64 v);               /* unsigned decimal */

#endif /* NANOBOOT_CONSOLE_H */
