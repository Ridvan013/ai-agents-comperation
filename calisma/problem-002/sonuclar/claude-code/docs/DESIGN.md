# int2048 — implementation notes

## Repository layout

| Path | Purpose |
| --- | --- |
| `src/include/int2048.h` | Public interface plus the private representation and helper declarations. |
| `src/int2048.cpp` | The full implementation. |
| `code.cpp` | Single-file OJ submission, generated from the two files above by `tools/make_submission.sh`. |
| `tests/repl.cpp` | Small REPL over the interface. |
| `tests/reference_test.py` | Randomised differential test against Python big integers. |
| `CMakeLists.txt` / `Makefile` | Build definitions. |

## Representation

A number is a sign (`+1` / `-1`, zero is always `+1`) and a little-endian
`std::vector<int>` of base-`1000` limbs. The vector is kept canonical: no
leading zero limbs, and zero is the empty vector. Base `1000` keeps every FFT
coefficient below `1000`, which leaves a very comfortable margin against
double-precision rounding even for million-digit products.

## Algorithms

- **Addition / subtraction** — schoolbook over limbs, `O(n)`. Signed operations
  reduce to magnitude add/subtract after a sign/magnitude comparison.
- **Multiplication** — schoolbook for small operands (`min length <= 64`),
  otherwise an iterative complex FFT. Both real inputs are packed into a single
  complex array so one forward and one inverse transform suffice.
- **Division / modulo** — floor semantics (the remainder takes the sign of the
  divisor, matching Python). Three layers:
  - single-limb divisor: direct long division;
  - moderate sizes: Newton's method builds `floor(1000^k / b)` from a two-limb
    seed, then one multiply and a bounded correction give the quotient;
  - large divisors: Burnikel–Ziegler recursive division (`div_2n_1n` /
    `div_3n_2n`) over a normalised, power-of-two-padded divisor, bottoming out
    in the Newton divider. This keeps the balanced big / big case near
    `O(M(n) log n)` instead of the quadratic schoolbook cost.

## Building and testing

```sh
make repl            # build the test REPL
make test            # build + run the Python differential test
make submission      # regenerate code.cpp
```

or with CMake:

```sh
cmake -B build && cmake --build build
python tests/reference_test.py
```
