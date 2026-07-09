# mov Language Problems

**ACMOJ Problem IDs**: 2276, 2277, 2278, 2279, 2280, 2281, 2282, 2283

## Table of Contents

- [mov Language Problems](#mov-language-problems)
  - [Table of Contents](#table-of-contents)
  - [Introduction to "mov language"](#introduction-to-mov-language)
  - [Assignment Descriptions](#assignment-descriptions)
  - [Problems](#problems)
  - [Submission Requirements](#submission-requirements)
    - [Submission Guidelines](#submission-guidelines)
    - [Evaluation Notes](#evaluation-notes)

## Introduction to "mov language"

Welcome to a world of only mov instructions!

As we all know, x86 assembly has so much redundancy that even with only the data transfer instruction mov, it's still Turing-complete. We believe that programming languages ​​should be as concise as possible, so we designed a language with only mov instructions and asked you to use it to write some simple programs.

### Memory and registers

`mem[]` in the MOV language is a one-dimensional array of size **512** (0-based).  
Each cell is **8 bits** (values 0–255, can be treated as `unsigned char`).

There are **26** 8-bit registers, represented by the uppercase letters **A**–**Z**.

### Instruction format

MOV has a single instruction form: `dst<src`. This means: assign the value at `src` to `dst` (i.e., `dst = src`).

Both `src` and `dst` accept the following addressing modes.

### Addressing Modes


| Mode   | Meaning |
|--------|---------|
| `R`    | The value stored in a register `R`. Example: `A`. |
| `[R]`  | The value at the memory address given by register `R`, i.e. `mem[R]`. Example: `[A]`. |
| `[I]`  | The value at the memory address given by an immediate integer `I`. Example: `[102]`. |
| `[R+I]`| The value at the memory address `R + I` (register plus immediate). Example: `[C+4]`. |
| `[R+R]`| The value at the memory address `R1 + R2` (sum of two registers). Example: `[E+F]`. |

**Example:** `[A]<[A+1]` means `mem[A] = mem[A+1]`.

`src` can also be an immediate constant.  
**Example:** `A<0` sets register `A` to `0`.

### Special-purpose registers

Among the 26 registers, some have special roles:

| Register | Purpose |
|----------|---------|
| `I`      | Input. Example: `[R]<I` means read one byte from input into the specified memory location. |
| `O`      | Output. Example: `O<R` means output the value of register `R` (interpreted as the corresponding ASCII character). |
| `Z`      | Halt. If `Z` is assigned a non-zero value (e.g., `Z<1`), the program halts immediately. |

**Important:** Do not use these special registers for other purposes (such as addressing or arithmetic). All other registers may be used freely.

### Program Execution

The program starts with the first instruction and executes sequentially line by line. When it reaches the last instruction, if it has not stopped, it will return to the first instruction and continue execution. In short, all your instructions will be executed in a loop until it stops.

## Assignment Descriptions

We provide a mov language interpreter(`/interpreter/interpreter`). We also provide the interpreter source code(``/interpreter/interpreter.c``). The first argument to the interpreter is the mov program you wrote, and the second argument is the program input. For example, you can run your program locally by typing `./interpreter/interpreter code/2277.mv input/2277.in`

### Precautions

- The code length cannot exceed 65536. (excluding comments and blank lines)
- You need to ensure that your code does not access out of bounds, which will cause a Bad Problem error.
- Each correct answer to a question will earn you 50% of the score, and the remaining 50% will be your performance score. The performance score is calculated as `50% * min{1, #std_len / #len}` where `#std_len` is the length of the standard answer, and `#len` is the length of your answer.

### Score composition

| Problem |  ID  |  Score  |
|---------|------|---------|
| Hello   | 2276 | 1000  |
| if else | 2277 | 5000  |
| i++     | 2278 | 5000  |
| echo    | 2279 | 20000  |
| printf  | 2280 | 50000  |
| A+B     | 2281 | 80000  |
| sort    | 2282 | 80000  |
| Hanoi   | 2283 | 120000  |

## Problems

### 2276 - Hello World

#### Description

This prints a line of text, "Hello World!".

Note that there are no quotes or carriage returns around the last line.

#### Example

output:

```
Hello World!
```

### 2277 - if else

#### Description

You need to read two ASCII characters and determine whether they are the same.

If they are the same, output 1; otherwise, output 0.

#### Format

- **Input:** Two ASCII characters. No space between them.
- **Output:** A single **character**: '1' or '0'.

#### Example

- Input: 
```
XY
```  
- Output: 
```
0
```

### 2278 - i++

#### Description

You need to read the ASCII characters '0' to '9' and output them as the result of the number increment. No carry is required, you can think of it as addition in the sense of modulo 10.

#### Format

- **Input:** A single ASCII character from '0' to '9'.
- **Output:** A single ASCII character from '0' to '9'.

#### Example

- Input: 
```
9
```
- Output: 
```
0
```

### 2279 - echo

#### Description

The input for this question is any number of characters, and you must output exactly the same content as the input.

Hint: After the input stream ends, the input is considered to be all 0.

#### Format

- **Input:** Any number of ASCII characters.
- **Output:** The same number of ASCII characters as the input.

#### Example

- Input: 
```
Darksharpness 114514
```
- Output: 
```
Darksharpness 114514
```

### 2280 - printf

#### Description

You need to read an ASCII character and output its decimal ASCII code.

Hint: O<0 is treated as not outputting any character.

#### Format

- **Input:** A single ASCII character.
- **Output:** A number of up to three digits, representing the ASCII code of the character

#### Example

- Input: 
```
9
```
- Output:
```
57
```

### 2281 - A+B

#### Description

You need to read two tens-digit numbers (represented by 10 ASCII characters between '0' and '9', connected by '+') and output them as the sum of the numbers.

#### Format

- **Input:** Two strings representing the numbers, separated by a '+' character.
- **Output:** A string representing the sum of the two numbers.

#### Example

- Input: 
```
1234567890+9876543210
```
- Output:
```
11111111100
```

### 2282 - sort

#### Description

You need to read in 5 ASCII characters between '0' and '9' and output them in ascending order.

#### Format

- **Input:** A string of 5 ASCII characters between '0' and '9'.
- **Output:** The same 5 characters sorted in ascending order.

#### Example

- Input:
```
54321
```
- Output:
```
12345
```

### 2283 - Hanoi

#### Description

There are three pillars labeled A, B, and C. Pillar A holds up to 10 disks. Each disk is slightly smaller than the one below it. You must use pillar B to move all the disks from pillar A to pillar C, ensuring that no larger disks are placed on top of any smaller disks.

This problem requires you to read in a character between '1' and ':' (corresponding to ASCII codes 49-58, with ':' representing 10) and output the optimal solution to the Tower of Hanoi with the corresponding number of floors.

**Note**: In this problem, if the number of valid lines of code exceeds 1024, you will receive a maximum score of 50%.

#### Format

- **Input:** A single character between '1' and ':'.
- **Output:** Several lines of operations in the format of `A->B`, note that a line break is required after the last line.

#### Example

- Input:
```
3
```
- Output:
```
A->C
A->B
C->B
A->C
B->A
B->C
A->C
```

## Submission Requirements

### Submission Guidelines

- For ACMOJ problems 2276 to 2283, submit only the `.mv` file containing your mov language code.
- Ensure your implementation meets the requirements.

### Evaluation Notes

- The evaluator will test your program with private OJ data in addition to local samples.
- Output must match the required format exactly.
- Exceeding time or memory limits will result in the corresponding verdict.