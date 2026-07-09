# STLite Linked HashMap

**ACMOJ Problem ID**: 1866

## Table of Contents

- [STLite Linked HashMap](#stlite-linked-hashmap)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
  - [Assignment Description](#assignment-description)
    - [Grade Composition](#grade-composition)
  - [Assignment Requirements](#assignment-requirements)
    - [Implementation Requirements](#implementation-requirements)
    - [Complexity Requirements](#complexity-requirements)
  - [Test Data](#test-data)
  - [Per-Testcase Resource Limits](#per-testcase-resource-limits)
  - [Submission Requirements](#submission-requirements)
    - [File Descriptions](#file-descriptions)
    - [Submission Guidelines](#submission-guidelines)
    - [Allowed Libraries](#allowed-libraries)
    - [Academic Integrity](#academic-integrity)

## Introduction

This assignment requires you to implement a `LinkedHashMap` data structure. A LinkedHashMap combines the features of a HashMap with the ability to maintain insertion order, similar to Java's `LinkedHashMap` or Python's `OrderedDict`.

## Assignment Description

### Grade Composition

| Component | Percentage |
| :--: | :--: |
| OJ Score | 80% |
| Code Review | 20% |

## Assignment Requirements

### Implementation Requirements

The LinkedHashMap implementation has requirements similar to HashMap, but with additional features:

1. **Insert key-value pairs** (HashMap basic functionality)
   - Implement insertion of (Key, Value) pairs into the data structure

2. **Query values by key** (HashMap basic functionality)
   - Implement querying the Value corresponding to a specific Key in the data structure

3. **Access elements in insertion order** (NEW functionality)
   - Implement the ability to iterate through elements in the order they were inserted
   - This is the main difference from a regular HashMap

4. **Constant time complexity**
   - All implemented features must have **expected O(1) time complexity**

5. **Interface implementation**
   - Specific interfaces to implement are detailed in the provided `linked_hashmap.hpp` file

### Complexity Requirements

- All operations (insert, query, iterate) must maintain **expected O(1)** time complexity
- This requires combining a hash table with a doubly-linked list

## Test Data

Public test cases for local testing are provided at:
- `./data/` - Test files organized by test groups

Each test directory contains:
- Test driver code (`.cpp` files)
- Expected output (`.ans` files)
- Some tests include `_memcheck` variants for memory leak detection

## Per-Testcase Resource Limits

- **Time Limit (per test case)**: 400 ms (min), 24000 ms (max)
- **Memory Limit (per test case)**: 512 MiB (min), 893 MiB (max)
- **Disk Usage**: Disk access is not permitted

## Submission Requirements

### File Descriptions

The assignment package includes:

- **`linked_hashmap.hpp`**: The only file you need to implement and submit to OJ.

- **`exceptions.hpp`** and **`utility.hpp`**: Auxiliary files (**DO NOT MODIFY**). These provide exception handling classes and the pair class.

- **`Tutorial.md`**: Additional guidance and examples for implementation.

- **`data/` directory**: Contains multiple sets of test data in separate subdirectories.

### Submission Guidelines

- For ACMOJ problem 1866, submit only the contents of `linked_hashmap.hpp`
- Do not modify the provided interface framework
- Ensure your implementation meets time and memory limits
- Maintain O(1) expected complexity for all operations
- Use C++

### Allowed Libraries

**IMPORTANT**: In this assignment, you are only allowed to use the following five C++ standard library headers (unless provided in the starter code):
- `<cstdio>`
- `<cstring>`
- `<iostream>`
- `<cmath>`
- `<string>`

If you need other functionality, please implement it yourself.

### Academic Integrity

If during Code Review any violations are found (including but not limited to using other header files or using unconventional methods to pass test cases), your final score may be significantly reduced or become **0 points**.

---

**For detailed interface specifications, please refer to the comments in `linked_hashmap.hpp` and `Tutorial.md`.**
