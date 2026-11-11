#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 delirioux
"""Xebec - check that the flash driver's RAM code never reaches back into flash.

    tools/check_ram_code.py FM1_EMU IMAGE.elf IMAGE_xip.bin

While the driver erases or programs, the SFC is off and the flash is not
on the bus: one instruction fetched or one constant read from flash in
that window and the FM-1 crashes with the flash half written. So every
instruction in .xb_ram_code (__xb_ram_code_start..__xb_ram_code_end) is
decoded with fm1-emu's decoder and checked:

  - it decodes (no unknown instructions);
  - every direct call and branch lands inside the RAM code;
  - no indirect call or jump and no table branch (a function pointer or a
    switch table could point anywhere, flash included);
  - no immediate that is a flash (XIP) address.

The listing works on the code's flash copy (__xb_ram_code_load), so branch
targets are moved to where the code runs in RAM before they are checked.
"""
import re
import subprocess
import sys

import xbelf

XIP_LO, XIP_HI = 0x02000000, 0x02100000   # the 1 MiB flash as the CPU sees it

LINE = re.compile(r"^([0-9a-f]{8})  (.{18}) (.*) \[([-0-9, ]*)\]\s*(.*)$")


def main():
    emu, elf, xip = sys.argv[1:4]
    syms = xbelf.symbols(elf)
    start, end, load = xbelf.need(syms, "__xb_ram_code_start", "__xb_ram_code_end", "__xb_ram_code_load")
    size = end - start
    if size <= 0:
        sys.exit(f"{elf}: no RAM code (.xb_ram_code is empty)")

    listing = subprocess.run([emu, "--list", xip, f"{load:x}", f"{load + size:x}"],
                             capture_output=True, text=True, check=True).stdout
    problems, count = [], 0
    for raw in listing.splitlines():
        if not raw.strip():
            continue
        m = LINE.match(raw)
        if not m:
            problems.append(f"cannot decode: {raw.strip()}")
            continue
        count += 1
        pc_load = int(m.group(1), 16)
        pc = pc_load - load + start
        syntax = m.group(3).strip()
        values = [int(v) & 0xFFFFFFFF for v in m.group(4).split(",") if v.strip()]
        desc = m.group(5).strip()
        where = f"{pc:#010x} ({syntax})"

        if ("call" in syntax or "goto" in syntax) and "#i" not in syntax:
            problems.append(f"{where}: indirect call or jump")
        if syntax.startswith(("tbb", "tbh", "rti", "rthw", "rtns")):
            problems.append(f"{where}: table branch or exception return")
        t = re.match(r"T ([0-9a-f]+)$", desc)
        if t:
            target = int(t.group(1), 16) - load + start
            if not start <= target < end:
                problems.append(f"{where}: branches to {target:#010x}, outside the RAM code")
        imm = re.match(r"I r\d+ (\d+)$", desc)
        if imm:
            values.append(int(imm.group(1)) & 0xFFFFFFFF)
        for v in values:
            if XIP_LO <= v < XIP_HI:
                problems.append(f"{where}: flash address {v:#010x}")

    print(f"{elf}: RAM code {start:#010x}..{end:#010x} ({size} bytes, {count} instructions)")
    if problems:
        for p in problems:
            print("  " + p)
        sys.exit(f"{elf}: the RAM code reaches into flash ({len(problems)} problems)")
    print(f"{elf}: RAM code stays in RAM")


if __name__ == "__main__":
    main()
