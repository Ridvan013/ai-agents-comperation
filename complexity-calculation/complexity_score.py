"""Specification-based complexity score for the twelve ProjDevBench problems.

Usage:
    python3 complexity_score.py <projdevbench_repo> [output.csv]

<projdevbench_repo> is a checkout of the ProjDevBench repository; the specification of
problem NNN is read from problem/NNN/README.md (the same file that was given to the tools).
The rules are described in COMPLEXITY_RULES.md.
"""
import csv
import os
import re
import sys

PROBLEMS = ["001", "002", "004", "005", "006", "009", "013", "014", "017", "018", "019", "020"]
FROM_SCRATCH = {"004", "017"}          # no starter code is provided
# Sections unrelated to the programming task (matched case-insensitively in the heading).
EXCLUDED = ["table of contents", "grade composition", "grading", "submission", "academic integrity",
            "git configuration", "oj git", "evaluation notes", "per-testcase resource limits",
            "allowed libraries", "performance scoring"]
INDICATORS = ["spec_words", "interface_items", "subtasks", "from_scratch"]


def task_text(path):
    """Return the lines of the README that describe the task, with excluded sections removed.
    Each item is (line, inside_code_block); also returns the kept headings and removed sections."""
    lines = open(path, encoding="utf-8").read().replace("\r\n", "\n").split("\n")
    in_code, skip_level = False, None
    kept, headings, removed = [], [], []
    for line in lines:
        if line.strip().startswith("```"):
            in_code = not in_code
            if skip_level is None:
                kept.append((line, True))
            continue
        heading = None if in_code else re.match(r"^(#+)\s+(.*)$", line)
        if heading:
            level, title = len(heading.group(1)), heading.group(2).strip()
            if skip_level is not None and level <= skip_level:
                skip_level = None
            if skip_level is None and any(word in title.lower() for word in EXCLUDED):
                skip_level = level
                removed.append(title)
                continue
            if skip_level is None:
                headings.append(title)
                kept.append((line, False))
            continue
        if skip_level is None:
            kept.append((line, in_code))
    return kept, headings, removed


def average_ranks(values):
    """Ascending ranks starting at 1; tied values receive their average rank."""
    order = sorted(range(len(values)), key=lambda i: values[i])
    ranks = [0.0] * len(values)
    i = 0
    while i < len(order):
        j = i
        while j + 1 < len(order) and values[order[j + 1]] == values[order[i]]:
            j += 1
        for k in range(i, j + 1):
            ranks[order[k]] = (i + j) / 2 + 1
        i = j + 1
    return ranks


def main():
    repo = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "complexity_scores.csv"
    rows = []
    for p in PROBLEMS:
        kept, headings, removed = task_text(os.path.join(repo, "problem", p, "README.md"))
        rows.append({
            "problem": p,
            # (1) number of words in the remaining text, code examples included
            "spec_words": sum(len(text.split()) for text, _ in kept),
            # (2) distinct terms written in code format (`...`) outside code blocks
            "interface_items": len({term for text, code in kept if not code
                                    for term in re.findall(r"`([^`\n]+)`", text)}),
            # (3) headings that define a subtask, at least one
            "subtasks": max(1, sum(1 for h in headings if "subtask" in h.lower())),
            # (4) 1 = developed from scratch, 0 = starter code provided
            "from_scratch": int(p in FROM_SCRATCH),
            "removed_sections": "; ".join(removed),
        })
    for key in INDICATORS:
        for row, r in zip(rows, average_ranks([row[key] for row in rows])):
            row["rank_" + key] = r
    for row in rows:
        row["complexity_score"] = round(sum(row["rank_" + k] for k in INDICATORS) / len(INDICATORS), 2)
    rows.sort(key=lambda row: -row["complexity_score"])
    with open(out, "w", newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)
    for row in rows:
        print(f"{row['problem']}  words={row['spec_words']:5d}  interface={row['interface_items']:3d}  "
              f"subtasks={row['subtasks']}  from_scratch={row['from_scratch']}  score={row['complexity_score']:5.2f}")


if __name__ == "__main__":
    main()
