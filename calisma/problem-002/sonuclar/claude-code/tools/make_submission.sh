#!/usr/bin/env bash
# Produce the single-file OJ submission `code.cpp` by concatenating the public
# header with the implementation (dropping the local #include of that header).
set -euo pipefail
cd "$(dirname "$0")/.."

out=code.cpp
{
  # Drop `#pragma once` (the file is a translation unit on the OJ, and the
  # #ifndef guard is enough); keep everything else.
  grep -v '#pragma once' src/include/int2048.h
  echo
  grep -v '#include "int2048.h"' src/int2048.cpp
} > "$out"

echo "Wrote $out"
