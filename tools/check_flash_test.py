#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 delirioux
"""Xebec - run the flash driver test image in fm1-emu and check it three ways.

    tools/check_flash_test.py FM1_EMU build/test

1. The test's own record (xb_flash_test): every step passed.
2. The emulator's flash command log (FM1_FLASH_TRACE): the only erases and
   programs that reached the chip are the expected ones, all inside the
   first sector of the write window; nothing else that changes the chip.
3. The emulator's instruction trace: each call into the driver's RAM code
   stays in RAM from entry to return (no fetch from flash in between), and
   there are exactly as many of those stays as the test made calls.
"""
import os
import re
import subprocess
import sys

import xbelf

STEPS = 3_000_000
ALL_PASSED = (1 << 13) - 1
EXPECTED_ERASES = 2      # steps 8 and 12
EXPECTED_PROGRAMS = 3    # step 9: two pages; step 11: one page, then the read-back stops it
EXPECTED_RAM_CALLS = 48
FIELDS = ["magic", "done", "passed", "failed", "first_fail", "ram_calls", "id", "safe_lo"]

# Commands that change the chip: page / quad program, sector / block / chip
# erase, status register writes, erase / program of the security registers.
WRITES = {0x02, 0x32, 0x20, 0x52, 0xD8, 0x60, 0xC7, 0x01, 0x31, 0x11, 0x42, 0x44}


def run_record(emu, xip, addr):
    env = dict(os.environ, FM1_FLASH_TRACE="1")
    r = subprocess.run([emu, "--bin", xip, "--entry", "0x02000120", "--steps", str(STEPS),
                        "--quiet", "--dump", f"{addr:x}", str(len(FIELDS))],
                       capture_output=True, text=True, env=env)
    words = []
    for line in r.stdout.splitlines():
        m = re.match(r"\s*0x[0-9a-f]+:\s+((?:[0-9a-f]{8}\s*)+)$", line)
        if m:
            words += [int(w, 16) for w in m.group(1).split()]
    if len(words) < len(FIELDS):
        print(r.stdout[-3000:], r.stderr[-3000:])
        sys.exit("no record dumped by fm1-emu")
    cmds = [(int(c, 16), int(a, 16)) for c, a in
            re.findall(r"flash cmd ([0-9a-f]{2}) addr ([0-9a-f]{6})", r.stderr)]
    return dict(zip(FIELDS, words)), cmds


def run_trace(emu, xip, ram_lo, ram_hi, park):
    """Stays in the RAM code before the test parks, and stray RAM fetches."""
    p = subprocess.Popen([emu, "--bin", xip, "--entry", "0x02000120", "--steps", str(STEPS),
                          "--trace", str(STEPS)], stdout=subprocess.PIPE, text=True)
    stays, inside, stray, parked, n = 0, False, [], False, 0
    try:
        for line in p.stdout:
            m = re.search(r"pc=0x([0-9a-f]{8})", line)
            if not m:
                continue
            n += 1
            pc = int(m.group(1), 16)
            if pc == park:
                parked = True
                break
            now = ram_lo <= pc < ram_hi
            if now and not inside:
                stays += 1
            if not now and 0x01C00000 <= pc < 0x01D00000:
                stray.append(pc)
            inside = now
    finally:
        p.kill()
        p.wait()
    return stays, stray, parked, n


def main():
    emu, out = sys.argv[1], sys.argv[2]
    elf = os.path.join(out, "xebec_flashtest.elf")
    xip = os.path.join(out, "xebec_flashtest_xip.bin")
    syms = xbelf.symbols(elf)
    rec_addr, park, ram_lo, ram_hi, safe_lo = xbelf.need(
        syms, "xb_flash_test", "xb_flash_test_park", "__xb_ram_code_start", "__xb_ram_code_end",
        "__xb_flash_safe_lo")
    problems = []

    # 1. The record.
    rec, cmds = run_record(emu, xip, rec_addr)
    print("record: " + ", ".join(f"{k}={v:#x}" for k, v in rec.items()))
    if rec["magic"] != 0x54464258:
        problems.append(f"the test did not reach its end within {STEPS} steps")
    if rec["passed"] != ALL_PASSED or rec["failed"]:
        problems.append(f"steps failed: mask {rec['failed']:#x}, first {rec['first_fail']}")
    if rec["safe_lo"] != safe_lo:
        problems.append(f"write window {rec['safe_lo']:#x}, the image says {safe_lo:#x}")
    if rec["ram_calls"] != EXPECTED_RAM_CALLS:
        problems.append(f"{rec['ram_calls']} calls into the RAM code, expected {EXPECTED_RAM_CALLS}")

    # 2. What reached the chip.
    writes = [(c, a) for c, a in cmds if c in WRITES]
    erases = [a for c, a in writes if c == 0x20]
    programs = [a for c, a in writes if c == 0x02]
    print(f"flash: {len(cmds)} commands, {len(erases)} sector erases, {len(programs)} page programs")
    for c, a in writes:
        if c not in (0x20, 0x02):
            problems.append(f"command {c:#04x} at {a:#08x} reached the chip")
        elif not safe_lo <= a < safe_lo + 0x1000:
            problems.append(f"command {c:#04x} at {a:#08x}, outside the test sector {safe_lo:#08x}")
    if erases != [safe_lo] * EXPECTED_ERASES:
        problems.append(f"sector erases at {[hex(a) for a in erases]}, expected {EXPECTED_ERASES} at {safe_lo:#x}")
    if len(programs) != EXPECTED_PROGRAMS:
        problems.append(f"{len(programs)} page programs, expected {EXPECTED_PROGRAMS}")

    # 3. The trace.
    stays, stray, parked, n = run_trace(emu, xip, ram_lo, ram_hi, park)
    print(f"trace: {n} instructions, {stays} stays in the RAM code")
    if not parked:
        problems.append(f"the traced run did not park within {STEPS} steps")
    if stays != rec["ram_calls"]:
        problems.append(f"{stays} stays in the RAM code for {rec['ram_calls']} calls: "
                        "the RAM code left RAM in between")
    if stray:
        problems.append(f"code ran from RAM outside the RAM code, first at {stray[0]:#010x}")

    if problems:
        for p in problems:
            print("  " + p)
        sys.exit("flash driver test FAILED")
    print("flash driver test passed: record, chip commands and trace all as expected")


if __name__ == "__main__":
    main()
