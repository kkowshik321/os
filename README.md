# NanoBoot — Boot-to-Shell Mini-Kernel

A small educational bare-metal RISC-V (RV64) kernel for QEMU `virt`. C plus a
little assembly, no libc, no OpenSBI: it boots in machine mode straight to a
serial shell with traps, a timer, a heap allocator and cooperative tasks.

## Prerequisites

| Tool | Debian/Ubuntu package |
|---|---|
| `riscv64-unknown-elf-gcc` + binutils | `gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf` |
| `qemu-system-riscv64` | `qemu-system-misc` |
| `make`, `python3` (tests only) | `make python3` |
| GDB (debugging only) | `gdb-multiarch` (or `riscv64-unknown-elf-gdb`) |

    sudo apt install gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf \
                     qemu-system-misc make python3 gdb-multiarch

Developed and tested with GCC 13.2.0 and QEMU 8.2.2.

## Build and run

    make            # build/kernel.elf
    make run        # same as the command below; exit QEMU with Ctrl-A then X
    make clean
    make test       # automated QEMU tests (see "Tests")

    qemu-system-riscv64 -machine virt -nographic -bios none -kernel build/kernel.elf

## Debugging with GDB

    make debug                       # QEMU paused, GDB server on tcp::1234
    # equivalent: qemu-system-riscv64 ... -kernel build/kernel.elf -s -S

    # in a second terminal:
    gdb-multiarch build/kernel.elf \
        -ex 'set architecture riscv:rv64' -ex 'target remote :1234' \
        -ex 'break kernel_main' -ex 'continue'

Execution starts in QEMU's reset ROM at `0x1000` and jumps to `_start` at
`0x80000000`. Useful breakpoints: `kernel_main`, `trap_handler`, `context_switch`.

## Shell commands

| Command | Description |
|---|---|
| `help` | List commands |
| `echo <text>` | Print text |
| `clear` | ANSI clear screen + cursor home |
| `uptime` | Timer ticks and elapsed time (100 Hz) |
| `mem` | Heap range, total/used/free bytes, allocation counts |
| `memtest [exhaust]` | Allocator self-test; `exhaust` fills the whole heap |
| `tasks` | Task table: id, name, state, stack range |
| `taskdemo` | Run two cooperative demo tasks to completion |
| `traptest [ebreak\|illegal\|load]` | Deliberate trap; prints diagnostics, then halts the CPU |

Line editing: Backspace (0x08/0x7F) and Delete (`ESC [ 3 ~`) erase; Enter is
CR or LF; 127 characters max per line (extra input rings the bell and is dropped).

## Architecture

    arch/riscv/boot.S      entry: park other harts, gp, sp, clear .bss, call kernel_main
    arch/riscv/trap.S      trap_entry: save 31 GPRs + CSRs into a 288-byte trap_frame, call C, mret
    arch/riscv/context.S   context_switch (ra, sp, s0-s11) and task_bootstrap
    kernel/main.c          init order: uart -> trap -> memory -> task -> timer -> shell
    kernel/uart.c          NS16550A polled TX/RX (volatile MMIO)
    kernel/console.c       text layer: \n -> \r\n, hex/decimal printing
    kernel/string.c        strlen/strcmp/strncmp/memcpy/memset (local, no libc)
    kernel/shell.c         line editor + table-driven commands
    kernel/trap.c          mcause decode, diagnostics, timer-IRQ dispatch
    kernel/panic.c         panic(): interrupts off, wfi loop
    kernel/timer.c         CLINT tick at 100 Hz, volatile tick counter
    kernel/memory.c        bump allocator over the linker-reserved heap
    kernel/task.c          task table, round-robin cooperative scheduler
    kernel/taskdemo.c      task-a / task-b demo
    include/platform.h     ALL hardware addresses (RAM, UART, CLINT) and timing constants
    include/riscv.h        CSR helpers, mstatus/mie/mcause bit definitions
    linker.ld              memory map, symbols, link-time ASSERTs

### Memory map (QEMU virt RAM @ 0x80000000, 128 MiB)

    .text .rodata .data  .bss (incl. task stacks)  .stack (16 KiB)  .heap (256 KiB)
    ^ 0x80000000                                                     ^ after the stack

The heap is last, so it cannot overlap anything else; `linker.ld` `ASSERT`s
check this at link time. Each task has an 8 KiB static stack in `.bss` with a
canary word at its bottom, checked on every yield/exit.

### Key design rules
- Traps: `mtvec` is installed before any interrupt source is enabled. Only the
  machine timer interrupt is recoverable; every other trap prints a report and halts.
- Timer: the handler increments a tick and re-arms `mtimecmp`; it never prints
  and never touches tasks.
- Tasks: cooperative only. Switching happens only inside `task_yield()` /
  `task_exit()`, which panic if called from the trap handler. A task entry
  that returns lands in `task_bootstrap`, which calls `task_exit()`.
- ABI: all stacks are 16-byte aligned; the trap frame is a multiple of 16 bytes.

## Tests

`make test` boots QEMU repeatedly and drives the serial shell from Python
(`tests/qemu_test.py`); groups can be run alone: `python3 tests/qemu_test.py shell trap timer mem tasks`.

Manual checklist: banner appears; `help`, `echo a b`, `clear`, bad command,
empty line, Backspace/Delete; `uptime` twice (ticks grow); `mem`, `memtest`;
`taskdemo` then `tasks`; `traptest` last (CPU halts; restart QEMU to continue).

## Known limitations
- UART input is polled (no UART interrupts); the shell spins on the host CPU.
- Machine mode only, hart 0 only; no paging, user mode, filesystem or SMP.
- Traps run on the current stack (no dedicated trap stack), so an overflow
  during a trap cannot be diagnosed. Only ebreak, illegal instruction and load
  access fault were triggered in tests; other causes are decoded but untested.
- Heap: no `free`; memory is not zeroed; `memtest` consumes heap space, and `memtest exhaust` consumes the whole heap.
- Tasks: no blocking/sleep primitives; a task that never yields hangs the shell
  (the timer keeps counting). `taskdemo` runs to completion, so `tasks` run from
  the shell afterwards shows FINISHED; READY/RUNNING states are visible in the
  table task-a prints mid-run. Stack canaries only catch overflow at yield/exit.
- Line editor: ASCII only, no cursor movement or history.
