/* linker.ld - memory layout for NanoBoot on QEMU virt (RAM at 0x80000000). */

OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

STACK_SIZE = 16K;
HEAP_SIZE  = 256K;      /* kernel bump-allocator heap (Module 5) */

SECTIONS
{
    .text : ALIGN(4K) {
        *(.text.boot)           /* _start must be first: it is the entry address */
        *(.text .text.*)
    } > RAM

    .rodata : ALIGN(8) {
        *(.rodata .rodata.* .srodata .srodata.*)
    } > RAM

    .data : ALIGN(8) {
        *(.data .data.*)
        . = ALIGN(8);
        /* Small-data region; gp points 0x800 into it for relaxed accesses. */
        __global_pointer$ = . + 0x800;
        *(.sdata .sdata.*)
    } > RAM

    .bss (NOLOAD) : ALIGN(8) {
        __bss_start = .;
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    } > RAM

    /* Boot stack lives after .bss and grows downward from __stack_top. */
    .stack (NOLOAD) : ALIGN(16) {
        __stack_bottom = .;
        . += STACK_SIZE;
        __stack_top = .;
    } > RAM

    /*
     * Kernel heap: a linker-reserved region AFTER everything else, so by
     * construction it cannot overlap .text/.rodata/.data/.bss (which includes
     * the static task stacks) or the boot stack. 4 KiB aligned.
     */
    .heap (NOLOAD) : ALIGN(4K) {
        __heap_start = .;
        . += HEAP_SIZE;
        __heap_end = .;
    } > RAM

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.riscv.attributes) }
}

/* Link-time sanity checks (the link fails if any of these is false). */
ASSERT((__stack_top & 15) == 0,           "boot stack top must be 16-byte aligned")
ASSERT(__heap_start >= __stack_top,       "heap overlaps boot stack")
ASSERT(__heap_start >= __bss_end,         "heap overlaps .bss")
ASSERT((__heap_start & 15) == 0,          "heap start must be 16-byte aligned")
ASSERT(__heap_end <= ORIGIN(RAM) + LENGTH(RAM), "heap exceeds RAM")
