/* uart.c - polled transmit driver for the NS16550A UART on QEMU virt. */
#include "types.h"
#include "platform.h"
#include "uart.h"

/* MMIO must be volatile so the compiler never caches or elides accesses. */
static inline volatile u8 *uart_reg(usize offset)
{
    return (volatile u8 *)(UART0_BASE + offset);
}

void uart_init(void)
{
    /* QEMU's UART is usable out of reset (baud/FIFO setup is not required). */
}

void uart_putc(char c)
{
    /* Spin until the transmit holding register is empty, then write. */
    while ((*uart_reg(UART_LSR) & UART_LSR_THRE) == 0)
        ;
    *uart_reg(UART_THR) = (u8)c;
}

void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_putc('\r');    /* terminals expect CR+LF */
        uart_putc(*s++);
    }
}

/* ---- Receive side (Module 2) ---- */

int uart_rx_ready(void)
{
    /* LSR bit 0 (Data Ready) is set while a received byte is unread. */
    return (*uart_reg(UART_LSR) & UART_LSR_DR) != 0;
}

int uart_getc_nowait(void)
{
    if (!uart_rx_ready())
        return -1;
    return *uart_reg(UART_RBR);   /* reading RBR also clears Data Ready */
}

char uart_getc(void)
{
    int c;
    while ((c = uart_getc_nowait()) < 0)
        ;
    return (char)c;
}
