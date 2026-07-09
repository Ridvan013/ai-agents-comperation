# STLite Map

**ACMOJ Problem IDs**: 2671, 2672

## Table of Contents

- [STLite Map](#stlite-map)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
  - [Assignment Description](#assignment-description)
    - [Grade Composition](#grade-composition)
  - [Assignment Requirements](#assignment-requirements)
    - [Implementation Requirements](#implementation-requirements)
    - [Tree Structure Choice](#tree-structure-choice)
    - [Important Notes](#important-notes)
  - [Test Data](#test-data)
  - [Per-Testcase Resource Limits](#per-testcase-resource-limits)
    - [Problem 2671](#problem-2671)
    - [Problem 2672](#problem-2672)
  - [Submission Requirements](#submission-requirements)
    - [File Descriptions](#file-descriptions)
    - [Submission Guidelines](#submission-guidelines)
    - [Allowed Libraries](#allowed-libraries)
    - [Academic Integrity](#academic-integrity)

## Introduction

This assignment requires you to implement a `map` data structure similar to `std::map` in the C++ Standard Library, including iterators. You need to implement this using a self-balancing binary search tree.

## Assignment Description

### Grade Composition

| Component | Percentage |
| :--: | :--: |
| OJ Score | 80% |
| Code Review | 20% |

## Assignment Requirements

### Implementation Requirements

- Implement a data structure similar to C++ STL `map`, including iterators and other features
- The interface framework is provided in this repository; you only need to implement the content required in the `.hpp` file
- You need to complete the assignment code in the provided files
- Submit `map.hpp` on OJ, which should implement basic map operations and have certain robustness
- You need to pass all test cases for both **2671** and **2672**

### Tree Structure Choice

You must choose **one** of the following self-balancing binary search tree structures:
- **AVL Tree**
- **AA Tree**
- **Red-Black Tree**

### Important Notes

- **No Default Constructor Assumption**: In some test cases, the template type does not have a default constructor. Therefore, directly using `T* p = new T[...];` may cause problems.

- **Robustness Testing**: Your program will be subject to robustness testing to ensure proper error handling.

- **Memory Leak Testing**: We will check for memory leaks.

## Test Data

Public test cases for local testing are provided at:
- `./data/` - Regular test files organized by test groups (one through five)
- `./corner_data/` - Corner case tests

Each test directory contains:
- `code.cpp` - Test driver code
- `answer.txt` - Expected output
- Some tests include `.memcheck` variants for memory leak detection
- Helper classes: `class-integer.hpp`, `class-bint.hpp`, `class-matrix.hpp`

## Per-Testcase Resource Limits

### Problem 2671

- **Time Limit (per test case)**: 100 ms (min), 20000 ms (max)
- **Memory Limit (per test case)**: 512 MiB
- **Disk Usage**: Disk access is not permitted

### Problem 2672

- **Time Limit (per test case)**: 10000 ms (min), 30000 ms (max)
- **Memory Limit (per test case)**: 512 MiB (min), 893 MiB (max)
- **Disk Usage**: Disk access is not permitted

## Submission Requirements

### File Descriptions

The assignment package includes:

- **`src/map.hpp`**: The only file you need to implement and submit to OJ. Complete the `TODO` sections. Do not arbitrarily change the interface, as it will prevent the evaluation code from correctly calling your file.

- **`src/exceptions.hpp`** and **`src/utility.hpp`**: Code required for correct local testing (**DO NOT MODIFY**).

- **`data/` directory**: Test source code and correct output.

- **`corner_data/` directory**: Additional corner case tests.

### Submission Guidelines

- For ACMOJ problems 2671 and 2672, submit only the contents of `map.hpp`
- Do not modify the provided interface framework
- Ensure your implementation meets time and memory limits for both problems
- Use C++

### Allowed Libraries

**IMPORTANT**: In this assignment, you are only allowed to use the following five C++ standard library headers:
- `<cstdio>`
- `<cstring>`
- `<iostream>`
- `<cmath>`
- `<string>`

If you need other functionality, please implement it yourself.

### Academic Integrity

- **Final Score** = 80% (OJ testing) + 20% (code review)
- **Academic Integrity Issues**: If during Code Review any violations are found (including but not limited to using other header files or using unconventional methods to pass test cases), your final score may be **0 points**.

---

**For detailed interface specifications, please refer to the comments in `src/map.hpp`.**
