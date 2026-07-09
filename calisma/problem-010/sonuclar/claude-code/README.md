# STLite List (Doubly-Linked List)

**ACMOJ Problem ID**: 2653

## Table of Contents

- [STLite List (Doubly-Linked List)](#stlite-list-doubly-linked-list)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
  - [Assignment Description](#assignment-description)
    - [Grade Composition](#grade-composition)
  - [Assignment Requirements](#assignment-requirements)
    - [Basic Requirements](#basic-requirements)
    - [Interface Specifications](#interface-specifications)
      - [Constructors and Destructors](#constructors-and-destructors)
      - [Element Access](#element-access)
      - [Capacity](#capacity)
      - [Modifiers](#modifiers)
      - [Operations](#operations)
    - [Implementation Hints](#implementation-hints)
  - [Test Data](#test-data)
  - [Per-Testcase Resource Limits](#per-testcase-resource-limits)
  - [Submission Requirements](#submission-requirements)
    - [File Descriptions](#file-descriptions)
    - [Submission Guidelines](#submission-guidelines)
    - [Allowed Libraries](#allowed-libraries)
    - [Academic Integrity](#academic-integrity)

## Introduction

This assignment requires you to implement a `list` container using a doubly-linked list, similar to `std::list` in the C++ Standard Library. You need to provide iterators for list operations and implement some algorithm library functions that don't support non-contiguous memory storage.

## Assignment Description

### Grade Composition

| Component | Percentage |
| :--: | :--: |
| OJ Score | 80% |
| Code Review | 20% |

## Assignment Requirements

### Basic Requirements

Implement a `list` class using a doubly-linked list structure, and provide iterators for related list operations.

Implement some functions from the `algorithm` library that don't support data structures with non-contiguous memory storage.

### Interface Specifications

#### Constructors and Destructors

- Default constructor
- Copy constructor
- `list &operator=(const list &other)` - Assignment operator that copies data from another list

#### Element Access

- `front()` - Access the first element
- `back()` - Access the last element

#### Capacity

- `empty()` - Check if the list is empty
- `size()` - Return the number of elements
- `clear()` - Remove all elements

#### Modifiers

- `insert(iterator pos, const T &value)` - Insert a node with value before `pos`, return iterator to the inserted node

- `erase(iterator pos)` - Delete the node at `pos`, return iterator to the next node

- `push_back()` - Add element to the end
- `push_front()` - Add element to the beginning

- `pop_back()` - Remove the last element
- `pop_front()` - Remove the first element

#### Operations

- `sort()` - Sort the list contents in ascending order

- `merge(list &other)` - Merge two lists. Both lists must be sorted in ascending order. After merging, the result must be in ascending order.

- `reverse()` - Reverse the list

- `unique()` - For each sequence of consecutive identical elements, keep only the first one and delete the rest

### Implementation Hints

- **No Default Constructor Assumption**: The template type is not guaranteed to have a default constructor. Handle this the same way as in `vector`. The template type is guaranteed to have `operator<` and `operator==` defined.

- **Data Movement Constraints**:
  - `sort()` **allows** copying and moving of data
  - `merge()` and `reverse()` require **no** copying or moving of data (only pointer manipulation)

- **Algorithm Library**: The provided `algorithm.hpp` can be used. You can use `sjtu::sort()` with the interface being array start and end positions plus a comparison function (see reference code). You may try explicit instantiation, e.g., `sjtu::sort<T *>()`.

## Test Data

Public test cases for local testing are provided at:
- `./data/` - Test files organized by test groups (one through six)

Each test directory contains:
- `code.cpp` - Test driver code
- `answer.txt` - Expected output
- Helper classes: `class-integer.hpp`, `class-bint.hpp`, `class-matrix.hpp`

## Per-Testcase Resource Limits

- **Time Limit (per test case)**: 2000 ms (min), 25000 ms (max)
- **Memory Limit (per test case)**: 512 MiB (min), 768 MiB (max)
- **Disk Usage**: Disk access is not permitted

## Submission Requirements

### File Descriptions

The assignment package includes:

- **`list.hpp`**: The only file you need to implement and submit to OJ.

- **`exceptions.hpp`** and **`utility.hpp`**: Auxiliary files (**DO NOT MODIFY**). These provide exception handling classes and the pair class.

- **`algorithm.hpp`**: Provides `sjtu::sort()` function for use in your implementation.

- **`data/` directory**: Contains multiple sets of test data in separate subdirectories.

### Submission Guidelines

- For ACMOJ problem 2653, submit only the contents of `list.hpp`
- Do not modify the provided interface framework
- Ensure your implementation meets time and memory limits
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

**For detailed interface specifications, please refer to the comments in `list.hpp`.**
