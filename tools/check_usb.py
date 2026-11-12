#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 delirioux
"""Xebec - plug the firmware into fm1-emu's USB host and check it.

    tools/check_usb.py FM1_LIVE build/xebec_xip.bin BUILD

fm1-live, headless with a USB cable: the host enumerates the FM-1, sends the
identity question at 2 s, and the screen is taken at 3 s. Checked: it
enumerates as "FM-1 Xebec" (1209:0001), answers FM-1_47932320, and the boot
screen's USB line shows OK.
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_boot_screen as screen   # noqa: E402

QUERY = "F0003245000000407FF7"
PRODUCT = "FM-1_47932320"


def unpack7(b):
    out, acc, n = [], 0, 0
    for x in b:
        acc |= (x & 0x7F) << n
        n += 7
        if n >= 8:
            out.append(acc & 0xFF)
            acc >>= 8
            n -= 8
    return out


def identity(msg):
    """The identity in a reply frame, or None."""
    if len(msg) < 10 or msg[0] != 0xF0 or msg[-1] != 0xF7:
        return None
    raw = unpack7(msg[1:-1])
    if len(raw) < 7 or raw[:3] != [0, 0x59, 0x11] or raw[3] | raw[4] << 8 | raw[5] << 16 != len(raw) - 7:
        return None
    if sum(raw[6:]) & 0xFF != 0xFF:
        return None
    return bytes(raw[6:-1]).split(b"\0")[0].decode()


def read_ppm(path):
    data = open(path, "rb").read()
    head = data.split(b"\n", 3)
    w, h = map(int, head[1].split())
    px = head[3]
    return [[tuple(px[(y * w + x) * 3:(y * w + x) * 3 + 3]) for x in range(w)] for y in range(h)]


def main():
    live, xip, build = sys.argv[1:4]
    env = dict(os.environ, FM1_USB_SEND=f"2000:{QUERY}", FM1_NO_PLAY="1", FM1_KEY_AT="999999")
    r = subprocess.run([live, "--bin", xip, "--entry", "0x02000120", "--usb", "--shot", "3000", "--lcd-only"],
                       capture_output=True, text=True, env=env, timeout=600)
    out = r.stdout + r.stderr
    problems = []

    m = re.search(r"usb: Configured \{ product: \"([^\"]*)\", vid: (\d+), pid: (\d+) \}", out)
    if not m:
        problems.append("the host did not get the FM-1 configured")
    else:
        print(f"enumerated: {m.group(1)!r}, {int(m.group(2)):04x}:{int(m.group(3)):04x}")
        if (m.group(1), int(m.group(2)), int(m.group(3))) != ("FM-1 Xebec", 0x1209, 0x0001):
            problems.append("unexpected product string or ids")
    for line in out.splitlines():
        if re.search(r"usb: (Failed|Detached)", line):
            problems.append(line.strip())

    ids = []
    for hexmsg in re.findall(r"usb in:\s+([0-9a-f ]+)", out):
        got = identity([int(x, 16) for x in hexmsg.split()])
        if got is not None:
            ids.append(got)
    print(f"identity replies: {ids}")
    if ids != [PRODUCT]:
        problems.append(f"expected one identity reply {PRODUCT}")

    got = read_ppm(os.path.join("work", "lcd.ppm"))
    mhz = screen.find_value(got, "CPU", 24, 1000)
    want = screen.expected(build, values={"CPU": f"{mhz}MHZ"}, usb_ok=True)
    diag = range(6, 14)                # the diagnostics line (USB counters), not checked here
    wrong = sum(1 for y in range(screen.H) if y not in diag for x in range(screen.W)
                if any(abs(a - b) > 2 for a, b in zip(got[y][x], want[y][x])))
    print(f"screen at 3 s: {wrong} pixels differ from the boot screen with USB OK")
    if mhz is None or wrong:
        problems.append("the screen does not show the boot screen with USB OK")

    if problems:
        print(out[-4000:])
        for p in problems:
            print("  " + p)
        sys.exit("USB check FAILED")
    print("USB passed: enumerated, identity answered, USB line OK")


if __name__ == "__main__":
    main()
