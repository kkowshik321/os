#!/usr/bin/env python3
"""
qemu_test.py - drive the NanoBoot serial shell in QEMU and check the output.

Usage:  python3 tests/qemu_test.py [group ...]
Groups: shell trap timer mem tasks   (default: all that exist)
"""
import os
import select
import subprocess
import sys
import time

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
ELF = os.path.join(ROOT, "build", "kernel.elf")
CMD = ["qemu-system-riscv64", "-machine", "virt", "-nographic",
       "-bios", "none", "-kernel", ELF]
PROMPT = "nanoboot> "
BANNER = "NanoBoot: RV64 kernel booted successfully"

results = []


def check(name, ok, detail=""):
    ok = bool(ok)
    results.append(ok)
    print(("PASS  " if ok else "FAIL  ") + name + ("" if ok else "   <-- " + str(detail)))


class Qemu:
    def __init__(self):
        self.p = subprocess.Popen(CMD, stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, bufsize=0)
        self.buf = ""
        self.pos = 0

    def _pump(self, timeout):
        r, _, _ = select.select([self.p.stdout], [], [], timeout)
        if r:
            data = os.read(self.p.stdout.fileno(), 65536)
            self.buf += data.decode("latin-1")
            return bool(data)
        return False

    def expect(self, token, timeout=5.0):
        """Wait for token after the current position; return text up to it."""
        end = time.time() + timeout
        while True:
            i = self.buf.find(token, self.pos)
            if i >= 0:
                out = self.buf[self.pos:i + len(token)]
                self.pos = i + len(token)
                return out
            if time.time() > end:
                return None
            self._pump(0.1)

    def send(self, text):
        self.p.stdin.write(text.encode("latin-1"))
        self.p.stdin.flush()

    def run(self, line, timeout=5.0):
        """Send a command line, return everything up to the next prompt."""
        self.send(line + "\r")
        return self.expect(PROMPT, timeout)

    def quiet(self, secs):
        """Collect output for secs seconds; return what arrived."""
        start = self.pos
        end = time.time() + secs
        while time.time() < end:
            self._pump(0.1)
        self.pos = len(self.buf)
        return self.buf[start:]

    def alive(self):
        return self.p.poll() is None

    def close(self):
        if self.alive():
            self.p.kill()
        self.p.wait()


def boot():
    q = Qemu()
    out = q.expect(PROMPT, 5.0)
    check("boots and shows banner + prompt", out is not None and BANNER in out,
          repr(q.buf))
    return q


# ---------------------------------------------------------------- shell
def test_shell():
    print("== shell ==")
    q = boot()
    out = q.run("help") or ""
    check("help lists commands", "Commands:" in out and "help" in out
          and "echo <text>" in out and "Clear terminal" in out, repr(out))
    out = q.run("echo hello world  again") or ""
    check("echo keeps spaces inside text", "\r\nhello world  again\r\n" in out, repr(out))
    out = q.run("clear") or ""
    check("clear sends ANSI clear/home", "\x1b[2J\x1b[H" in out, repr(out))
    out = q.run("invalid") or ""
    check("unknown command message", "unknown command: invalid\r\n" in out, repr(out))
    out = q.run("") or ""
    check("empty command re-prompts", out == "\r\n" + PROMPT, repr(out))
    out = q.run("   ") or ""
    check("whitespace-only command re-prompts", out == "   \r\n" + PROMPT, repr(out))
    out = q.run("echo abx\x7fc") or ""
    check("backspace (0x7f) erases", "\r\nabc\r\n" in out and "\b \b" in out, repr(out))
    out = q.run("echo abx\x08c") or ""
    check("backspace (0x08) erases", "\r\nabc\r\n" in out, repr(out))
    out = q.run("echo ab\x1b[3~c") or ""
    check("Delete key (ESC[3~) erases", "\r\nac\r\n" in out, repr(out))
    out = q.run("\x7f\x7f\x7fecho ok") or ""
    check("backspace on empty line is safe", "\r\nok\r\n" in out, repr(out))
    out = q.run("echo \x1b[A\x1b[Bx") or ""
    check("arrow-key escape sequences ignored", "\r\nx\r\n" in out, repr(out))
    out = q.run("A" * 300) or ""
    check("overlong line is truncated to 127 bytes (no corruption)",
          ("unknown command: " + "A" * 127 + "\r\n") in out and "A" * 128 not in out
          and out.count("\a") == 300 - 127, "bell count %d" % out.count("\a"))
    out = q.run("echo still alive") or ""
    check("shell works after overflow", "\r\nstill alive\r\n" in out, repr(out))
    q.send("echo crlf\r\n")
    out = q.expect(PROMPT) or ""
    out2 = q.quiet(0.3)
    check("CRLF counts as one Enter", "\r\ncrlf\r\n" in out and out2 == "", repr(out + "|" + out2))
    q.close()


