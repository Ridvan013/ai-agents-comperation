# Problem 001 - A+B Problem

**ACMOJ Problem ID**: 1000

## Table of Contents

- [Problem 001 - A+B Problem](#problem-001---ab-problem)
  - [Table of Contents](#table-of-contents)
  - [Problem Description](#problem-description)
  - [Input Format](#input-format)
  - [Output Format](#output-format)
  - [Sample](#sample)
    - [Input](#input)
    - [Output](#output)
  - [Per-Testcase Resource Limits](#per-testcase-resource-limits)
  - [Test Data](#test-data)
  - [Submission Requirements](#submission-requirements)
    - [OJ Git Repository Compilation Process](#oj-git-repository-compilation-process)
    - [Git Configuration Requirements](#git-configuration-requirements)
    - [Submission Guidelines](#submission-guidelines)
    - [Evaluation Notes](#evaluation-notes)
    - [Academic Integrity](#academic-integrity)

## Problem Description

Input two integers and output their sum.

## Input Format

A single line containing two integers, A and B, separated by a space.

## Output Format

A single number, the sum of A + B.

## Sample

### Input

```
1 1
```

### Output

```
2
```

## Per-Testcase Resource Limits

- **Time Limit (per test case)**: 1000 ms  
- **Memory Limit (per test case)**: 256 MiB  
- **Disk Usage**: No disk usage is permitted.

## Test Data

The test data for this problem is located at `/workspace/data/001/data_test/`.

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

- The evaluation system will test your program using the provided test data  
- The program output must exactly match the expected output (including format)  
- Exceeding time or memory limits will be judged as the corresponding error type  
- Please pay attention to the overall time performance of your code and the time complexity of each part of your algorithm.

### Academic Integrity

If any violations are found during evaluation or code review (including but not limited to using unconventional methods to pass test cases), your final score may be significantly reduced or become **0 points**.