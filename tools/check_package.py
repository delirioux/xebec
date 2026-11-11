#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 delirioux
"""Xebec - check Xebec's update package against M-VAVE's stock package.

    tools/check_package.py build/xebec.fwsc build/xebec_app.bin build/xebec.elf [--stock FM-1_015.fwsc]

1. With --stock: M-VAVE's FM-1_015.fwsc cuts into exactly the parts in
   vendor/m-vave/stock, and they rebuild its flash.bin byte for byte.
2. The package holds flash.bin and M-VAVE's ota.bin, nothing else, under
   Xebec's identity.
3. flash.bin is M-VAVE's, byte for byte, except Xebec's app in the app slot
   and the two CRCs that cover it.
4. The flash driver's write window lies past flash.bin, inside the VM area
   of the package's own layout, clear of BTIF, the user patches and key_mac.
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fwsc      # noqa: E402
import package   # noqa: E402
import xbelf     # noqa: E402

WINDOW_LEN = 0x5000      # board/flash.h: XB_FLASH_SAFE_HI - the window start


def entries(area, at, count):
    """Directory entries -> {name: (offset, length)}."""
    out = {}
    for k in range(count):
        e = area[at + 32 * k:at + 32 * (k + 1)]
        if fwsc.crc16(e[2:32]) != struct.unpack_from("<H", e, 0)[0]:
            raise SystemExit(f"directory entry {k}: CRC")
        off, length = struct.unpack_from("<II", e, 4)
        out[bytes(e[16:32]).split(b"\0")[0].split(b"\xff")[0].decode()] = (off, length)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pkg")
    ap.add_argument("app")
    ap.add_argument("elf")
    ap.add_argument("--stock")
    a = ap.parse_args()
    problems = []

    if a.stock:
        package.extract(a.stock, os.path.join(os.path.dirname(os.path.abspath(a.pkg)), "stock-parts"))
        for name in package.PART_SHA256:
            got = open(os.path.join(os.path.dirname(os.path.abspath(a.pkg)), "stock-parts", name), "rb").read()
            if got != open(os.path.join(package.STOCK_DIR, name), "rb").read():
                problems.append(f"vendor/m-vave/stock/{name} differs from what FM-1_015.fwsc holds")

    ident, chip, files = fwsc.read(a.pkg)
    names = [(t, n) for t, n, _ in files]
    print(f"{a.pkg}: identity {ident}, chip {chip}, files {[n for _, n in names]}")
    if ident != package.DEFAULT_ID or chip != "AC791N":
        problems.append(f"identity {ident} / chip {chip}")
    if names != [(0, "flash.bin"), (100, "ota.bin")]:
        problems.append(f"files {names}, expected flash.bin and ota.bin only")
    data = dict((n, d) for _, n, d in files)
    if data.get("ota.bin") != package.ota_loader():
        problems.append("ota.bin is not M-VAVE's loader")

    parts = package.stock_parts()
    flash = data.get("flash.bin", b"")
    app = open(a.app, "rb").read()
    if len(flash) != package.FLASH_LEN:
        problems.append(f"flash.bin is {len(flash):#x} bytes, M-VAVE's is {package.FLASH_LEN:#x}")
    else:
        if flash[:fwsc.APP_AREA] != parts["head.bin"]:
            problems.append("the head (boot loader, isd_config, directory) differs from M-VAVE's")
        area = package.plain_area(flash)
        head = bytearray(parts["area_head.bin"])
        got = bytearray(area[:package.SLOT_AT])
        for at in (0x00, 0x20):          # app_area_head, app.bin: their CRCs change with the app
            head[at:at + 4] = got[at:at + 4] = b"...."
        if got != head:
            problems.append("the app area's directory differs from M-VAVE's beyond the two CRCs")
        if area[package.SLOT_AT + package.SLOT_LEN:] != parts["area_tail.bin"]:
            problems.append("cfg_tool.bin / cfg differ from M-VAVE's")
        slot = area[package.SLOT_AT:package.SLOT_AT + package.SLOT_LEN]
        if slot != app + b"\xFF" * (package.SLOT_LEN - len(app)):
            problems.append("the app slot does not hold build/xebec_app.bin")
        if set(flash[fwsc.APP_AREA + package.CONTENT_LEN:]) != {0xFF}:
            problems.append("flash.bin holds data past the app area")
        print(f"flash.bin: M-VAVE's head and layout, Xebec's app ({len(app)} of {package.SLOT_LEN} bytes)")

        # 4. The write window against the package's own layout.
        regions = entries(area, 0x40, 5)         # cfg_tool.bin, VM, PRCT, BTIF, USR
        key_mac = entries(bytearray(fwsc_head(parts["head.bin"])), 0x80, 1)["key_mac"]
        lo, = xbelf.need(xbelf.symbols(a.elf), "__xb_flash_safe_lo")
        hi = lo + WINDOW_LEN
        vm, prct, btif, usr = regions["VM"], regions["PRCT"], regions["BTIF"], regions["USR"]
        print(f"layout: PRCT {prct[0]:#x}+{prct[1]:#x}, VM {vm[0]:#x}+{vm[1]:#x}, BTIF {btif[0]:#x}, "
              f"USR {usr[0]:#x}+{usr[1]:#x}, key_mac {key_mac[0]:#x}; write window {lo:#x}..{hi:#x}")
        if prct != (0, package.FLASH_LEN):
            problems.append("PRCT does not cover exactly flash.bin")
        if not (vm[0] <= lo and hi <= vm[0] + vm[1]):
            problems.append("the write window is not inside VM")
        for name, (off, length) in (("PRCT", prct), ("BTIF", btif), ("USR", usr), ("key_mac", key_mac)):
            if lo < off + length and off < hi:
                problems.append(f"the write window overlaps {name}")

    if problems:
        for p in problems:
            print("  " + p)
        sys.exit("package check FAILED")
    print("package passed: M-VAVE's package with Xebec in the app slot, write window clear")


def fwsc_head(head):
    """The flash head's first 160 bytes, deciphered (32-byte blocks, key 0xFFFF)."""
    h = bytearray(head[:160])
    for p in range(0, 160, 32):
        fwsc.cipher(h, p, 32, 0xFFFF)
    return h


if __name__ == "__main__":
    main()
