# STLite Vector

**ACMOJ Problem ID**: 2579

## Table of Contents

- [STLite Vector](#stlite-vector)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
  - [Assignment Description](#assignment-description)
    - [Grade Composition](#grade-composition)
  - [Assignment Requirements](#assignment-requirements)
    - [Implementation Details](#implementation-details)
    - [Important Notes](#important-notes)
  - [Test Data](#test-data)
  - [Per-Testcase Resource Limits](#per-testcase-resource-limits)
  - [Submission Requirements](#submission-requirements)
    - [File Descriptions](#file-descriptions)
    - [Submission Guidelines](#submission-guidelines)
    - [Allowed Libraries](#allowed-libraries)
    - [Academic Integrity](#academic-integrity)

## Introduction

This assignment requires you to implement a `vector` container similar to `std::vector` in the C++ Standard Library. You need to implement dynamic array functionality with automatic memory management and provide a complete set of member functions and iterators.

## Assignment Description

### Grade Composition

| Component | Percentage |
| :--: | :--: |
| OJ Score | 80% |
| Code Review | 20% |

## Assignment Requirements

### Implementation Details

The basic implementation requirements for Vector are provided in the project documentation and `vector.hpp`. You only need to submit the contents of `vector.hpp` for final submission.

### Important Notes

- **No Default Constructor Assumption**: In some test cases, the template type does not have a default constructor. Therefore, directly using `T* p = new T[...];` may cause problems. You should use placement new or allocator-based approaches.

- **Robustness Testing**: Your program will be subject to robustness testing to ensure proper error handling and edge case management.

- **Memory Management**: Pay attention to memory leaks. Ensure all dynamically allocated memory is properly freed.

## Test Data

Public test cases for local testing are provided at:
- `./data/` - Test files organized by difficulty

Each test directory contains:
- `code.cpp` - Test driver code
- `answer.txt` - Expected output

## Per-Testcase Resource Limits

- **Time Limit (per test case)**: 250 ms (min), 100000 ms (max)
- **Memory Limit (per test case)**: 512 MiB (min), 768 MiB (max)
- **Disk Usage**: Disk access is not permitted

## Submission Requirements

### File Descriptions

The assignment package includes:

- **`vector.hpp`**: The only file you need to implement and submit to OJ. This file contains the interface framework; you need to complete the implementation.

- **`exceptions.hpp`** and **`utility.hpp`**: Auxiliary files (**DO NOT MODIFY**). These provide exception handling classes and the pair class, which you can use freely in your code.

- **`data/` directory**: Contains multiple sets of test data, located in separate subdirectories with test helper classes like `class-integer.hpp`, `class-bint.hpp`, and `class-matrix.hpp`.

### Submission Guidelines

- For ACMOJ problem 2579, submit only the contents of `vector.hpp`
- Do not modify the provided interface framework
- Ensure your implementation meets time and memory limits
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

If during Code Review any violations are found (including but not limited to using other header files or using unconventional methods to pass test cases), your final score may be significantly reduced or become **0 points**.

---

**For detailed interface specifications, please refer to the comments in `vector.hpp`.**
