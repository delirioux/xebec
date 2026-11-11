#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 delirioux
# Xebec - build the firmware app image.
#
#   JIELI_TOOLCHAIN=~/.jieli/current tools/build.sh
#
# Output in build/:
#   xebec.elf        linked image with symbols
#   xebec_app.bin    the app as it sits in flash from 0x02000120
#   xebec_xip.bin    the same, padded to start at 0x02000000 (for fm1-emu --bin)
#   xebec.dis        disassembly
#   xebec.map        link map
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TC="${JIELI_TOOLCHAIN:-$HOME/.jieli/current}"
CC="$TC/pi32v2/bin/clang"
LD="$TC/pi32v2/bin/ld"
OBJCOPY="$TC/common/bin/objcopy"
OBJDUMP="$TC/common/bin/objdump"
OUT="$ROOT/build"

for tool in "$CC" "$LD" "$OBJCOPY" "$OBJDUMP"; do
    [ -x "$tool" ] || { echo "missing $tool: run tools/get_toolchain.sh" >&2; exit 1; }
done

# r3 with the FPU: floats run in hardware, never through soft-float routines.
CFLAGS="-target pi32v2 -mcpu=r3 -mfprev1 -O2 -ffreestanding -nostdlib -fno-builtin \
        -ffunction-sections -fdata-sections -Wall -Wextra -Werror \
        -I$ROOT/firmware/board -I$ROOT/firmware/src"

rm -rf "$OUT"
mkdir -p "$OUT/obj"

OBJS=""
"$CC" -target pi32v2 -mcpu=r3 -c "$ROOT/firmware/boot/start.S" -o "$OUT/obj/start.o"
OBJS="$OBJS $OUT/obj/start.o"
for src in "$ROOT"/firmware/boot/*.c "$ROOT"/firmware/board/*.c "$ROOT"/firmware/src/*.c; do
    obj="$OUT/obj/$(basename "$src" .c).o"
    "$CC" $CFLAGS -c "$src" -o "$obj"
    OBJS="$OBJS $obj"
done

"$LD" -T "$ROOT/firmware/boot/xebec.ld" --gc-sections -Map "$OUT/xebec.map" \
    $OBJS -o "$OUT/xebec.elf"

# JieLi's objcopy takes exactly one -j per call: code and data separately.
"$OBJCOPY" -O binary -j .text "$OUT/xebec.elf" "$OUT/text.bin"
"$OBJCOPY" -O binary -j .data "$OUT/xebec.elf" "$OUT/data.bin"
"$OBJDUMP" -d "$OUT/xebec.elf" > "$OUT/xebec.dis"

# Join them: .data's flash copy sits at __xb_data_load (from the link map).
# fm1-emu loads --bin at 0x02000000, so the XIP image gets the 0x120-byte
# header gap in front.
python3 - "$OUT" <<'EOF'
import re, sys
out = sys.argv[1]
text = open(f"{out}/text.bin", "rb").read()
data = open(f"{out}/data.bin", "rb").read()
m = re.search(r"0x([0-9a-fA-F]+)\s+__xb_data_load\b", open(f"{out}/xebec.map").read())
if not m:
    sys.exit("__xb_data_load not found in xebec.map")
data_off = int(m.group(1), 16) - 0x02000120
if data_off < len(text):
    sys.exit(f".data load address overlaps .text ({data_off:#x} < {len(text):#x})")
app = text + bytes(data_off - len(text)) + data
open(f"{out}/xebec_app.bin", "wb").write(app)
open(f"{out}/xebec_xip.bin", "wb").write(bytes(0x120) + app)
print(f"xebec_app.bin: {len(app)} bytes (text {len(text)}, data {len(data)})")
EOF