# ---------------------------------------------------------------- trap
def addr_of_first(mnemonic):
    """Address of the first `mnemonic` instruction inside <trap_test>."""
    dis = subprocess.run(["riscv64-unknown-elf-objdump", "-d", ELF],
                         capture_output=True, text=True).stdout
    in_fn = False
    for line in dis.splitlines():
        if line.endswith("<trap_test>:"):
            in_fn = True
        elif in_fn and line.endswith(">:"):
            break
        elif in_fn and line.split("\t")[-1].strip().startswith(mnemonic):
            return int(line.split(":")[0], 16)
    return None


def trap_case(kind, cause_hex, desc, mepc_expect=None, mtval_expect=None):
    import re
    q = boot()
    q.send("traptest " + kind + "\r")
    out = q.expect("System halted.", 5.0) or ""
    check("traptest %s prints trap report" % kind,
          "*** NanoBoot TRAP ***" in out, repr(out))
    check("  mcause = %s (%s)" % (cause_hex, desc),
          "mcause : 0x%016x  (exception: %s)" % (cause_hex, desc) in out, repr(out))
    m = re.search(r"mepc   : 0x([0-9a-f]{16})", out)
    check("  mepc printed" + (" and equals trapping instruction" if mepc_expect else ""),
          bool(m) and (mepc_expect is None or int(m.group(1), 16) == mepc_expect),
          "%s vs %s" % (m and m.group(1), mepc_expect and hex(mepc_expect)))
    m = re.search(r"mtval  : 0x([0-9a-f]{16})", out)
    check("  mtval printed" + (" = 0x%x" % mtval_expect if mtval_expect is not None else ""),
          bool(m) and (mtval_expect is None or int(m.group(1), 16) == mtval_expect),
          m and m.group(1))
    check("  mstatus printed", "mstatus: 0x" in out, repr(out))
    m = re.search(r"\bsp\s+0x([0-9a-f]{16})", out)
    check("  saved sp is 16-byte aligned", bool(m) and int(m.group(1), 16) % 16 == 0,
          m and m.group(1))
    extra = q.quiet(1.0)
    check("  no prompt after trap (halted, not running)", PROMPT not in extra, repr(extra))
    check("  QEMU still alive (CPU parked in wfi)", q.alive())
    q.send("echo hi\r")
    extra = q.quiet(0.5)
    check("  halted CPU ignores input", extra == "", repr(extra))
    q.close()


def test_trap():
    print("== trap ==")
    q = boot()
    out = q.run("traptest bogus") or ""
    check("traptest with bad kind prints usage and keeps running",
          "usage: traptest" in out, repr(out))
    out = q.run("help") or ""
    check("help lists traptest", "traptest [kind]" in out, repr(out))
    q.close()
    trap_case("ebreak", 3, "breakpoint", mepc_expect=addr_of_first("ebreak"))
    trap_case("illegal", 2, "illegal instruction")
    trap_case("load", 5, "load access fault", mtval_expect=0x88000000)
    q = boot()
    out = q.run("echo fresh boot works") or ""
    check("new QEMU session after a halt boots to a normal shell",
          "fresh boot works" in out, repr(out))
    q.close()


# ---------------------------------------------------------------- timer
def get_uptime(q):
    import re
    out = q.run("uptime") or ""
    m = re.search(r"ticks: (\d+)\r\nuptime: (\d+)\.(\d\d) seconds\r\n", out)
    return (int(m.group(1)), int(m.group(2)), int(m.group(3)), out) if m else (None, None, None, out)


