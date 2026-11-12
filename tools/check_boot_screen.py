#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 delirioux
"""Xebec - check the boot screen fm1-emu captured, pixel for pixel.

    tools/check_boot_screen.py work/lcd_1900.bmp BUILD

Draws the boot screen the firmware should show (the same font, read from
firmware/src/font5x7.h, the same layout and colours as firmware/src/main.c,
the version from firmware/src/version.h) and compares it with the capture.
BUILD is the build code on the screen (the upper-case short commit).
Writes work/expected.bmp next to the capture for a look when it fails.
"""
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W = H = 240

# Layout and colours: keep in step with firmware/src/main.c.
SEA, TEXT, RULE, OK, FAIL = (0x00, 0x80, 0x80), (0xFF, 0xFF, 0xFF), (0x80, 0xC0, 0xC0), \
    (0x40, 0xFF, 0x40), (0xFF, 0x40, 0x40)
TITLE_Y, VERSION_Y, RULE_Y, CHECK_Y0, CHECK_STEP = 20, 64, 90, 100, 20
LABEL_X, RESULT_X, READY_Y = 12, 204, 222
CHECKS = ["DISPLAY", "RAM 64K", "CPU", "FLASH", "USB"]
VALUE_CHECKS = {"CPU": lambda n: f"{n}MHZ"}      # drawn with a measured value instead of OK
# USB shows "--" (white) until a computer configures the FM-1; fm1-emu has no cable here


