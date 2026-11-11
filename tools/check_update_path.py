#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 delirioux
"""Xebec - run the firmware image in fm1-emu with the update keys and check the hand-over.

    tools/check_update_path.py FM1_EMU build/xebec_xip.bin build/xebec.elf work/

Run from the repository root (fm1-emu writes its screen captures to work/).

Three power-ons of the real firmware image:

1. OCT- and OCT+ held: the loader staged and the update record written, in
   flash and in RAM, byte for byte as expected; nothing else reaches the
   chip. (fm1-emu does not model the reset that follows: Xebec stops there.)
2. Only OCT- held: a normal boot, nothing written at all.
3. A normal boot over a staging left behind by an earlier update: the
   staging area erased, nothing else.

The expected record is built here from update.h's description, not from
Xebec's code.
"""
import os
import re
import struct
import subprocess
import sys

import xbelf

RECORD_RAM = 0x01C7FD88
RECORD_LEN = 112
AREA_LEN = 0x5000
RECORD_OFF = 0x4F00
LOADER_LEN = 0x4DE1
WRITES = {0x02, 0x32, 0x20, 0x52, 0xD8, 0x60, 0xC7, 0x01, 0x31, 0x11, 0x42, 0x44}


def crc16(data, c=0):
    for b in data:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021 if c & 0x8000 else c << 1) & 0xFFFF
    return c


def expected_record(area):
    rec = bytearray(RECORD_LEN)
    struct.pack_into("<HHH", rec, 2, 0x5A0D, 0x5A01, 0x5441)
    rec[8:20] = b"ota-FM-1_015"
    struct.pack_into("<I", rec, 72, area)
    struct.pack_into("<H", rec, 0, crc16(rec[2:80]))
    return bytes(rec)


def run(emu, xip, work, name, keys=None, raw_flash=None):
    args = [emu, "--bin", xip, "--entry", "0x02000120", "--ms", "3000", "--steps", "2000000000",
            "--quiet", "--snap", "2900", "--dump", f"{RECORD_RAM:x}", str(RECORD_LEN // 4)]
    if keys:
        args += ["--keys", keys]
    if raw_flash:
        args += ["--raw-flash", raw_flash]
    env = dict(os.environ, FM1_FLASH_TRACE="1")
    r = subprocess.run(args, capture_output=True, text=True, env=env)
    words = []
    for line in r.stdout.splitlines():
        m = re.match(r"\s*0x[0-9a-f]+:\s+((?:[0-9a-f]{8}\s*)+)$", line)
        if m:
            words += [int(w, 16) for w in m.group(1).split()]
    if len(words) < RECORD_LEN // 4:
        print(r.stdout[-3000:], r.stderr[-3000:])
        sys.exit(f"{name}: no RAM dump from fm1-emu")
    snap = os.path.join("work", "lcd_2900.bmp")     # where fm1-emu writes --snap
    if os.path.exists(snap):
        os.replace(snap, os.path.join(work, f"update_{name}.bmp"))
    ram = b"".join(struct.pack("<I", w) for w in words[:RECORD_LEN // 4])
    cmds = [(int(c, 16), int(a, 16)) for c, a in
            re.findall(r"flash cmd ([0-9a-f]{2}) addr ([0-9a-f]{6})", r.stderr)]
    writes = [(c, a) for c, a in cmds if c in WRITES]
    return ram, writes


def main():
    emu, xip, elf, work = sys.argv[1:5]
    os.makedirs(work, exist_ok=True)
    area, = xbelf.need(xbelf.symbols(elf), "__xb_flash_safe_lo")
    sectors = [area + i for i in range(0, AREA_LEN, 0x1000)]
    first = area + 32
    body = [first] + list(range((first & ~0xFF) + 0x100, first + LOADER_LEN, 0x100))
    want_rec = expected_record(area)
    problems = []

    # 1. Both keys held.
    ram, writes = run(emu, xip, work, "keys", keys="0-3000:OCTDN,0-3000:OCTUP")
    erases = [a for c, a in writes if c == 0x20]
    programs = [a for c, a in writes if c == 0x02]
    others = [(c, a) for c, a in writes if c not in (0x20, 0x02)]
    want_programs = body + [area, area + RECORD_OFF]
    print(f"keys held: {len(erases)} sector erases, {len(programs)} page programs; "
          f"RAM record {'as expected' if ram == want_rec else 'DIFFERENT'}")
    if erases != sectors:
        problems.append(f"keys held: sector erases at {[hex(a) for a in erases]}, expected {[hex(a) for a in sectors]}")
    if programs != want_programs:
        problems.append(f"keys held: {len(programs)} page programs, expected {len(want_programs)} "
                        "(the loader from area + 32, its head, then the record at area + 0x4F00)")
    if others:
        problems.append(f"keys held: other commands reached the chip: {others}")
    if ram != want_rec:
        problems.append(f"keys held: RAM record {ram.hex()}, expected {want_rec.hex()}")

    # 2. Only OCT- held.
    ram, writes = run(emu, xip, work, "one_key", keys="0-3000:OCTDN")
    print(f"one key: {len(writes)} commands that change the chip")
    if writes:
        problems.append(f"one key: the chip was changed: {writes[:5]}")
    if any(ram):
        problems.append("one key: a RAM record was written")

    # 3. A staging left behind: head and record in the area, then a normal boot.
    flash = bytearray(b"\xFF" * 0x100000)
    flash[area:area + 32] = bytes(range(32))
    flash[area + RECORD_OFF:area + RECORD_OFF + RECORD_LEN] = want_rec
    raw = os.path.join(work, "leftover_flash.bin")
    open(raw, "wb").write(flash)
    ram, writes = run(emu, xip, work, "cleanup", raw_flash=raw)
    erases = [a for c, a in writes if c == 0x20]
    print(f"leftover: {len(erases)} sector erases, {len(writes) - len(erases)} other changes")
    if erases != sectors or len(writes) != len(erases):
        problems.append(f"leftover: changes {[(hex(c), hex(a)) for c, a in writes]}, expected the {len(sectors)} area erases")

    if problems:
        for p in problems:
            print("  " + p)
        sys.exit("update path FAILED")
    print("update path passed: hand-over with both keys, nothing with one, leftover cleared")


if __name__ == "__main__":
    main()
