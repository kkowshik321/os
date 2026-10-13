/* console.c - thin text layer over the UART driver. */
#include "console.h"
#include "uart.h"

void console_putc(char c)
{
    if (c == '\n')
        uart_putc('\r');            /* terminals need CR+LF */
    uart_putc(c);
}

void console_puts(const char *s)
{
    while (*s)
        console_putc(*s++);
}

char console_getc(void)
{
    return uart_getc();
}

/* ---- Number printing (there is no printf in NanoBoot) ---- */

void console_put_hex(u64 v, int digits)
{
    static const char hex[] = "0123456789abcdef";
    console_puts("0x");
    for (int i = digits - 1; i >= 0; i--)
        console_putc(hex[(v >> (i * 4)) & 0xf]);
}

void console_put_dec(u64 v)
{
    char tmp[21];                   /* 2^64 has 20 decimal digits */
    int n = 0;
    do {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (n)
        console_putc(tmp[--n]);
}
