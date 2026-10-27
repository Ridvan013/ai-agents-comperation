# Specification-based complexity score

The rules were fixed before the score was computed. Only the problem specifications are used;
nothing derived from the tool runs or test results is used.

## Source
Each problem's `README.md` from the ProjDevBench repository (`problem/<id>/README.md`),
identical to the specification given to the tools.

## Preprocessing
- Lines inside fenced code blocks (```) are content, never headings.
- A heading is a line starting with `#` outside a code block; its level is the number of `#`.
- A section runs from its heading to the next heading of the same or higher level.
- Sections unrelated to the programming task are removed together with their subsections.
  A section is removed if its heading contains (case-insensitive) any of: "table of contents",
  "grade composition", "grading", "submission", "academic integrity", "git configuration",
  "oj git", "evaluation notes", "per-testcase resource limits", "allowed libraries",
  "performance scoring".

## Indicators (higher = more complex)
1. Specification length: number of words (whitespace-separated tokens) in the remaining text,
   code examples included.
2. Interface size: number of distinct terms written in code format (`...`) in the remaining
   text, outside code blocks.
3. Subtasks: number of headings in the remaining text that contain "subtask"; minimum 1.
4. Development setting: 1 if the project is developed from scratch (no starter code), else 0.

## Score
- The twelve problems are ranked in ascending order on each indicator; tied values receive
  their average rank.
- Complexity score = mean of the four ranks (equal weights).
