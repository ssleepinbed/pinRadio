#!/bin/bash
set -e

OUT="pinRadio"
SRC="src/main.c"

gcc -Wall "$SRC" -o "$OUT"

echo "Build complete: $OUT"
echo "Run with sudo ./pinRadio,"
echo "make sure you're running on a BCM 283X"