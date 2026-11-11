# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 delirioux
"""Xebec - the FM-1 update package format (.fwsc), read and written.

Written for Xebec from the format as M-VAVE's open SDK packager (Package.cs,
MIT, after kagaimiq's jl-misctools) and M-VAVE's own packages show it:

  .fwsc   a UFW container with one marker byte after each of its first 20
          blocks of 47 bytes: together the package identity (FM-1_...)
  UFW     a 64-byte head and 80-byte file entries, each ciphered with the
          JieLi stream cipher (key 0xFFFF); the files follow
  flash   flash.bin, the flash image from offset 0; its app area (from 0x4000)
          is scrambled per 32-byte block with the chip key, as the SFC
          reads it back
"""
import struct

APP_AREA = 0x4000           # flash offset of the app area
CHIP_KEY = 0x980F           # the FM-1's chip key (M-VAVE's packages and isd_config)


def crc16(data, c=0):
    """CRC-16, polynomial 0x1021, start 0."""
    for b in data:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021 if c & 0x8000 else c << 1) & 0xFFFF
    return c


def cipher(buf, start, n, key):
    """JieLi stream cipher, in place (the same call deciphers)."""
    for i in range(start, start + n):
        buf[i] ^= key & 0xFF
        key = ((key << 1) ^ (0x1021 if key & 0x8000 else 0)) & 0xFFFF


def sfc(buf, start, n, key=CHIP_KEY):
    """The SFC's scramble of an app area held in buf: per 32-byte block, the
    key mixed with the block's offset in the area (in place, both ways)."""
    for o in range(start & ~31, start + n, 32):
        cipher(buf, o, min(32, start + n - o), key ^ (o >> 2))


def round_up(n, a):
    return (n + a - 1) // a * a


def unmark(data):
    """.fwsc -> (UFW bytes, identity)."""
    out, marks, i = bytearray(), [], 0
    while i < len(data):
        out.append(data[i])
        if len(out) <= 940 and len(out) % 47 == 0:
            i += 1
            marks.append(data[i])
        i += 1
    ident = "".join(chr(m - k - 1) for k, m in enumerate(marks) if m != 125)
    return bytes(out), ident


def mark(ufw, ident):
    """UFW bytes + identity -> .fwsc."""
    if len(ident) > 20 or any((ord(c) + k + 1) & 0xFF == 125 for k, c in enumerate(ident)):
        raise SystemExit(f"identity {ident!r}: at most 20 characters, none marking as 125")
    out = bytearray()
    for i, b in enumerate(ufw):
        out.append(b)
        if i < 940 and (i + 1) % 47 == 0:
            k = i // 47
            out.append((ord(ident[k]) + k + 1) & 0xFF if k < len(ident) else 125)
    return bytes(out)


def read(path):
    """A .fwsc -> (identity, chip name, [(type, name, bytes)])."""
    ufw, ident = unmark(open(path, "rb").read())
    head = bytearray(ufw[:64])
    cipher(head, 0, 64, 0xFFFF)
    if crc16(head[2:64]) != struct.unpack_from("<H", head, 0)[0]:
        raise SystemExit(f"{path}: UFW head CRC")
    count = struct.unpack_from("<H", head, 8)[0]
    entries = bytearray(ufw[64:64 + 80 * count])
    if crc16(entries) != struct.unpack_from("<H", head, 2)[0]:
        raise SystemExit(f"{path}: UFW entry list CRC")
    files = []
    for i in range(count):
        e = bytearray(entries[80 * i:80 * (i + 1)])
        cipher(e, 0, 80, 0xFFFF)
        typ, _, fcrc = struct.unpack_from("<HHH", e, 0)
        off, length = struct.unpack_from("<II", e, 8)
        name = e[64:80].split(b"\0")[0].decode()
        data = ufw[off:off + length]
        # flash.bin and ota.bin carry a plain CRC; M-VAVE's resource files
        # (patches, config, scripts) are encoded otherwise and not checked here
        if typ in (0, 100) and crc16(data) != fcrc:
            raise SystemExit(f"{path}: {name} CRC")
        files.append((typ, name, data))
    return ident, bytes(head[16:24]).rstrip(b"\0").decode(), files


def write(path, files, ident, chip="AC791N"):
    """[(type, name, bytes)] -> .fwsc at path, the layout of M-VAVE's packager:
    the head and entries in the first KiB, the files after it, 32-byte aligned."""
    offsets, at = [], 1024
    for _, _, data in files:
        offsets.append(at)
        at += round_up(len(data), 32)
    total = at
    ufw = bytearray(b"\xFF" * total)
    ufw[0:64 + 80 * len(files)] = bytes(64 + 80 * len(files))
    for i, ((typ, name, data), off) in enumerate(zip(files, offsets)):
        e = bytearray(80)
        struct.pack_into("<HHH", e, 0, typ, i, crc16(data))
        struct.pack_into("<III", e, 8, off, len(data), round_up(len(data), 32))
        e[64:64 + len(name)] = name.encode()
        cipher(e, 0, 80, 0xFFFF)
        ufw[64 + 80 * i:64 + 80 * (i + 1)] = e
        ufw[off:off + len(data)] = data
    head = bytearray(64)
    struct.pack_into("<IHHI", head, 4, total, len(files), 4, 512)
    head[16:16 + len(chip)] = chip.encode()
    struct.pack_into("<H", head, 2, crc16(ufw[64:64 + 80 * len(files)]))
    struct.pack_into("<H", head, 0, crc16(head[2:64]))
    cipher(head, 0, 64, 0xFFFF)
    ufw[0:64] = head
    open(path, "wb").write(mark(bytes(ufw), ident))
