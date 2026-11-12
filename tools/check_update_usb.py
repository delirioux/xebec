#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 delirioux
"""Xebec - the USB update checks (update_usb.c, sysex.c), natively, against a simulated installer.

    tools/check_update_usb.py build/xebec.fwsc work/FM-1_015.fwsc WORKDIR

Builds firmware/test/update_usb_native.c with the host's gcc and runs it on
Xebec's package and M-VAVE's stock package, both served as the web installer
serves them, with this FM-1's flash head taken to be M-VAVE's (the head
Xebec's package keeps). Also checked: a changed loader, a broken UFW head,
a different flash head and a silent installer are refused, and nothing is
erased or written by the checks.
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fwsc  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    xebec, stock, work = sys.argv[1:4]
    os.makedirs(work, exist_ok=True)
    paths = []
    for name, src in (("xebec.ufw", xebec), ("stock.ufw", stock)):
        ufw, _ = fwsc.unmark(open(src, "rb").read())
        path = os.path.join(work, name)
        open(path, "wb").write(ufw)
        paths.append(path)
    loader_c = os.path.join(work, "ota_loader.c")
    subprocess.run([sys.executable, os.path.join(ROOT, "tools", "gen_loader.py"),
                    os.path.join(ROOT, "vendor", "m-vave", "ota.bin"), loader_c], check=True)
    exe = os.path.join(work, "update_usb_native")
    src = [os.path.join(ROOT, "firmware", p) for p in
           ("test/update_usb_native.c", "src/update_usb.c", "src/sysex.c", "src/ota_stage.c")]
    subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                    "-I", os.path.join(ROOT, "firmware", "src"),
                    "-o", exe, *src, loader_c], check=True)
    r = subprocess.run([exe, *paths, os.path.join(ROOT, "vendor", "m-vave", "stock", "head.bin")])
    if r.returncode:
        sys.exit("USB update checks FAILED")


if __name__ == "__main__":
    main()
