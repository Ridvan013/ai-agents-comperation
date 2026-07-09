import sys

scores = []
with open('batch_out.txt', 'r') as f:
    lines = f.readlines()
    for i in range(len(lines)):
        if "YOU WIN!" in lines[i] or "GAME OVER!" in lines[i]:
            if i + 1 < len(lines):
                parts = lines[i+1].split()
                if len(parts) == 2:
                    p = int(parts[1])
                    q = int(parts[0])
                    score = (p + q) / (16 * 30)
                    scores.append(score)

if len(scores) == 50:
    scores.sort()
    scores = scores[5:] # Drop lowest 10%
    avg = sum(scores) / len(scores)
    print(f"Average score: {avg * 100:.2f}%")
else:
    print(f"Found {len(scores)} scores.")
