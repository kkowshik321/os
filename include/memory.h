/*
 * memory.h - kernel bump allocator.
 *
 * The heap is the linker-reserved region [__heap_start, __heap_end) defined
 * in linker.ld (256 KiB, placed after .bss and the boot stack). Allocation
 * just advances a pointer; there is no free(). Memory is NOT zeroed.
 */
#ifndef NANOBOOT_MEMORY_H
#define NANOBOOT_MEMORY_H

#include "types.h"

#define KMALLOC_ALIGN 16        /* every block is aligned (and padded) to 16 bytes */

void  memory_init(void);
void *kmalloc(size_t size);     /* NULL if size == 0 or the heap is exhausted */

usize memory_heap_start(void);
usize memory_heap_end(void);
usize memory_total(void);       /* heap size in bytes                          */
usize memory_used(void);        /* bytes handed out, including alignment pad   */
usize memory_free(void);        /* bytes still available                       */
usize memory_alloc_count(void); /* successful kmalloc calls                    */
usize memory_fail_count(void);  /* kmalloc calls that returned NULL (size > 0) */

/* Shell self-tests. Return 0 on success, non-zero on failure (details printed). */
int memory_selftest(void);
int memory_exhaust_test(void);  /* destructive: consumes the whole heap */

#endif /* NANOBOOT_MEMORY_H */
