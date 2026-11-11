#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 delirioux
"""Xebec - build Xebec's update package (.fwsc) for the FM-1.

    tools/package.py build/xebec_app.bin build/xebec.fwsc [--id FM-1_47932320]
    tools/package.py --extract FM-1_015.fwsc vendor/m-vave/stock

The package is M-VAVE's stock FM-1 package (FM-1_015, from M-VAVE's open
SDK, Apache-2.0) with Xebec in the app slot and nothing else changed:

  flash.bin  0x93000 bytes, as M-VAVE's:
             0x0000  head: boot loader, isd_config, directory    M-VAVE's, unchanged
             0x4000  app area, scrambled with the chip key:
                     entries (app.bin, cfg_tool.bin, VM, PRCT, BTIF, USR)
                                                                 M-VAVE's, two CRCs updated
                     app.bin slot, 0x8DFBC bytes                 Xebec, padded with 0xFF
                     cfg_tool.bin, cfg (EQ table)                M-VAVE's, unchanged
  ota.bin    M-VAVE's OTA loader                                 unchanged

M-VAVE's parts sit in vendor/m-vave/stock, cut out of FM-1_015.fwsc by
--extract (which checks that file's SHA-256 and that it rebuilds byte for
byte from them). The package leaves out the stock package's factory patch
bank (USR) and other extras, so an install never touches the user's patches.
"""
import argparse
import hashlib
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fwsc  # noqa: E402

STOCK_SHA256 = "db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a"   # FM-1_015.fwsc
FLASH_LEN = 0x93000         # flash.bin, the package's part of the flash (PRCT)
SLOT_AT, SLOT_LEN = 0x120, 0x8DFBC   # app.bin in the app area
AREA_LEN = 0x8E25B          # app_area_head's block: entries, app.bin, cfg_tool.bin
CONTENT_LEN = 0x8EDD4       # the scrambled part of the area: the block and the cfg directory
DEFAULT_ID = "FM-1_47932320"   # "47" + XEBEC on a phone keypad, as Xebec's sister firmwares

PART_SHA256 = {             # the parts, as --extract writes them from FM-1_015.fwsc
    "head.bin": "d9f43191777656de01c7bed4f9e9ba2e34e94832c5d2ed02c3fc7ab7a6d9bd67",
    "area_head.bin": "600a42e1eb27a1d8b30e8d7e7e3645576659f7e7279ddabc5ebf0ae98d3a8bdf",
    "area_tail.bin": "d8a73afbdcb19dbe45e4c36dd139b8ec914fffaadc6f7c5942c19ab8163e5d79",
}
OTA_SHA256 = "ef1f2db89b011fa53e71df363f0122de0ea896618c53fb53e8da676322004541"
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STOCK_DIR = os.path.join(HERE, "vendor", "m-vave", "stock")
OTA = os.path.join(HERE, "vendor", "m-vave", "ota.bin")


def entry_set_crc(area, at, dcrc):
    """Set a 32-byte directory entry's data CRC, then its own CRC."""
    struct.pack_into("<H", area, at + 2, dcrc)
    struct.pack_into("<H", area, at, fwsc.crc16(area[at + 2:at + 32]))


def flash_image(app, head, area_head, area_tail):
    """flash.bin from an app and M-VAVE's parts."""
    if len(app) > SLOT_LEN:
        raise SystemExit(f"app is {len(app)} bytes, the slot {SLOT_LEN}")
    area = bytearray(area_head + app + b"\xFF" * (SLOT_LEN - len(app)) + area_tail)
    assert len(area) == CONTENT_LEN
    entry_set_crc(area, 0x20, fwsc.crc16(area[SLOT_AT:SLOT_AT + SLOT_LEN]))   # app.bin
    entry_set_crc(area, 0x00, fwsc.crc16(area[32:AREA_LEN]))                  # app_area_head
    fwsc.sfc(area, 0, len(area))
    flash = bytearray(b"\xFF" * FLASH_LEN)
    flash[0:fwsc.APP_AREA] = head
    flash[fwsc.APP_AREA:fwsc.APP_AREA + len(area)] = area
    return bytes(flash)


def plain_area(flash):
    area = bytearray(flash[fwsc.APP_AREA:fwsc.APP_AREA + CONTENT_LEN])
    fwsc.sfc(area, 0, len(area))
    return area


def stock_parts():
    parts = {}
    for name, sha in PART_SHA256.items():
        parts[name] = open(os.path.join(STOCK_DIR, name), "rb").read()
        if hashlib.sha256(parts[name]).hexdigest() != sha:
            raise SystemExit(f"vendor/m-vave/stock/{name}: not M-VAVE's part (SHA-256)")
    return parts


def ota_loader():
    data = open(OTA, "rb").read()
    if hashlib.sha256(data).hexdigest() != OTA_SHA256:
        raise SystemExit("vendor/m-vave/ota.bin: not M-VAVE's loader (SHA-256)")
    return data


def extract(stock_path, out_dir):
    """Cut M-VAVE's parts out of FM-1_015.fwsc and prove they rebuild it."""
    data = open(stock_path, "rb").read()
    if hashlib.sha256(data).hexdigest() != STOCK_SHA256:
        raise SystemExit(f"{stock_path}: not M-VAVE's FM-1_015.fwsc (SHA-256)")
    ident, chip, files = fwsc.read(stock_path)
    flash = dict((n, d) for _, n, d in files)["flash.bin"]
    if len(flash) != FLASH_LEN or ident != "FM-1_015" or chip != "AC791N":
        raise SystemExit(f"{stock_path}: unexpected layout")
    if set(flash[fwsc.APP_AREA + CONTENT_LEN:]) != {0xFF}:
        raise SystemExit(f"{stock_path}: data past the app area")
    area = plain_area(flash)
    parts = {
        "head.bin": flash[:fwsc.APP_AREA],
        "area_head.bin": bytes(area[:SLOT_AT]),
        "area_tail.bin": bytes(area[SLOT_AT + SLOT_LEN:]),
    }
    rebuilt = flash_image(bytes(area[SLOT_AT:SLOT_AT + SLOT_LEN]), *parts.values())
    if rebuilt != flash:
        raise SystemExit("the parts do not rebuild M-VAVE's flash.bin byte for byte")
    os.makedirs(out_dir, exist_ok=True)
    for name, d in parts.items():
        open(os.path.join(out_dir, name), "wb").write(d)
        sha = hashlib.sha256(d).hexdigest()
        print(f"{name}: {len(d)} bytes, sha256 {sha}{'' if sha == PART_SHA256[name] else '  DIFFERS from PART_SHA256'}")
    if dict((n, f) for _, n, f in files)["ota.bin"] != ota_loader():
        raise SystemExit(f"{stock_path}: its ota.bin differs from vendor/m-vave/ota.bin")
    print("M-VAVE's flash.bin rebuilds byte for byte from these parts and its own app")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("out")
    ap.add_argument("--id", default=DEFAULT_ID)
    ap.add_argument("--extract", action="store_true")
    a = ap.parse_args()
    if a.extract:
        extract(a.src, a.out)
        return
    p = stock_parts()
    flash = flash_image(open(a.src, "rb").read(), p["head.bin"], p["area_head.bin"], p["area_tail.bin"])
    ota = ota_loader()
    fwsc.write(a.out, [(0, "flash.bin", flash), (100, "ota.bin", ota)], a.id)
    print(f"{a.out}: {os.path.getsize(a.out)} bytes, identity {a.id}")


if __name__ == "__main__":
    main()
