#!/usr/bin/env python3
"""Generate the single-file OJ submission `code.cpp`.

It concatenates `src/include/int2048.h` and `src/int2048.cpp`, dropping the
`#include "int2048.h"` line from the implementation so the result is a
self-contained translation unit (as required by the assignment).
"""

import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join(ROOT, "src", "include", "int2048.h")
IMPL = os.path.join(ROOT, "src", "int2048.cpp")
OUT = os.path.join(ROOT, "code.cpp")


def main() -> None:
    with open(HEADER, "r", encoding="utf-8") as f:
        header_lines = f.readlines()
    with open(IMPL, "r", encoding="utf-8") as f:
        impl_lines = f.readlines()

    # `#pragma once` is meaningless (and warns) in a single-file submission.
    header = "".join(ln for ln in header_lines if ln.strip() != "#pragma once")

    # The submission must only rely on the headers already pulled in by
    # int2048.h (the assignment forbids other includes). std::string,
    # std::max and std::swap are provided transitively by libstdc++, so the
    # convenience includes used during local development are dropped here.
    dropped = {
        '#include "int2048.h"',
        "#include <algorithm>",
        "#include <string>",
    }
    impl = "".join(ln for ln in impl_lines if ln.strip() not in dropped)

    banner = (
        "// ============================================================\n"
        "// Auto-generated OJ submission for ACMOJ 2014-2019 (int2048).\n"
        "// Generated from src/include/int2048.h + src/int2048.cpp by\n"
        "// tools/build_submission.py -- do not edit by hand.\n"
        "// ============================================================\n\n"
    )

    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(banner)
        f.write(header)
        f.write("\n")
        f.write(impl)

    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