def rgb565_shown(rgb):
    """A colour as the panel shows it after RGB565, as fm1-emu writes it."""
    r, g, b = rgb
    return ((r >> 3) * 255 // 31, (g >> 2) * 255 // 63, (b >> 3) * 255 // 31)


def load_font():
    src = open(os.path.join(ROOT, "firmware/src/font5x7.h")).read()
    font = {}
    for ch, rows in re.findall(r"\['(.)' - ' '\] = \{([^}]*)\}", src):
        font[ch] = [int(v, 16) for v in rows.split(",")]
    return font


def version():
    src = open(os.path.join(ROOT, "firmware/src/version.h")).read()
    return re.search(r'#define XB_VERSION "([^"]+)"', src).group(1)


class Screen:
    def __init__(self, font):
        self.px = [[rgb565_shown(SEA)] * W for _ in range(H)]
        self.font = font

    def fill(self, x, y, w, h, rgb):
        for yy in range(y, min(y + h, H)):
            for xx in range(x, min(x + w, W)):
                self.px[yy][xx] = rgb565_shown(rgb)

    @staticmethod
    def width(text, scale):
        return (len(text) * 6 - 1) * scale if text else 0

    def text(self, x, y, text, scale, fg, bg):
        blank = [0] * 7
        w = min(self.width(text, scale), W - x)
        h = min(7 * scale, H - y)
        for py in range(h):
            row = py // scale
            for px in range(w):
                cell, col = divmod(px // scale, 6)
                g = self.font.get(text[cell].upper(), blank)
                on = col < 5 and g[row] & (0x10 >> col)
                self.px[y + py][x + px] = rgb565_shown(fg if on else bg)

    def centred(self, y, text, scale, fg):
        self.text((W - self.width(text, scale)) // 2, y, text, scale, fg, SEA)


def expected(build, fail_ram=False, values=None, usb_ok=False):
    s = Screen(load_font())
    values = values or {}
    s.centred(TITLE_Y, "XEBEC", 4, TEXT)
    s.centred(VERSION_Y, f"V{version()}  {build}", 2, TEXT)
    s.fill(LABEL_X, RULE_Y, W - 2 * LABEL_X, 2, RULE)
    for i, label in enumerate(CHECKS):
        y = CHECK_Y0 + i * CHECK_STEP
        s.text(LABEL_X, y, label, 2, TEXT, SEA)
        if fail_ram and label.startswith("RAM"):
            s.text(RESULT_X - 24, y, "FAIL", 2, FAIL, SEA)
        elif label in VALUE_CHECKS:
            text = values.get(label, "")
            s.text(RESULT_X + s.width("OK", 2) - s.width(text, 2), y, text, 2, OK, SEA)
        elif label == "USB" and not usb_ok:
            s.text(RESULT_X + s.width("OK", 2) - s.width("--", 2), y, "--", 2, TEXT, SEA)
        else:
            s.text(RESULT_X, y, "OK", 2, OK, SEA)
    s.centred(READY_Y, "READY", 2, TEXT)
    return s.px


def read_bmp(path):
    data = open(path, "rb").read()
    offset, = struct.unpack_from("<I", data, 10)
    width, height = struct.unpack_from("<ii", data, 18)
    bpp, = struct.unpack_from("<H", data, 28)
    if (width, abs(height), bpp) != (W, H, 24):
        sys.exit(f"{path}: expected a {W}x{H} 24-bit BMP, got {width}x{height} {bpp}-bit")
    row = (W * 3 + 3) & ~3
    px = []
    for y in range(H):
        src_y = H - 1 - y if height > 0 else y      # BMP rows run bottom-up
        base = offset + src_y * row
        px.append([tuple(reversed(data[base + x * 3: base + x * 3 + 3])) for x in range(W)])
    return px


def write_bmp(path, px):
    row = (W * 3 + 3) & ~3
    out = bytearray(b"BM" + struct.pack("<IHHI", 54 + row * H, 0, 0, 54))
    out += struct.pack("<IiiHHIIiiII", 40, W, H, 1, 24, 0, row * H, 0, 0, 0, 0)
    for y in range(H - 1, -1, -1):
        line = b"".join(bytes(reversed(p)) for p in px[y])
        out += line + bytes(row - len(line))
    open(path, "wb").write(out)


def find_value(got, label, lo, hi):
    """The number drawn on a value line, if one renders exactly as on screen."""
    i = CHECKS.index(label)
    y0 = CHECK_Y0 + i * CHECK_STEP
    rows = range(y0, y0 + 14)
    for n in range(lo, hi + 1):
        s = Screen(load_font())
        s.fill(0, y0, W, 14, SEA)
        text = VALUE_CHECKS[label](n)
        s.text(LABEL_X, y0, label, 2, TEXT, SEA)
        s.text(RESULT_X + s.width("OK", 2) - s.width(text, 2), y0, text, 2, OK, SEA)
        if all(abs(a - b) <= 2 for y in rows for x in range(W) for a, b in zip(got[y][x], s.px[y][x])):
            return n
    return None


def main():
    path, build = sys.argv[1], sys.argv[2]
    want_mhz = int(sys.argv[3]) if len(sys.argv) > 3 else None
    got = read_bmp(path)
    mhz = find_value(got, "CPU", 24, 1000)
    if mhz is None:
        sys.exit(f"{path}: the CPU line shows no clock between 24 and 1000 MHz")
    print(f"{path}: CPU line shows {mhz} MHz")
    if want_mhz is not None and mhz != want_mhz:
        sys.exit(f"{path}: CPU {mhz} MHz, expected {want_mhz}")
    want = expected(build, values={"CPU": VALUE_CHECKS["CPU"](mhz)})
    write_bmp(os.path.join(os.path.dirname(path) or ".", "expected.bmp"), want)

    wrong = [(x, y) for y in range(H) for x in range(W)
             if any(abs(a - b) > 2 for a, b in zip(got[y][x], want[y][x]))]
    if wrong:
        x, y = wrong[0]
        sys.exit(f"{path}: {len(wrong)} of {W * H} pixels differ from the expected boot screen; "
                 f"first at ({x},{y}): {got[y][x]}, expected {want[y][x]}")
    print(f"{path}: boot screen matches, pixel for pixel (build {build})")


if __name__ == "__main__":
    main()
