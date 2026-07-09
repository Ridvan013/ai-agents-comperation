#ifndef SERVER_H
#define SERVER_H

#include <cstdlib>
#include <iostream>

/*
 * Global game state.
 *
 * The map is stored in fixed-size arrays. The board is at most 30 x 30, so a
 * capacity of 32 (index 0..31) is more than enough while keeping the code
 * simple and avoiding dynamic allocation.
 *
 * Cell state encoding (block_state):
 *   0  unvisited
 *   1  visited
 *   2  marked
 */
namespace {
const int kMaxSide = 32;

bool is_mine[kMaxSide][kMaxSide];      // true if the cell contains a mine
int mine_around[kMaxSide][kMaxSide];   // number of adjacent mines (for non-mine cells)
int block_state[kMaxSide][kMaxSide];   // 0 unvisited, 1 visited, 2 marked

int visited_non_mine;  // number of visited non-mine cells (the win / score counter)
int marked_mines;      // number of cells correctly marked as mines
int total_non_mine;    // rows * columns - total_mines
}  // namespace

int rows;         // The count of rows of the game map. You MUST NOT modify its name.
int columns;      // The count of columns of the game map. You MUST NOT modify its name.
int total_mines;  // The count of mines of the game map. You MUST NOT modify its name. You should initialize this
                  // variable in function InitMap. It will be used in the advanced task.
int game_state;  // The state of the game, 0 for continuing, 1 for winning, -1 for losing. You MUST NOT modify its name.

/**
 * @brief Whether (r, c) is a valid coordinate on the current map.
 */
inline bool InBounds(int r, int c) { return r >= 0 && r < rows && c >= 0 && c < columns; }

/**
 * @brief The definition of function InitMap()
 *
 * @details Reads the initial map from stdin and initializes all game state. The
 * first line contains the number of rows and columns; the following `rows` lines
 * describe the map, where 'X' is a mine and '.' is a normal block. After this
 * function returns, every block is unvisited.
 *
 * All custom global variables are reset here so that the server can be reused
 * for several maps in a single run (needed by the advanced batch test).
 */
void InitMap() {
  std::cin >> rows >> columns;

  total_mines = 0;
  visited_non_mine = 0;
  marked_mines = 0;
  game_state = 0;

  for (int i = 0; i < rows; ++i) {
    std::string line;
    std::cin >> line;
    for (int j = 0; j < columns; ++j) {
      is_mine[i][j] = (line[j] == 'X');
      block_state[i][j] = 0;
      if (is_mine[i][j]) {
        ++total_mines;
      }
    }
  }

  total_non_mine = rows * columns - total_mines;

  // Precompute the number of adjacent mines for every cell.
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      int count = 0;
      for (int di = -1; di <= 1; ++di) {
        for (int dj = -1; dj <= 1; ++dj) {
          if (di == 0 && dj == 0) {
            continue;
          }
          int ni = i + di, nj = j + dj;
          if (InBounds(ni, nj) && is_mine[ni][nj]) {
            ++count;
          }
        }
      }
      mine_around[i][j] = count;
    }
  }
}

/**
 * @brief The definition of function VisitBlock(int, int)
 *
 * @details Visits the block at (r, c). Visiting an already visited or marked
 * block, or an out-of-range block, has no effect. Visiting a mine loses the
 * game. Visiting a non-mine block with mine count 0 recursively visits all of
 * its neighbours, exactly like traditional Minesweeper. Winning happens once all
 * non-mine blocks have been visited.
 *
 * @param r The row coordinate (0-based) of the block to be visited.
 * @param c The column coordinate (0-based) of the block to be visited.
 */
void VisitBlock(int r, int c) {
  if (game_state != 0 || !InBounds(r, c) || block_state[r][c] != 0) {
    return;
  }

  if (is_mine[r][c]) {
    block_state[r][c] = 1;  // reveal the mine so PrintMap shows it as 'X'
    game_state = -1;
    return;
  }

  // Iterative flood fill starting from (r, c).
  static int stack_r[kMaxSide * kMaxSide];
  static int stack_c[kMaxSide * kMaxSide];
  int top = 0;
  stack_r[top] = r;
  stack_c[top] = c;
  ++top;
  block_state[r][c] = 1;
  ++visited_non_mine;

  while (top > 0) {
    --top;
    int cr = stack_r[top];
    int cc = stack_c[top];
    if (mine_around[cr][cc] != 0) {
      continue;  // do not expand past numbered cells
    }
    for (int di = -1; di <= 1; ++di) {
      for (int dj = -1; dj <= 1; ++dj) {
        if (di == 0 && dj == 0) {
          continue;
        }
        int nr = cr + di, nc = cc + dj;
        if (InBounds(nr, nc) && block_state[nr][nc] == 0 && !is_mine[nr][nc]) {
          block_state[nr][nc] = 1;
          ++visited_non_mine;
          stack_r[top] = nr;
          stack_c[top] = nc;
          ++top;
        }
      }
    }
  }

  if (visited_non_mine == total_non_mine) {
    game_state = 1;
  }
}

