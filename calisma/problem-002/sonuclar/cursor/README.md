# int2048 - Big Integer Arithmetic

**ACMOJ Problem IDs**: 2014-2019

## Table of Contents

- [int2048 - Big Integer Arithmetic](#int2048---big-integer-arithmetic)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
    - [Background](#background)
  - [Assignment Description](#assignment-description)
    - [Grade Composition](#grade-composition)
  - [Assignment Requirements](#assignment-requirements)
    - [Implementation Requirements](#implementation-requirements)
    - [Special Requirements](#special-requirements)
      - [Division and Modulo Operations](#division-and-modulo-operations)
    - [Interface Requirements](#interface-requirements)
      - [Basic Operations (Integer 1)](#basic-operations-integer-1)
      - [Operator Overloading (Integer 2)](#operator-overloading-integer-2)
  - [Test Cases and Data Constraints](#test-cases-and-data-constraints)
    - [2014 - Basic Test](#2014---basic-test)
    - [2015 - Operator Overloading Test](#2015---operator-overloading-test)
    - [2016 - Digit Compression Test](#2016---digit-compression-test)
    - [2017 - Multiplication Speed Test](#2017---multiplication-speed-test)
    - [2018 - Division Speed Test](#2018---division-speed-test)
    - [2019 - Stress Test](#2019---stress-test)
  - [Submission Requirements](#submission-requirements)
    - [File Preparation](#file-preparation)
    - [Submission Guidelines](#submission-guidelines)
    - [Evaluation Notes](#evaluation-notes)
    - [Academic Integrity](#academic-integrity)

## Introduction

### Background

**High-precision arithmetic** is a fundamental algorithm in computer programming. Due to CPU word length limitations, standard integer types have restricted ranges. For example, on a 32-bit CPU, an integer can only store values up to 4,294,967,295. For calculations involving larger numbers, simulation techniques are required.

The typical approach involves:
- Storing digits in arrays
- Processing input by separating characters
- Outputting in reverse array order
- Simulating manual arithmetic operations (column addition, subtraction, etc.)
- Using different number bases for various optimization purposes

This assignment requires implementing high-precision addition, subtraction, multiplication, and division operations for arbitrarily large integers in C++.

## Assignment Description

### Grade Composition

You need to pass **6 ACMOJ problems** with the following IDs:

| Problem ID | Problem Name | Percentage |
| :--: | :--: | :--: |
| 2014 | int2048 - Basic Test | 10% |
| 2015 | int2048 - Operator Overloading | 10% |
| 2016 | int2048 - Digit Compression Test | 10% |
| 2017 | int2048 - Multiplication Speed Test | 15% |
| 2018 | int2048 - Division Speed Test | 15% |
| 2019 | int2048 - Stress Test | 20% |
| Code Review | 20% |

Here are several points that need clarification:

- In the Code Review, we will **strictly examine your code style and repository organization structure, etc.**. 

- This assignment provides some sample test data stored in the `/workspace/data/002/data/` directory. Note that these are **not the actual test cases** used for evaluation on the Online Judge.

- Besides the provided sample data, we strongly encourage you to design your own test cases based on your program logic to assist debugging.

- Please ensure your code has sufficient robustness to handle various edge cases and extreme situations.

## Assignment Requirements

### Implementation Requirements

You are required to implement a C++ big integer class named `int2048`.

**File Structure:**
- The interface is provided in `int2048.h`
- You need to create a new file `int2048.cpp` to implement the given interfaces
- You may add new interfaces or modify existing ones (such as implementing `swap` functions or move constructors) **while maintaining compatibility with all current interfaces**
- All implementations should be completed in `int2048.cpp`, not in `int2048.h`
- For OJ submission, you need to copy the implementation from `int2048.cpp` into `int2048.h` before submitting

### Special Requirements

#### Division and Modulo Operations

**IMPORTANT**: You do not need to handle cases where the divisor or modulus is 0. Such cases result in undefined behavior. We guarantee that test cases will not contain such data.

**Division Operation Behavior:**

In our big integer class implementation, the division operation `x / y` always rounds toward negative infinity (floor division, similar to Python's behavior):

```python
# Python code example
print(10 // 3, -10 // 3, 10 // -3, -10 // -3)
# Output: 3 -4 -4 3
# Because -10 / -3 = 3.333... rounds down to 3
# And 10 / -3 = -3.333... rounds down to -4
```

Note that in C++, [signed integer division](https://en.cppreference.com/w/cpp/language/operator_arithmetic) rounds toward zero:

```cpp
// C++ code example
std::cout
    << 10 / 3   << ' '
    << 10 / -3  << ' '
    << -10 / 3  << ' '
    << -10 / -3 << '\n';
// Output: 3 -3 -3 3
// Because -10 / -3 = 3.333... rounds toward zero to 3
// And 10 / -3 = -3.333... rounds toward zero to -3
```

**Modulo Operation Definition:**

The modulo operation `x % y` is defined as: `x % y = x - (x / y) * y`

### Interface Requirements

#### Basic Operations (Integer 1)

The following basic operations are tested in problem 2014:

```cpp
// Read a big integer from string
void read(const std::string &s);

// Print the stored big integer
void print();

// Add a big integer (in-place)
int2048 &add(const int2048 &);

// Return the sum of two big integers
friend int2048 add(int2048, const int2048 &);

// Subtract a big integer (in-place)
int2048 &minus(const int2048 &);

// Return the difference of two big integers
friend int2048 minus(int2048, const int2048 &);
```

#### Operator Overloading (Integer 2)

The following operator overloads are tested in problems 2015-2019:

```cpp
// Unary operators
int2048 operator+() const;
int2048 operator-() const;

// Assignment operator
int2048 &operator=(const int2048 &);

// Arithmetic compound assignment operators
int2048 &operator+=(const int2048 &);
int2048 &operator-=(const int2048 &);
int2048 &operator*=(const int2048 &);
int2048 &operator/=(const int2048 &);
int2048 &operator%=(const int2048 &);

// Arithmetic binary operators
friend int2048 operator+(int2048, const int2048 &);
friend int2048 operator-(int2048, const int2048 &);
friend int2048 operator*(int2048, const int2048 &);
friend int2048 operator/(int2048, const int2048 &);
friend int2048 operator%(int2048, const int2048 &);

// Stream operators
friend std::istream &operator>>(std::istream &, int2048 &);
friend std::ostream &operator<<(std::ostream &, const int2048 &);

// Comparison operators
friend bool operator==(const int2048 &, const int2048 &);
friend bool operator!=(const int2048 &, const int2048 &);
friend bool operator<(const int2048 &, const int2048 &);
friend bool operator>(const int2048 &, const int2048 &);
friend bool operator<=(const int2048 &, const int2048 &);
friend bool operator>=(const int2048 &, const int2048 &);
```

## Test Cases and Data Constraints

### 2014 - Basic Test

**Problem Description:**

This problem corresponds to Integer 1, testing only basic operations (read, print, add, minus).

**Data Constraints:**

- This is a basic test case without operator overloading
- Number absolute values during computation: $\leq 10^{1000}$

**Resource Limits (per test case):**

- **Time Limit**: 1000 ms
- **Memory Limit**: 47 MiB
- **Disk Usage**: Disk access is not permitted

### 2015 - Operator Overloading Test

**Problem Description:**

This problem (and all subsequent problems) corresponds to Integer 2, testing operator overloading functionality.

**Data Constraints:**

- This is a basic test case, mainly testing operator overloading
- Number absolute values during computation: $\leq 10^{1000}$

**Resource Limits (per test case):**

- **Time Limit**: 1000 ms (minimum), 5000 ms (maximum)
- **Memory Limit**: 47 MiB
- **Disk Usage**: Disk access is not permitted

### 2016 - Digit Compression Test

**Problem Description:**

This problem tests the efficiency of digit compression (base conversion) techniques.

**Data Constraints:**

- For basic test cases: Number absolute values during computation $\leq 10^{1000}$
- For digit compression test cases: Number absolute values during computation $\leq 10^{30000}$, with division operands $\leq 10^{3000}$

**Resource Limits (per test case):**

- **Time Limit**: 1000 ms (minimum), 2500 ms (maximum)
- **Memory Limit**: 47 MiB
- **Disk Usage**: Disk access is not permitted

### 2017 - Multiplication Speed Test

**Problem Description:**

This problem focuses on testing multiplication operation speed.

**Data Constraints:**

- Number absolute values during computation: $\leq 10^{200000}$

**Resource Limits (per test case):**

- **Time Limit**: 1000 ms
- **Memory Limit**: 47 MiB
- **Disk Usage**: Disk access is not permitted

### 2018 - Division Speed Test

**Problem Description:**

This problem focuses on testing division operation speed.

**Data Constraints:**

- Number absolute values during computation: $\leq 10^{12000}$

**Resource Limits (per test case):**

- **Time Limit**: 1000 ms
- **Memory Limit**: 47 MiB
- **Disk Usage**: Disk access is not permitted

### 2019 - Stress Test

**Problem Description:**

This problem is a comprehensive stress test for all operations.

**Data Constraints:**

- Number absolute values during computation: $\leq 10^{500000}$

**Resource Limits (per test case):**

- **Time Limit**: 1000 ms (minimum), 10000 ms (maximum)
- **Memory Limit**: 190 MiB
- **Disk Usage**: Disk access is not permitted

## Submission Requirements

### C++ Compilation Process

For C++ submissions, the Online Judge will use a command similar to the following to compile your code:

```bash
g++ code.cpp -o code -fmax-errors=10 -O2 -DONLINE_JUDGE -lm -std=c++20
```

Compilation parameters:
- `-std=c++20`: Uses C++20 standard
- `-O2`: Optimization level 2
- `-DONLINE_JUDGE`: Defines the ONLINE_JUDGE macro
- `-fmax-errors=10`: Limits error messages
- `-lm`: Links math library

### File Preparation

For OJ submission, you need to prepare a single `code.cpp` file that contains both the header definitions and implementations:

1. **During Development**:
   - Keep your interface declarations in `int2048.h`
   - Keep your implementations in `int2048.cpp`
   - This separation makes development and debugging easier

2. **For OJ Submission**:
   - Create a file named `code.cpp`
   - Copy the entire content of `int2048.h` into `code.cpp`
   - Copy the implementation from `int2048.cpp` and paste it into `code.cpp` (you can place it inside the class definition or after it)
   - Remove any `#include "int2048.h"` statements from the implementation part
   - Ensure all necessary standard library headers are included (e.g., `<iostream>`, `<string>`, etc.)

### Submission Guidelines

- Submit a single file named `code.cpp` to the Online Judge

### Evaluation Notes

- The program output must exactly match the expected output (including format)
- Exceeding time or memory limits will be judged as the corresponding error type
- Passing sample test cases does not guarantee passing all OJ test cases
- Please pay attention to algorithm time complexity and optimization techniques

### Academic Integrity

If any violations are found during evaluation or code review (including but not limited to using unconventional methods to pass test cases), your final score may be significantly reduced or become **0 points**. 

