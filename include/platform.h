/* platform.h - all QEMU "virt" machine hardware addresses live here. */
#ifndef NANOBOOT_PLATFORM_H
#define NANOBOOT_PLATFORM_H

/* DRAM: QEMU virt maps RAM at 0x80000000 (default size 128 MiB). */
#define RAM_BASE        0x80000000UL
#define RAM_SIZE        (128UL * 1024 * 1024)

/* NS16550A-compatible UART0. */
#define UART0_BASE      0x10000000UL

/* UART register offsets (byte-wide registers, DLAB = 0). */
#define UART_THR        0x00    /* Transmit Holding Register (write) */
#define UART_RBR        0x00    /* Receive Buffer Register (read, same offset as THR) */
#define UART_LSR        0x05    /* Line Status Register (read)       */

/* LSR bits. */
#define UART_LSR_DR     (1 << 0) /* Data Ready: a received byte is waiting in RBR */
#define UART_LSR_THRE   (1 << 5) /* Transmit Holding Register Empty  */

/*
 * CLINT (Core-Local Interruptor): QEMU virt places it at 0x02000000.
 * It provides the machine timer used for the periodic tick.
 *
 *   CLINT_MTIME      free-running 64-bit counter, increments at TIMEBASE_HZ.
 *                    Read it to learn "now".
 *   CLINT_MTIMECMP0  64-bit compare register for hart 0. The machine timer
 *                    interrupt (mip.MTIP) is pending while mtime >= mtimecmp,
 *                    so writing a new, later deadline both acknowledges the
 *                    interrupt and schedules the next one.
 *
 * (CLINT_MSIP, the software-interrupt register at offset 0, is unused.)
 */
#define CLINT_BASE          0x02000000UL
#define CLINT_MTIMECMP0     (CLINT_BASE + 0x4000UL)   /* hart 0 compare value */
#define CLINT_MTIME         (CLINT_BASE + 0xBFF8UL)   /* current time         */

/* QEMU virt advertises timebase-frequency = 10 MHz in its device tree. */
#define TIMEBASE_HZ         10000000UL

/* Kernel tick: 100 Hz (10 ms) -> 100000 mtime counts between interrupts. */
#define TICK_HZ             100UL
#define TIMER_INTERVAL      (TIMEBASE_HZ / TICK_HZ)

#endif /* NANOBOOT_PLATFORM_H */
