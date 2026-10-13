/*
 * memory.c - bump allocator over the linker-defined heap.
 *
 *   heap_start                                                   heap_end
 *   |#### allocated ####|<- heap_next          free             |
 *
 * Invariants: heap_start <= heap_next <= heap_end, and heap_next is always
 * a multiple of KMALLOC_ALIGN (start is aligned and every request is rounded
 * up to a multiple of it), so every returned pointer is 16-byte aligned.
 */
#include "memory.h"
#include "console.h"
#include "panic.h"
#include "platform.h"

extern char __heap_start[];     /* from linker.ld */
extern char __heap_end[];
extern char __stack_top[];

static usize heap_start;
static usize heap_end;
static usize heap_next;
static usize alloc_count;
static usize fail_count;

void memory_init(void)
{
    heap_start = (usize)__heap_start;
    heap_end   = (usize)__heap_end;
    heap_next  = heap_start;
    alloc_count = 0;
    fail_count = 0;

    if ((heap_start & (KMALLOC_ALIGN - 1)) != 0 || heap_end <= heap_start)
        panic("memory_init: invalid heap bounds");
}

void *kmalloc(size_t size)
{
    if (size == 0)
        return NULL;

    /* Round up to a multiple of KMALLOC_ALIGN without overflowing size_t. */
    if (size > SIZE_MAX - (KMALLOC_ALIGN - 1)) {
        fail_count++;
        return NULL;
    }
    size_t need = (size + (KMALLOC_ALIGN - 1)) & ~(size_t)(KMALLOC_ALIGN - 1);

    /* Compare against remaining space (heap_end - heap_next cannot underflow
     * thanks to the invariant); never compute heap_next + need. */
    if (need > heap_end - heap_next) {
        fail_count++;
        return NULL;
    }

    void *p = (void *)heap_next;
    heap_next += need;
    alloc_count++;
    return p;
}

usize memory_heap_start(void)  { return heap_start; }
usize memory_heap_end(void)    { return heap_end; }
usize memory_total(void)       { return heap_end - heap_start; }
usize memory_used(void)        { return heap_next - heap_start; }
usize memory_free(void)        { return heap_end - heap_next; }
usize memory_alloc_count(void) { return alloc_count; }
usize memory_fail_count(void)  { return fail_count; }

/* ---------------------------------------------------------------- tests */

static int test_fail(const char *why)
{
    console_puts("memtest: FAIL - ");
    console_puts(why);
    console_putc('\n');
    return 1;
}

int memory_selftest(void)
{
    static const size_t sizes[] = { 1, 15, 16, 17, 100, 1000 };
    enum { N = sizeof(sizes) / sizeof(sizes[0]) };
    u8 *blocks[N];
    usize used0 = memory_used();
    usize count0 = memory_alloc_count();
    usize expect_used = 0;

    /* The heap must lie entirely after the boot stack and inside RAM. */
    if (heap_start < (usize)__stack_top)
        return test_fail("heap overlaps boot stack");
    if (heap_end > RAM_BASE + RAM_SIZE)
        return test_fail("heap extends past RAM");

    for (int i = 0; i < N; i++) {
        blocks[i] = kmalloc(sizes[i]);
        if (!blocks[i])
            return test_fail("kmalloc returned NULL unexpectedly");
        if (((usize)blocks[i] & (KMALLOC_ALIGN - 1)) != 0)
            return test_fail("block not 16-byte aligned");
        if ((usize)blocks[i] < heap_start || (usize)blocks[i] + sizes[i] > heap_end)
            return test_fail("block outside heap");
        if (i > 0 && (usize)blocks[i] < (usize)blocks[i - 1] + sizes[i - 1])
            return test_fail("blocks overlap");
        expect_used += (sizes[i] + KMALLOC_ALIGN - 1) & ~(usize)(KMALLOC_ALIGN - 1);
    }

    /* Fill each block with its own pattern, touching exactly `size` bytes. */
    for (int i = 0; i < N; i++)
        for (size_t j = 0; j < sizes[i]; j++)
            blocks[i][j] = (u8)(0xA0 + i);
    /* Verify afterwards: a neighbour's overrun would show up here. */
    for (int i = 0; i < N; i++)
        for (size_t j = 0; j < sizes[i]; j++)
            if (blocks[i][j] != (u8)(0xA0 + i))
                return test_fail("block contents corrupted");

    if (memory_used() - used0 != expect_used)
        return test_fail("used-bytes accounting mismatch");
    if (memory_alloc_count() - count0 != N)
        return test_fail("allocation count mismatch");

    /* Failure paths must return NULL and leave the heap untouched. */
    usize used1 = memory_used();
    if (kmalloc(0) != NULL)
        return test_fail("kmalloc(0) should be NULL");
    if (kmalloc(memory_free() + 1) != NULL)
        return test_fail("oversize request should be NULL");
    if (kmalloc(memory_total() * 2) != NULL)
        return test_fail("2x heap request should be NULL");
    if (kmalloc(SIZE_MAX) != NULL)
        return test_fail("SIZE_MAX request should be NULL");
    if (kmalloc(SIZE_MAX - 14) != NULL)     /* rounding would overflow */
        return test_fail("overflowing round-up should be NULL");
    if (memory_used() != used1)
        return test_fail("failed requests changed heap state");
    if (!kmalloc(16))
        return test_fail("allocator unusable after failed requests");

    console_puts("memtest: PASS (");
    console_put_dec(N);
    console_puts(" blocks aligned, patterns intact, failures return NULL)\n");
    return 0;
}

int memory_exhaust_test(void)
{
    usize n = 0;

    console_puts("memtest exhaust: allocating until the heap is full...\n");
    while (kmalloc(1024))
        n++;
    while (kmalloc(16))     /* mop up the tail */
        n++;

    if (memory_free() != 0)
        return test_fail("heap not completely full after exhaustion");
    if (kmalloc(1) != NULL)
        return test_fail("kmalloc succeeded on a full heap");

    console_puts("memtest exhaust: PASS (");
    console_put_dec(n);
    console_puts(" blocks, then NULL; heap is now full)\n");
    return 0;
}
