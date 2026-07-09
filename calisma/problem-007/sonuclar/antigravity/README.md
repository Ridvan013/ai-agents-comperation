# BASIC Interpreter Assignment

**ACMOJ Problem ID**: 2510

## Table of Contents

- [BASIC Interpreter Assignment](#basic-interpreter-assignment)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
    - [Background](#background)
  - [Assignment Description](#assignment-description)
    - [Grade Composition](#grade-composition)
  - [Assignment Requirements](#assignment-requirements)
    - [BASIC Language Features](#basic-language-features)
    - [Command Types](#command-types)
    - [Implementation Tasks](#implementation-tasks)
    - [Data Constraints](#data-constraints)
  - [Test Data](#test-data)
  - [Per-Testcase Resource Limits](#per-testcase-resource-limits)
  - [Implementation Guidelines](#implementation-guidelines)
    - [Repository Structure](#repository-structure)
    - [Files to Study](#files-to-study)
    - [Files to Implement](#files-to-implement)
    - [Implementation Steps](#implementation-steps)
  - [Submission Requirements](#submission-requirements)
    - [Submission Guidelines](#submission-guidelines)
    - [Evaluation Notes](#evaluation-notes)
  - [Special Notes](#special-notes)
    - [Undefined Behaviors](#undefined-behaviors)
    - [Variable Naming Rules](#variable-naming-rules)
    - [Numeric Values](#numeric-values)
    - [Edge Cases](#edge-cases)
  - [Local Testing](#local-testing)
  - [FAQ](#faq)
  - [Academic Integrity](#academic-integrity)


## Introduction

### Background

BASIC (Beginner's All-purpose Symbolic Instruction Code) is a classic **interpreted** programming language. In this assignment, you need to implement a Minimal BASIC interpreter using C++ that can execute BASIC programs with various features.

The interpreter should support:
- **Immediate execution** of certain statements
- Execution of control flow statements
- Running programs with numbered lines in ascending order

## Assignment Description

### Grade Composition

| Component | Percentage |
| :--: | :--: |
| OJ Score | 80% |
| Code Review | 20% |

- Local sample data are provided to help with development and debugging
- Passing local samples does not guarantee passing OJ tests
- In the Code Review, we will **strictly examine your code style and repository organization structure, etc.**

## Assignment Requirements

### BASIC Language Features

Your interpreter must support the following BASIC commands and features:

**Immediate Mode Commands:**
- `REM <comment>` - Comment (ignored)
- `LET <var> = <exp>` - Variable assignment
- `PRINT <exp>` - Print expression value
- `INPUT <var>` - Read input to variable
- `END` - Terminate program
- `GOTO <line>` - Jump to line number
- `IF <exp1> <op> <exp2> THEN <line>` - Conditional jump
- `RUN` - Execute stored program
- `LIST` - Display stored program
- `CLEAR` - Clear stored program
- `QUIT` - Exit interpreter
- `HELP` - Display help (optional, not tested)

**Program Mode:**
- Programs consist of numbered lines
- Lines executed in ascending order by line number
- Lines can be added, replaced, or removed
- Control flow via GOTO and IF-THEN statements

**Expression Features:**
- Integer arithmetic (addition, subtraction, multiplication, division, modulo)
- Variables (alphanumeric names, not keywords)
- Proper operator precedence and parentheses
- All values are integers (no floating point)

### Command Types

1. **Direct Commands**: Executed immediately (e.g., `PRINT 42`)
2. **Numbered Lines**: Stored in program (e.g., `10 LET x = 5`)
3. **Line Deletion**: Empty line number removes that line (e.g., `10` removes line 10)

### Implementation Tasks

Your main task is to implement a functional BASIC interpreter that:

1. **Parses input** using tokenization
2. **Validates syntax** for all commands
3. **Stores program lines** in appropriate data structures
4. **Executes commands** both immediately and in program mode
5. **Manages variables** and their values
6. **Evaluates expressions** with proper precedence
7. **Handles control flow** (GOTO, IF-THEN)
8. **Provides error handling** for invalid inputs

### Data Constraints

- Variable values: within `int` range (-2³¹ to 2³¹-1)
- Line numbers: positive integers
- No floating-point numbers supported
- Expression results must fit in `int` range
- Program execution should complete in reasonable time

## Test Data

Public test cases for local testing are provided at:
- `/workspace/data/007/`
- Contains 100 test cases: `trace00.txt` to `trace99.txt`

Each test file contains:
- Input commands (one per line)
- Expected output based on BASIC semantics

## Per-Testcase Resource Limits

- **Time Limit (per test case)**: 5000 ms
- **Memory Limit (per test case)**: 256 MiB
- **Disk Usage**: Disk access is not permitted

## Implementation Guidelines

### Repository Structure

```
├── CMakeLists.txt
├── README.md
├── Basic
│   ├── Basic.cpp              # Main interpreter loop
│   ├── Utils
│   │   ├── error.cpp          # Error handling
│   │   ├── error.hpp
│   │   ├── strlib.cpp         # String utilities
│   │   ├── strlib.hpp
│   │   ├── tokenScanner.cpp   # Tokenization
│   │   └── tokenScanner.hpp
│   ├── evalstate.cpp          # Variable state management
│   ├── evalstate.hpp
│   ├── exp.cpp                # Expression evaluation
│   ├── exp.hpp
│   ├── parser.cpp             # Expression parsing
│   ├── parser.hpp
│   ├── program.cpp            # Program storage
│   ├── program.hpp
│   ├── statement.cpp          # Statement execution
│   └── statement.hpp
├── StanfordCPPLib             # Stanford C++ library
├── Basic-Demo-64bit           # Reference implementation
├── Minimal-BASIC-Interpreter-2023.pdf  # Detailed specification
├── score.cpp                  # Local evaluation script
└── submit_acmoj/
    └── acmoj_client.py
```

### Files to Study

**DO NOT MODIFY** these files (report issues to TAs if bugs found):

```
Basic/Utils/
├── error.cpp/hpp       # Exception handling utilities
├── strlib.cpp/hpp      # String manipulation functions
├── tokenScanner.cpp/hpp # Lexical analysis tools
```

**Key Resources:**
- `tokenScanner.hpp` - Learn tokenization from StanfordCPPLib
- `strlib.hpp` - String processing utilities
- [StanfordCPPLib Documentation](https://cs.stanford.edu/people/eroberts/StanfordCPPLib/doc/index.html)
- `error.hpp` - Exception handling patterns

### Files to Implement

**PRIMARY** files where you need to add code:

```
Basic/
├── Basic.cpp          # Main interpreter loop and command processing
├── program.cpp/hpp    # Program storage and line management
└── statement.cpp/hpp  # Statement execution logic
```

**OPTIONAL** files you may modify:
- `evalstate.cpp/hpp` - Variable management
- `exp.cpp/hpp` - Expression evaluation
- `parser.cpp/hpp` - Expression parsing

### Implementation Steps

For detailed command specifications, refer to `Minimal-BASIC-Interpreter-2023.pdf`.

#### Step 1: Input Tokenization

**Purpose:** Split commands into tokens for easier processing

**Method:** Learn and use `Basic/Utils/tokenScanner` class

**Location:** `Basic/Basic.cpp`

**Example:**
```
Input:  "LET x = 10 + 20"
Tokens: ["LET", "x", "=", "10", "+", "20"]
```

#### Step 2: Syntax Validation

**Purpose:** Check if tokens form valid commands

**Method:** Verify token types and command structure

**Location:** `Basic/Basic.cpp`

**Examples:**
```
Valid:   "111"          → integer
Invalid: "1a1"          → malformed integer

Valid:   "END"          → valid END command
Invalid: "END Hahaha"   → invalid END command
```

#### Step 3: Program Storage

**Purpose:** Store numbered lines for program execution

**Method:** Design data structure (e.g., `map<int, Statement*>`)

**Location:** `Basic/program.cpp`, `Basic/program.hpp`

**Features:**
- Add/replace lines by line number
- Remove lines
- List all lines in order
- Clear all lines
- Execute program (RUN command)

#### Step 4: Statement Execution

**Purpose:** Execute BASIC commands

**Method:** 
- Design statement class hierarchy in `Basic/statement.hpp`
- Implement `execute()` method for each statement type
- Call statement execution from `Basic/Basic.cpp`

**Location:** `Basic/statement.cpp`, `Basic/statement.hpp`

**Architecture:**
```
Statement (base class)
├── RemStatement
├── LetStatement
├── PrintStatement
├── InputStatement
├── EndStatement
├── GotoStatement
└── IfStatement
```

**Expression Evaluation Flow:**
1. `Basic/parser.hpp` - Parse expression into syntax tree
2. `Basic/exp.hpp` - Evaluate expression tree
3. `Basic/evalstate.hpp` - Store/retrieve variable values

## Submission Requirements

### OJ Git Repository Compilation Process

For Git compilation, we will first clone the repository using a command similar to:
```bash
git clone <repo_url> . --depth 1 --recurse-submodules --shallow-submodules --no-local
```

Then we check if there is a `CMakeLists.txt` file. If it exists, we run (if not, a warning message will be displayed):
```bash
cmake .
```

Finally, we check if there is any of `GNUmakefile`/`makefile`/`Makefile` (if cmake was run previously, this will be the generated Makefile). If it exists, we run (if not, a warning message will be displayed):
```bash
make
```

After this process is complete, we will use the `code` file in the project root directory as the compilation result.

A `CMakeLists.txt` file is provided in the project. You can use or modify it as needed. The local environment has gcc-13 and g++-13 available.

### Git Configuration Requirements

**IMPORTANT**: You must create a `.gitignore` file in your project root directory to avoid OJ evaluation conflicts.

The `.gitignore` file should include at least the following entries:

```gitignore
CMakeFiles/
CMakeCache.txt
```

### Submission Guidelines

- The submitted code must be able to compile successfully through the above compilation process  
- The compiled executable file name must be `code`  
- The program needs to be able to read data from standard input and write results to standard output  
- Please ensure the code runs correctly within the given time and space limits  
- **You must use C++ or C language** to implement this assignment  

### Evaluation Notes

- The evaluator will test your program with 100 test cases
- Output must match the reference implementation exactly
- Exceeding time or memory limits will result in TLE/MLE verdict
- Robustness and memory leak checks will be performed
- No performance optimization required beyond passing tests

## Special Notes

### Undefined Behaviors

The following cases are **undefined behaviors**. You may handle them any way you choose. **Do NOT include these in test cases:**

1. **Commands without spaces:**
   - `PRINT-1+1` (PRINT followed by expression)
   - `PRINTA` (could be PRINT A or syntax error)
   - Line number without space before statement

2. **Whitespace variations:**
   - Leading/trailing spaces on lines
   - Missing spaces where required by specification

3. **LET statement spacing:**
   - `LETx=5` (no spaces around `=`)
   - `LET x=5` (missing space after LET)

4. **General rule:** Follow spacing requirements in specification exactly

### Variable Naming Rules

**Valid variable names:**
- One or more alphanumeric characters
- Can contain both letters and digits
- Examples: `x`, `var1`, `result2024`

**Invalid variable names:**
- Keywords: `REM`, `LET`, `PRINT`, `INPUT`, `END`, `GOTO`, `IF`, `THEN`, `RUN`, `LIST`, `CLEAR`, `QUIT`, `HELP`
- Names containing keywords (e.g., `PRINT1` is undefined behavior)
- Names with special characters (undefined behavior)

**Special case:** `10` as variable name
- `10 INPUT 10` is valid (second 10 is variable name)
- `20 PRINT 10` still prints numeric value `10` (not variable)
- Numeric strings are interpreted as numbers first, variables second

### Numeric Values

- **Range:** 32-bit signed integer (`-2^31` to `2^31-1`)
- **Overflow:** Not allowed in test cases
- **Floating point:** NOT supported
- **Input format:** Only integers, no decimals

### Edge Cases

- **Line number format:** `010 INPUT x` - Preserve leading zeros in LIST output
- **Empty line number:** `10` alone removes line 10
- **Division by zero:** Should be handled (undefined behavior in tests)
- **Invalid expressions:** Should produce error messages

---

## Local Testing

### Test Data Files

The `Test` directory contains 100 test cases that match the OJ test data exactly.

### Reference Implementation

The reference implementation `Basic-Demo-64bit` is provided for Linux systems.

**Running the reference:**
```bash
chmod +x Basic-Demo-64bit  # Grant execution permission if needed
./Basic-Demo-64bit         # Run the reference interpreter
```

### Evaluation Script

Use `score.cpp` to test your implementation locally:

```bash
g++ -o score score.cpp
./score -f                 # Run full test suite
./score -h                 # View help options
```

**Note:** If you modify the project structure, update file paths in `score.cpp` accordingly.

### Evaluation Method

Your program's output is compared against the reference implementation (differential testing) for identical inputs.

---

## FAQ

### Q: Are numeric overflows allowed in test cases?
**A:** No. All values must fit in `int` range (-2³¹ to 2³¹-1).

### Q: Is `10 INPUT 10` valid?
**A:** Yes. The second `10` is treated as a variable name. However, if we input `5`, then `20 PRINT 10` will still output `10` (the numeric constant), not the variable value. Purely numeric variable names are interpreted as numbers first, variables second.

### Q: What is the numeric range?
**A:** `int` range (-2³¹ to 2³¹-1). Floating-point numbers are NOT supported. Inputting floats is invalid.

### Q: Should `010 INPUT 10` preserve leading zeros in LIST output?
**A:** Yes. Preserve the original format. No need to modify line number formatting.

### Q: Is `PRINT-1+1` valid?
**A:** In principle yes, but commands immediately followed by expressions (like `PRINT-1+1` or `PRINTA`) are **undefined behavior**. You may interpret `PRINTA` as `PRINT A` or as a syntax error. **Do not include such cases in test data.**

### Q: Is spacing required around `=` in `LET` statements?
**A:** Yes. `LETx=5` and `LET x=5` are undefined behaviors. Test cases must include proper spacing: `LET x = 5`.

### Q: Must there be a space between line number and statement?
**A:** Yes. Missing spaces are undefined behavior. Do not include in test cases.

### Q: What about leading/trailing spaces?
**A:** Undefined behavior. Additionally, spaces required by the specification must be present, otherwise it's undefined behavior. Do not include such cases in test cases.

### Q: What are the variable naming rules?
**A:** Must consist of one or more alphanumeric characters (letters and digits) and cannot be a keyword (`REM`, `LET`, `PRINT`, `INPUT`, `END`, `GOTO`, `IF`, `THEN`, `RUN`, `LIST`, `CLEAR`, `QUIT`, `HELP`). Other characters are undefined behavior. Variable names containing keywords are also undefined behavior.

---

## Academic Integrity

If any violations are found during evaluation or code review (including but not limited to using unconventional methods to pass test cases), your final score may be significantly reduced or become **0 points**.

---

**For complete command specifications and detailed examples, please refer to `Minimal-BASIC-Interpreter-2023.pdf`.**
