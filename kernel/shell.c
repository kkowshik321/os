/*
 * shell.c - interactive serial shell.
 *
 * Input is collected into a fixed 128-byte buffer (no dynamic allocation).
 * At most SHELL_LINE_MAX-1 characters are accepted so there is always room
 * for the terminating NUL; extra characters are rejected with a bell.
 *
 * Commands live in a table (name, usage, description, handler), and `help`
 * prints from that table, so adding a command later is a one-line change.
 */
#include "types.h"
#include "console.h"
#include "string.h"
#include "task.h"
#include "shell.h"
#include "memory.h"
#include "platform.h"
#include "timer.h"
#include "trap.h"

#define SHELL_LINE_MAX 128

typedef void (*command_fn)(const char *args);

struct command {
    const char *name;
    const char *usage;
    const char *description;
    command_fn  handler;
};

static void cmd_help(const char *args);
static void cmd_echo(const char *args);
static void cmd_clear(const char *args);
static void cmd_traptest(const char *args);
static void cmd_uptime(const char *args);
static void cmd_mem(const char *args);
static void cmd_tasks(const char *args);
static void cmd_taskdemo(const char *args);
static void cmd_memtest(const char *args);

static const struct command commands[] = {
    { "help",  "help",        "Show this help",  cmd_help  },
    { "echo",  "echo <text>", "Print text",      cmd_echo  },
    { "clear", "clear",       "Clear terminal",  cmd_clear },
    { "uptime", "uptime",     "Show timer ticks and uptime", cmd_uptime },
    { "mem",    "mem",        "Show heap usage", cmd_mem },
    { "memtest", "memtest [exhaust]", "Allocator self-test (exhaust fills heap)", cmd_memtest },
    { "tasks",  "tasks",      "List tasks and their stacks", cmd_tasks },
    { "taskdemo", "taskdemo", "Run two cooperative demo tasks", cmd_taskdemo },
    { "traptest", "traptest [kind]", "Trap on purpose (ebreak|illegal|load), then halt", cmd_traptest },
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

/* ---- Line input ---- */

/*
 * Read one line into buf (size bytes including NUL). Returns its length.
 *
 * Handles: CR or LF = Enter (a LF right after a CR is swallowed so CRLF
 * terminals do not produce an extra empty command); 0x08 and 0x7F erase the
 * last character (never below an empty line); ESC [ 3 ~ (the Delete key) also
 * erases; other escape sequences (arrows, etc.) are ignored.
 */
static usize shell_readline(char *buf, usize size)
{
    static int prev_was_cr;
    usize len = 0;
    int esc = 0;            /* 0 = normal, 1 = after ESC, 2 = inside ESC [ ... */
    u32 param = 0;

    for (;;) {
        char c = console_getc();

        if (esc == 1) {                 /* byte after ESC */
            esc = (c == '[') ? 2 : 0;
            param = 0;
            continue;
        }
        if (esc == 2) {                 /* CSI parameters, then a final byte */
            if (c >= '0' && c <= '9') {
                if (param < 1000)
                    param = param * 10 + (u32)(c - '0');
                continue;
            }
            if (c == ';')
                continue;
            esc = 0;
            if (c == '~' && param == 3 && len > 0) {    /* Delete key */
                len--;
                console_puts("\b \b");
            }
            continue;
        }

        if (c == '\n' && prev_was_cr) { /* second half of CRLF */
            prev_was_cr = 0;
            continue;
        }
        prev_was_cr = (c == '\r');

        if (c == '\r' || c == '\n') {
            console_putc('\n');
            buf[len] = '\0';
            return len;
        }
        if (c == 0x1b) {
            esc = 1;
        } else if (c == 0x08 || c == 0x7f) {
            if (len > 0) {
                len--;
                console_puts("\b \b");    /* move back, blank, move back */
            }
        } else if (c >= 0x20 && c < 0x7f) {
            if (len < size - 1) {
                buf[len++] = c;
                console_putc(c);          /* echo */
            } else {
                console_putc('\a');       /* buffer full: reject, ring bell */
            }
        }
        /* all other control bytes are ignored */
    }
}

/* ---- Command dispatch ---- */

static const char *skip_spaces(const char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}

static void shell_execute(char *line)
{
    char *cmd = (char *)skip_spaces(line);
    if (*cmd == '\0')
        return;                         /* empty command: just re-prompt */

    char *end = cmd;
    while (*end && *end != ' ' && *end != '\t')
        end++;
    const char *args = end;
    if (*end) {
        *end = '\0';                    /* terminate the command word */
        args = skip_spaces(end + 1);
    }

    for (usize i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(cmd, commands[i].name) == 0) {
            commands[i].handler(args);
            return;
        }
    }
    console_puts("unknown command: ");
    console_puts(cmd);
    console_putc('\n');
}

void shell_run(void)
{
    char line[SHELL_LINE_MAX];

    for (;;) {
        console_puts("nanoboot> ");
        shell_readline(line, sizeof(line));
        shell_execute(line);
    }
}

/* ---- Commands ---- */

static void cmd_help(const char *args)
{
    (void)args;
    console_puts("Commands:\n");
    for (usize i = 0; i < NUM_COMMANDS; i++) {
        console_puts("  ");
        console_puts(commands[i].usage);
        usize pad = strlen(commands[i].usage);
        do {
            console_putc(' ');          /* pad to column 18, at least 1 space */
        } while (++pad < 18);
        console_puts(commands[i].description);
        console_putc('\n');
    }
}

static void cmd_echo(const char *args)
{
    console_puts(args);
    console_putc('\n');
}

static void cmd_clear(const char *args)
{
    (void)args;
    console_puts("\x1b[2J\x1b[H");        /* ANSI: erase screen, cursor home */
}

static void cmd_traptest(const char *args)
{
    /* Every test trap is unrecoverable: diagnostics print, then the CPU halts. */
    if (trap_test(args) < 0)
        console_puts("usage: traptest [ebreak|illegal|load]\n");
}

static void cmd_uptime(const char *args)
{
    (void)args;
    u64 t = timer_ticks();
    u64 frac = t % TICK_HZ;                 /* hundredths of a second */

    console_puts("ticks: ");
    console_put_dec(t);
    console_puts("\nuptime: ");
    console_put_dec(t / TICK_HZ);
    console_putc('.');
    if (frac < 10)
        console_putc('0');
    console_put_dec(frac);
    console_puts(" seconds\n");
}

static void print_bytes_line(const char *label, usize n)
{
    console_puts(label);
    console_put_dec(n);
    console_puts(" bytes\n");
}

static void cmd_mem(const char *args)
{
    (void)args;
    console_puts("heap: ");
    console_put_hex(memory_heap_start(), 16);
    console_puts(" - ");
    console_put_hex(memory_heap_end(), 16);
    console_putc('\n');
    print_bytes_line("total: ", memory_total());
    print_bytes_line("used: ", memory_used());
    print_bytes_line("free: ", memory_free());
    console_puts("allocations: ");
    console_put_dec(memory_alloc_count());
    console_puts("  failed: ");
    console_put_dec(memory_fail_count());
    console_putc('\n');
}

static void cmd_memtest(const char *args)
{
    if (strcmp(args, "exhaust") == 0)
        memory_exhaust_test();
    else if (*args == '\0')
        memory_selftest();
    else
        console_puts("usage: memtest [exhaust]\n");
}

static void cmd_tasks(const char *args)
{
    (void)args;
    task_list();
}

static void cmd_taskdemo(const char *args)
{
    (void)args;
    taskdemo_run();
}
