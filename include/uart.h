/* uart.h - minimal UART console interface. */
#ifndef NANOBOOT_UART_H
#define NANOBOOT_UART_H

void uart_init(void);
void uart_putc(char c);          /* blocking transmit of one raw byte */
void uart_puts(const char *s);   /* transmit string, '\n' -> "\r\n"   */

int  uart_rx_ready(void);        /* non-zero if a received byte is waiting */
int  uart_getc_nowait(void);     /* returns byte (0..255) or -1 if none     */
char uart_getc(void);            /* blocking receive of one byte            */

#endif /* NANOBOOT_UART_H */
