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
# and in build/test/ the same for xebec_flashtest, the flash driver test
# image (firmware/test/flash_test.c in place of main.c; fm1-emu only).
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

# Build code shown on the boot screen: the commit, upper case (the font has no lower case).
BUILD="$(git -C "$ROOT" rev-parse --short=7 HEAD 2>/dev/null | tr 'a-f' 'A-F' || true)"
[ -n "$BUILD" ] || BUILD="LOCAL"

rm -rf "$OUT"
mkdir -p "$OUT/obj" "$OUT/test/obj"

compile() {
    "$CC" $CFLAGS "-DXB_BUILD=\"$BUILD\"" -c "$1" -o "$2"
}

# Everything but main.c, shared by the firmware and the test image.
COMMON=""
"$CC" -target pi32v2 -mcpu=r3 -c "$ROOT/firmware/boot/start.S" -o "$OUT/obj/start.o"
COMMON="$COMMON $OUT/obj/start.o"
for src in "$ROOT"/firmware/boot/*.c "$ROOT"/firmware/board/*.c "$ROOT"/firmware/src/*.c; do
    [ "$(basename "$src")" = main.c ] && continue
    obj="$OUT/obj/$(basename "$src" .c).o"
    compile "$src" "$obj"
    COMMON="$COMMON $obj"
done
compile "$ROOT/firmware/src/main.c" "$OUT/obj/main.o"
compile "$ROOT/firmware/test/flash_test.c" "$OUT/test/obj/flash_test.o"

# image DIR NAME MAIN_OBJ: link and write DIR/NAME.elf, NAME_app.bin,
# NAME_xip.bin, NAME.dis and NAME.map.
image() {
    dir="$1"; name="$2"; main_obj="$3"

    "$LD" -T "$ROOT/firmware/boot/xebec.ld" --gc-sections -Map "$dir/$name.map" \
        $COMMON "$main_obj" -o "$dir/$name.elf"

    # JieLi's objcopy takes exactly one -j per call: code and data separately.
    "$OBJCOPY" -O binary -j .text "$dir/$name.elf" "$dir/text.bin"
    "$OBJCOPY" -O binary -j .data "$dir/$name.elf" "$dir/data.bin"
    "$OBJDUMP" -d "$dir/$name.elf" > "$dir/$name.dis"

    # Join them: .data's flash copy (the flash driver's RAM code first) sits
    # at __xb_data_load (from the link map). fm1-emu loads --bin at
    # 0x02000000, so the XIP image gets the 0x120-byte header gap in front.
    python3 - "$dir" "$name" <<'EOF'
import re, sys
out, name = sys.argv[1], sys.argv[2]
text = open(f"{out}/text.bin", "rb").read()
data = open(f"{out}/data.bin", "rb").read()
m = re.search(r"0x([0-9a-fA-F]+)\s+__xb_data_load\b", open(f"{out}/{name}.map").read())
if not m:
    sys.exit("__xb_data_load not found in the link map")
data_off = int(m.group(1), 16) - 0x02000120
if data_off < len(text):
    sys.exit(f".data load address overlaps .text ({data_off:#x} < {len(text):#x})")
app = text + bytes(data_off - len(text)) + data
open(f"{out}/{name}_app.bin", "wb").write(app)
open(f"{out}/{name}_xip.bin", "wb").write(bytes(0x120) + app)
print(f"{name}_app.bin: {len(app)} bytes (text {len(text)}, data {len(data)})")
EOF
    rm -f "$dir/text.bin" "$dir/data.bin"
}

image "$OUT" xebec "$OUT/obj/main.o"
# The flash driver test, for fm1-emu only (tools/check_flash_test.py).
image "$OUT/test" xebec_flashtest "$OUT/test/obj/flash_test.o"