def test_timer():
    print("== timer ==")
    q = boot()
    t1, s1, f1, out = get_uptime(q)
    check("uptime prints 'ticks: N' / 'uptime: S.FF seconds'", t1 is not None, repr(out))
    check("uptime seconds field matches ticks/100", t1 is not None and s1 * 100 + f1 == t1,
          (t1, s1, f1))
    time.sleep(1.0)
    t2, _, _, _ = get_uptime(q)
    delta = (t2 or 0) - (t1 or 0)
    check("ticks increase over ~1s at ~100 Hz (got %d)" % delta, 60 <= delta <= 140, delta)
    t3, _, _, _ = get_uptime(q)
    check("ticks increase across back-to-back calls", t3 is not None and t3 >= t2, (t2, t3))

    # Timer IRQs fire ~100x/s; commands and echo must stay intact meanwhile.
    ok = True
    for i in range(40):
        text = "line-%d-" % i + "xyz" * 20
        out = q.run("echo " + text) or ""
        if ("\r\n" + text + "\r\n") not in out:
            ok = False
            break
    check("40 echo commands intact while timer interrupts run", ok, repr(out))
    long_line = "".join(chr(ord('a') + i % 26) for i in range(120))
    q.send("echo " + long_line + "\r")
    out = q.expect(PROMPT) or ""
    check("long typed line is not corrupted by interrupts", ("\r\n" + long_line + "\r\n") in out,
          repr(out))
    t4, _, _, _ = get_uptime(q)
    check("timer keeps running after all that", t4 is not None and t4 > t3, (t3, t4))
    q.close()

    # Fatal exceptions still produce diagnostics with the timer enabled.
    q = boot()
    q.send("traptest\r")
    out = q.expect("System halted.", 5.0) or ""
    check("ebreak diagnostics still work with timer enabled",
          "(exception: breakpoint)" in out and "mepc   : 0x" in out and "mtval  : 0x" in out,
          repr(out))
    check("  mstatus.MPIE (bit 7) set: trapped with interrupts enabled",
          "mstatus: 0x0000000a000018" in out and int(out.split("mstatus: 0x")[1][:16], 16) & 0x80,
          out.split("mstatus: ")[1][:20] if "mstatus: " in out else out)
    extra = q.quiet(1.0)
    check("  no further interrupt output after halt (MIE cleared)", extra.strip() == "", repr(extra))
    q.close()


# ---------------------------------------------------------------- mem
def nm_symbols():
    out = subprocess.run(["riscv64-unknown-elf-nm", ELF], capture_output=True, text=True).stdout
    return {l.split()[2]: int(l.split()[0], 16) for l in out.splitlines() if len(l.split()) == 3}


def parse_mem(out):
    import re
    m = re.search(r"heap: 0x([0-9a-f]{16}) - 0x([0-9a-f]{16})\r\ntotal: (\d+) bytes\r\n"
                  r"used: (\d+) bytes\r\nfree: (\d+) bytes\r\nallocations: (\d+)  failed: (\d+)\r\n", out)
    if not m:
        return None
    h0, h1, tot, used, free, allocs, fails = m.groups()
    return dict(start=int(h0, 16), end=int(h1, 16), total=int(tot), used=int(used),
                free=int(free), allocs=int(allocs), fails=int(fails))