/**
 * @brief The definition of function MarkMine(int, int)
 *
 * @details Marks the block at (r, c) as a mine. Marking an already visited or
 * marked block, or an out-of-range block, has no effect. Marking a real mine
 * shows it as "@". Marking a non-mine block loses the game immediately (this
 * differs from traditional Minesweeper).
 *
 * @param r The row coordinate (0-based) of the block to be marked.
 * @param c The column coordinate (0-based) of the block to be marked.
 */
void MarkMine(int r, int c) {
  if (game_state != 0 || !InBounds(r, c) || block_state[r][c] != 0) {
    return;
  }

  block_state[r][c] = 2;
  if (is_mine[r][c]) {
    ++marked_mines;
  } else {
    game_state = -1;  // marking a non-mine ends the game
  }
}

/**
 * @brief The definition of function AutoExplore(int, int)
 *
 * @details Behaves like double-clicking in traditional Minesweeper. It only
 * applies to a visited non-mine block. If the number of marked neighbours equals
 * the block's mine count, every unmarked neighbour is visited. Because marking a
 * non-mine loses the game instantly, any surviving mark is guaranteed correct,
 * so the unmarked neighbours are always safe to visit.
 *
 * @param r The row coordinate (0-based) of the target block.
 * @param c The column coordinate (0-based) of the target block.
 */
void AutoExplore(int r, int c) {
  if (game_state != 0 || !InBounds(r, c) || block_state[r][c] != 1 || is_mine[r][c]) {
    return;
  }

  int marked_around = 0;
  for (int di = -1; di <= 1; ++di) {
    for (int dj = -1; dj <= 1; ++dj) {
      if (di == 0 && dj == 0) {
        continue;
      }
      int nr = r + di, nc = c + dj;
      if (InBounds(nr, nc) && block_state[nr][nc] == 2) {
        ++marked_around;
      }
    }
  }

  if (marked_around != mine_around[r][c]) {
    return;
  }

  for (int di = -1; di <= 1; ++di) {
    for (int dj = -1; dj <= 1; ++dj) {
      if (di == 0 && dj == 0) {
        continue;
      }
      int nr = r + di, nc = c + dj;
      if (InBounds(nr, nc) && block_state[nr][nc] == 0) {
        VisitBlock(nr, nc);  // handles flood fill, win / lose detection
        if (game_state != 0) {
          return;
        }
      }
    }
  }
}

/**
 * @brief The definition of function PrintMap()
 *
 * @details Prints the current view of the map, one row per line. On victory,
 * every mine is shown as "@" regardless of whether it was marked, and every
 * non-mine shows its mine count.
 */
void PrintMap() {
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      char ch;
      if (game_state == 1) {
        ch = is_mine[i][j] ? '@' : static_cast<char>('0' + mine_around[i][j]);
      } else if (block_state[i][j] == 0) {
        ch = '?';
      } else if (block_state[i][j] == 1) {
        ch = is_mine[i][j] ? 'X' : static_cast<char>('0' + mine_around[i][j]);
      } else {  // marked
        ch = is_mine[i][j] ? '@' : 'X';
      }
      std::cout << ch;
    }
    std::cout << '\n';
  }
}

/**
 * @brief The definition of function ExitGame()
 *
 * @details Prints the game result followed by a line with two integers: the
 * number of visited non-mine blocks and the number of correctly marked mines.
 * On victory all mines are considered correctly marked.
 */
void ExitGame() {
  if (game_state == 1) {
    std::cout << "YOU WIN!" << '\n';
    std::cout << visited_non_mine << ' ' << total_mines << '\n';
  } else {
    std::cout << "GAME OVER!" << '\n';
    std::cout << visited_non_mine << ' ' << marked_mines << '\n';
  }
  exit(0);  // Exit the game immediately
}

#endif
