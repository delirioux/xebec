#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 delirioux
"""Xebec - the GitHub Pages site: a short page at the root, dev's installer under dev/.

    tools/make_site.py build/xebec.fwsc BUILD _site
"""
import hashlib
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

LANDING = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Xebec</title>
<style>body{margin:0;background:#0b2b2b;color:#f2f7f7;font:16px/1.5 system-ui,sans-serif}
main{max-width:640px;margin:0 auto;padding:24px 16px;text-align:center}
img{width:min(320px,80%);background:#fff;border-radius:8px;padding:8px}a{color:#2fb3b3}</style></head>
<body><main><img src="dev/logo.png" alt="Xebec">
<p>Built from scratch for the M-VAVE FM-1, fast and lean, no borrowed planks.</p>
<p>Not released yet. The current test build and its installer: <a href="dev/">dev</a>.</p>
<p><a href="https://github.com/delirioux/xebec">github.com/delirioux/xebec</a></p></main></body></html>
"""


def main():
    pkg, build, out = sys.argv[1:4]
    dev = os.path.join(out, "dev")
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(dev)
    sha = hashlib.sha256(open(pkg, "rb").read()).hexdigest()
    page = open(os.path.join(ROOT, "web", "index.html")).read()
    if "__SHA256__" not in page or "__BUILD__" not in page:
        sys.exit("web/index.html: placeholders missing")
    open(os.path.join(dev, "index.html"), "w").write(page.replace("__SHA256__", sha).replace("__BUILD__", build))
    shutil.copy(os.path.join(ROOT, "web", "install.js"), dev)
    shutil.copy(os.path.join(ROOT, "docs", "logo.png"), os.path.join(dev, "logo.png"))
    shutil.copy(pkg, os.path.join(dev, "xebec.fwsc"))
    open(os.path.join(out, "index.html"), "w").write(LANDING)
    print(f"{out}: root page, dev/ installer for build {build} (package sha256 {sha})")


if __name__ == "__main__":
    main()