def test_mem():
    print("== mem ==")
    sym = nm_symbols()
    q = boot()
    out = q.run("mem") or ""
    m = parse_mem(out)
    check("mem prints heap range/total/used/free/allocations", m is not None, repr(out))
    m = m or dict(start=0, end=0, total=0, used=0, free=0, allocs=0, fails=0)
    check("heap start/end match linker symbols",
          m["start"] == sym["__heap_start"] and m["end"] == sym["__heap_end"], m)
    check("heap start is 16-byte aligned", m["start"] % 16 == 0, hex(m["start"]))
    check("total == end - start == 256 KiB", m["total"] == m["end"] - m["start"] == 262144, m)
    check("used + free == total, nothing allocated yet", m["used"] + m["free"] == m["total"]
          and m["used"] == 0 and m["allocs"] == 0, m)
    check("heap does not overlap kernel image, .bss or boot stack",
          m["start"] >= sym["__bss_end"] and m["start"] >= sym["__stack_top"]
          and sym["_start"] < sym["__bss_start"] <= sym["__bss_end"] <= sym["__stack_bottom"], m)
    check("heap lies inside RAM", 0x80000000 <= m["start"] and m["end"] <= 0x88000000, m)

    out = q.run("memtest") or ""
    check("memtest PASS", "memtest: PASS" in out, repr(out))
    m2 = parse_mem(q.run("mem") or "") or m
    check("mem after memtest: used grew, allocations counted, failures counted",
          m2["used"] == 1216 and m2["allocs"] == 7 and m2["fails"] == 4 and
          m2["used"] % 16 == 0 and m2["used"] + m2["free"] == m2["total"], m2)
    out = q.run("memtest") or ""
    check("memtest is repeatable", "memtest: PASS" in out, repr(out))
    out = q.run("memtest bogus") or ""
    check("memtest with bad argument prints usage", "usage: memtest" in out, repr(out))
    out = q.run("echo shell fine") or ""
    check("shell still works after allocator failures", "shell fine" in out, repr(out))
    q.close()

    q = boot()
    out = q.run("memtest exhaust", timeout=10) or ""
    check("memtest exhaust: allocator returns NULL when full", "memtest exhaust: PASS" in out, repr(out))
    m3 = parse_mem(q.run("mem") or "")
    check("heap reports 0 bytes free and counts failed allocations",
          m3 is not None and m3["free"] == 0 and m3["used"] == m3["total"] and m3["fails"] >= 2, m3)
    out = q.run("memtest") or ""
    check("memtest on a full heap fails gracefully (no crash)", "memtest: FAIL" in out, repr(out))
    out = q.run("echo shell survives exhaustion") or ""
    check("shell remains functional after heap exhaustion", "shell survives exhaustion" in out, repr(out))
    t1, _, _, _ = get_uptime(q)
    check("timer/uptime still fine after exhaustion", t1 is not None, t1)
    q.close()


# ---------------------------------------------------------------- tasks
def parse_tasks(out):
    import re
    rows = {}
    for m in re.finditer(r"^(\d+)   (\S+)\s+(UNUSED|READY|RUNNING|FINISHED)\s+0x([0-9a-f]{16}) - 0x([0-9a-f]{16})\r?$",
                         out, re.M):
        rows[m.group(2)] = dict(id=int(m.group(1)), state=m.group(3),
                                lo=int(m.group(4), 16), hi=int(m.group(5), 16))
    return rows


def demo_order(out):
    import re
    return [(m.group(1), int(m.group(2)))
            for m in re.finditer(r"task-([ab]): iteration (\d+)", out)]


EXPECTED_ORDER = [(t, i) for i in range(5) for t in "ab"]


