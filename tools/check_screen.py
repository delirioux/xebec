#!/usr/bin/env python3
"""Xebec - check a screen capture from fm1-emu (a 240 x 240, 24-bit BMP).

    tools/check_screen.py work/lcd.bmp RRGGBB

Passes when every pixel is the expected colour, give or take the
precision lost in RGB565 (5/6/5 bits per channel).
"""
import struct
import sys


def rgb565_round(rgb):
    """The colour as RGB565 shows it, back in 8-bit channels."""
    r, g, b = rgb
    return ((r >> 3) * 255 // 31, (g >> 2) * 255 // 63, (b >> 3) * 255 // 31)


def main():
    path, hex_colour = sys.argv[1], sys.argv[2]
    want = rgb565_round(tuple(int(hex_colour[i:i + 2], 16) for i in (0, 2, 4)))

    data = open(path, "rb").read()
    offset, = struct.unpack_from("<I", data, 10)
    width, height = struct.unpack_from("<ii", data, 18)
    bpp, = struct.unpack_from("<H", data, 28)
    if (width, abs(height), bpp) != (240, 240, 24):
        sys.exit(f"{path}: expected a 240x240 24-bit BMP, got {width}x{height} {bpp}-bit")

    row = (width * 3 + 3) & ~3
    wrong = 0
    first = None
    for y in range(240):
        for x in range(240):
            b, g, r = data[offset + y * row + x * 3: offset + y * row + x * 3 + 3]
            if any(abs(a - c) > 2 for a, c in zip((r, g, b), want)):
                wrong += 1
                if first is None:
                    first = (x, y, (r, g, b))

    if wrong:
        x, y, got = first
        sys.exit(f"{path}: {wrong} of 57600 pixels wrong; first at ({x},{y}): "
                 f"{got}, expected {want}")
    print(f"{path}: all 57600 pixels are {want}")


if __name__ == "__main__":
    main()
