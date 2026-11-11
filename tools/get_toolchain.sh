#!/bin/sh
# Xebec - fetch JieLi's Linux toolchain (clang for pi32v2) from JieLi's own
# package server and point $DEST/current at it.
#
#   tools/get_toolchain.sh [DEST]        DEST defaults to ~/.jieli
set -eu

DEST="${1:-$HOME/.jieli}"
URL="https://pkgman.jieliapp.com/s/linux-toolchain"

mkdir -p "$DEST"
curl -fL --retry 3 "$URL" | tar -xJ -C "$DEST"

TC="$(ls -d "$DEST"/jieli-linux-toolchains-* | sort | tail -n 1)"
ln -sfn "$(basename "$TC")" "$DEST/current"
echo "JIELI_TOOLCHAIN=$DEST/current ($(basename "$TC"))"