def test_tasks():
    import re
    print("== tasks ==")
    sym = nm_symbols()
    q = boot()
    out = q.run("tasks") or ""
    check("tasks prints header", "id  name       state      stack" in out, repr(out))
    rows = parse_tasks(out)
    sh = rows.get("shell")
    check("only the shell exists before taskdemo", list(rows) == ["shell"], rows)
    check("shell is task 0, RUNNING, on the boot stack",
          sh and sh["id"] == 0 and sh["state"] == "RUNNING"
          and sh["lo"] == sym["__stack_bottom"] and sh["hi"] == sym["__stack_top"], sh)

    out = q.run("taskdemo", timeout=10) or ""
    check("taskdemo announces start and finish",
          "started demo tasks" in out and "demo tasks finished" in out, repr(out))
    check("tasks alternate strictly a0 b0 a1 b1 ... a4 b4 (only via yield)",
          demo_order(out) == EXPECTED_ORDER, demo_order(out))
    mid = out.split("task table while I am running:")[1] if "task table while I am running:" in out else ""
    mrows = parse_tasks(mid)
    check("task table seen from inside task-a: shell READY, task-a RUNNING, task-b READY",
          mrows.get("shell", {}).get("state") == "READY"
          and mrows.get("task-a", {}).get("state") == "RUNNING"
          and mrows.get("task-b", {}).get("state") == "READY", mrows)

    rows = parse_tasks(q.run("tasks") or "")
    check("after demo: shell RUNNING, task-a and task-b FINISHED",
          rows.get("shell", {}).get("state") == "RUNNING"
          and rows.get("task-a", {}).get("state") == "FINISHED"
          and rows.get("task-b", {}).get("state") == "FINISHED", rows)
    a, b = rows.get("task-a"), rows.get("task-b")
    if a and b:
        check("task stacks: 8 KiB, 16-byte aligned, distinct, inside .bss",
              all(r["hi"] - r["lo"] == 8192 and r["lo"] % 16 == 0 and
                  sym["__bss_start"] <= r["lo"] and r["hi"] <= sym["__bss_end"] for r in (a, b))
              and (a["hi"] <= b["lo"] or b["hi"] <= a["lo"]), (a, b))
        check("task stacks do not overlap the boot stack or heap",
              all(r["hi"] <= sym["__stack_bottom"] and r["hi"] <= sym["__heap_start"] for r in (a, b)),
              (a, b))
    else:
        check("task stacks present", False, rows)

    # Re-run repeatedly: slot/stack reuse, context-switch stress.
    ok = True
    for n in range(10):
        out = q.run("taskdemo", timeout=10) or ""
        if demo_order(out) != EXPECTED_ORDER or "demo tasks finished" not in out:
            ok = False
            break
    check("taskdemo can be re-run 10 more times (slots/stacks reused, no corruption)", ok,
          demo_order(out))
    rows = parse_tasks(q.run("tasks") or "")
    check("same task ids reused (1 and 2), still only 3 rows",
          rows.get("task-a", {}).get("id") == 1 and rows.get("task-b", {}).get("id") == 2
          and len(rows) == 3, rows)

    # Other subsystems still healthy after lots of task switching.
    t1, _, _, _ = get_uptime(q)
    time.sleep(0.5)
    t2, _, _, _ = get_uptime(q)
    check("timer/uptime keep working after task switching", t1 is not None and t2 > t1, (t1, t2))
    out = q.run("memtest") or ""
    check("memtest still passes", "memtest: PASS" in out, repr(out))
    out = q.run("echo shell ok") or ""
    check("shell responsive", "\r\nshell ok\r\n" in out, repr(out))
    out = q.run("help") or ""
    check("help lists tasks and taskdemo", "tasks " in out and "taskdemo" in out, repr(out))
    q.close()

    # Timer IRQs must not cause switching: scheduler code is never reached from them.
    src = ""
    for f in ("kernel/timer.c", "kernel/trap.c", "arch/riscv/trap.S"):
        src += open(os.path.join(ROOT, f)).read()
    refs = [w for w in ("task_yield", "scheduler_start", "context_switch", "switch_to", "task_exit")
            if w in src]
    check("timer/trap code never references the scheduler (no preemption)", refs == [] , refs)
    tsrc = open(os.path.join(ROOT, "kernel/task.c")).read()
    check("task_yield/task_exit assert they are not in interrupt context",
          tsrc.count("trap_in_handler()") >= 2)

    # A trap inside a task context still produces diagnostics.
    q = boot()
    q.run("taskdemo", timeout=10)
    q.send("traptest\r")
    out = q.expect("System halted.", 5.0) or ""
    check("traptest after tasks have run still reports and halts", "(exception: breakpoint)" in out, repr(out))
    q.close()


# --- GROUPS ---
GROUPS = {"shell": test_shell, "trap": test_trap, "timer": test_timer, "mem": test_mem,
          "tasks": test_tasks}

if __name__ == "__main__":
    if not os.path.exists(ELF):
        sys.exit("build/kernel.elf missing - run make first")
    wanted = sys.argv[1:] or list(GROUPS)
    for g in wanted:
        GROUPS[g]()
    print("\n%d/%d checks passed" % (sum(results), len(results)))
    sys.exit(0 if all(results) else 1)
