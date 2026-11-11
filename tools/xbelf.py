# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 delirioux
"""Xebec - read symbol values from a 32-bit little-endian ELF (the linked image)."""
import struct


def symbols(path):
    """{name: value} for every named symbol in the ELF's symbol tables."""
    data = open(path, "rb").read()
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        raise SystemExit(f"{path}: not a 32-bit little-endian ELF")
    shoff, = struct.unpack_from("<I", data, 0x20)
    shentsize, shnum = struct.unpack_from("<HH", data, 0x2E)
    sections = [struct.unpack_from("<IIIIIIIIII", data, shoff + i * shentsize) for i in range(shnum)]
    out = {}
    for sec in sections:
        if sec[1] != 2:                     # SHT_SYMTAB
            continue
        strtab = sections[sec[6]]
        for k in range(sec[5] // 16):
            name_off, value = struct.unpack_from("<II", data, sec[4] + k * 16)
            if not name_off:
                continue
            start = strtab[4] + name_off
            name = data[start:data.index(b"\0", start)].decode()
            out[name] = value
    return out


def need(syms, *names):
    """The values of `names`, failing with a clear message if one is missing."""
    missing = [n for n in names if n not in syms]
    if missing:
        raise SystemExit(f"missing symbols: {', '.join(missing)}")
    return [syms[n] for n in names]
