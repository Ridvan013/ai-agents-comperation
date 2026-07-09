#ifndef SERVER_H
#define SERVER_H

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

/*
 * You may need to define some global variables for the information of the game map here.
 * Although we don't encourage to use global variables in real cpp projects, you may have to use them because the use of
 * class is not taught yet. However, if you are member of A-class or have learnt the use of cpp class, member functions,
 * etc., you're free to modify this structure.
 */
int rows;         // The count of rows of the game map. You MUST NOT modify its name.
int columns;      // The count of columns of the game map. You MUST NOT modify its name.
int total_mines;  // The count of mines of the game map. You MUST NOT modify its name. You should initialize this
                  // variable in function InitMap. It will be used in the advanced task.
int game_state;  // The state of the game, 0 for continuing, 1 for winning, -1 for losing. You MUST NOT modify its name.

// ---------------------------------------------------------------------------
// Internal server state (implementation detail, names are free to choose).
// ---------------------------------------------------------------------------
namespace server_detail {

constexpr int kMaxSize = 32;

// Cell display / logical state.
enum CellState : char {
  kUnvisited = 0,  // Neither visited nor marked.
  kVisited = 1,    // Visited (revealed) by the player.
  kMarked = 2,     // Marked as a mine by the player.
};

bool is_mine[kMaxSize][kMaxSize];    // Whether a grid holds a mine.
int adjacent[kMaxSize][kMaxSize];    // Number of mines in the 8 surrounding grids.
CellState state[kMaxSize][kMaxSize]; // Current state of each grid.

int visited_non_mine;  // Number of visited non-mine grids so far.
int correct_marks;     // Number of correctly marked mines so far.
int non_mine_total;    // Total number of non-mine grids on the map.

// When true, ExitGame() does not terminate the process. Used by the local
// benchmark harness that plays many games in one run. The provided basic.cpp
// and advanced.cpp never touch it, so the default (false) preserves the
// required single-run behaviour.
bool suppress_exit = false;

inline bool InBounds(int r, int c) { return r >= 0 && r < rows && c >= 0 && c < columns; }

// Recompute the adjacent-mine count for every grid.
inline void ComputeAdjacency() {
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < columns; ++c) {
      int count = 0;
      for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
          if (dr == 0 && dc == 0) continue;
          int nr = r + dr;
          int nc = c + dc;
          if (InBounds(nr, nc) && is_mine[nr][nc]) ++count;
        }
      }
      adjacent[r][c] = count;
    }
  }
}

// Flood-fill visit starting from a guaranteed non-mine, unvisited grid.
// Visiting a grid whose adjacent count is 0 recursively reveals its neighbours.
inline void FloodVisit(int r, int c) {
  std::vector<std::pair<int, int>> stack;
  stack.emplace_back(r, c);
  while (!stack.empty()) {
    auto cur = stack.back();
    stack.pop_back();
    int cr = cur.first;
    int cc = cur.second;
    if (state[cr][cc] != kUnvisited) continue;
    state[cr][cc] = kVisited;
    ++visited_non_mine;
    if (adjacent[cr][cc] != 0) continue;
    for (int dr = -1; dr <= 1; ++dr) {
      for (int dc = -1; dc <= 1; ++dc) {
        if (dr == 0 && dc == 0) continue;
        int nr = cr + dr;
        int nc = cc + dc;
        if (InBounds(nr, nc) && state[nr][nc] == kUnvisited && !is_mine[nr][nc]) {
          stack.emplace_back(nr, nc);
        }
      }
    }
  }
}

// Update game_state to a win if every non-mine grid has been visited.
inline void CheckWin() {
  if (game_state == 0 && visited_non_mine == non_mine_total) {
    game_state = 1;
  }
}

}  // namespace server_detail

/**
 * @brief The definition of function InitMap()
 *
 * @details This function is designed to read the initial map from stdin. For example, if there is a 3 * 3 map in which
 * mines are located at (0, 1) and (1, 2) (0-based), the stdin would be
 *     3 3
 *     .X.
 *     ...
 *     ..X
 * where X stands for a mine block and . stands for a normal block. After executing this function, your game map
 * would be initialized, with all the blocks unvisited.
 */
void InitMap() {
  using namespace server_detail;
  std::cin >> rows >> columns;
  total_mines = 0;
  visited_non_mine = 0;
  correct_marks = 0;
  non_mine_total = 0;
  game_state = 0;
  for (int r = 0; r < rows; ++r) {
    std::string line;
    std::cin >> line;
    for (int c = 0; c < columns; ++c) {
      bool mine = (line[c] == 'X');
      is_mine[r][c] = mine;
      state[r][c] = kUnvisited;
      if (mine) {
        ++total_mines;
      } else {
        ++non_mine_total;
      }
    }
  }
  ComputeAdjacency();
}

/**
 * @brief The definition of function VisitBlock(int, int)
 *
 * @details This function is designed to visit a block in the game map. See README / header docs for the rules.
 *
 * @param r The row coordinate (0-based) of the block to be visited.
 * @param c The column coordinate (0-based) of the block to be visited.
 *
 * @note Edits game_state: 0 continue, 1 win, -1 lose. Does nothing for invalid/no-op operations.
 */
void VisitBlock(int r, int c) {
  using namespace server_detail;
  if (game_state != 0 || !InBounds(r, c)) return;
  if (state[r][c] != kUnvisited) return;  // Already visited or marked: no effect.
  if (is_mine[r][c]) {
    state[r][c] = kVisited;  // Reveal the mine that was stepped on.
    game_state = -1;
    return;
  }
  FloodVisit(r, c);
  CheckWin();
}

/**
 * @brief The definition of function MarkMine(int, int)
 *
 * @details Marks a grid as a mine. Marking a correct mine shows "@"; marking a non-mine ends the game immediately.
 *
 * @param r The row coordinate (0-based) of the block to be marked.
 * @param c The column coordinate (0-based) of the block to be marked.
 *
 * @note Edits game_state as described above. Does nothing for invalid/no-op operations.
 */
void MarkMine(int r, int c) {
  using namespace server_detail;
  if (game_state != 0 || !InBounds(r, c)) return;
  if (state[r][c] != kUnvisited) return;  // Already visited or marked: no effect.
  state[r][c] = kMarked;
  if (is_mine[r][c]) {
    ++correct_marks;
  } else {
    game_state = -1;  // Marking a non-mine grid loses immediately.
  }
}

/**
 * @brief The definition of function AutoExplore(int, int)
 *
 * @details Similar to double-clicking in traditional Minesweeper. Only valid on a visited non-mine grid. If the number
 * of marked grids around it equals its mine count, every remaining unmarked neighbour (all non-mine under valid play)
 * is visited.
 */
void AutoExplore(int r, int c) {
  using namespace server_detail;
  if (game_state != 0 || !InBounds(r, c)) return;
  if (state[r][c] != kVisited || is_mine[r][c]) return;  // Must target a visited number grid.

  int marked_around = 0;
  for (int dr = -1; dr <= 1; ++dr) {
    for (int dc = -1; dc <= 1; ++dc) {
      if (dr == 0 && dc == 0) continue;
      int nr = r + dr;
      int nc = c + dc;
      if (InBounds(nr, nc) && state[nr][nc] == kMarked) ++marked_around;
    }
  }
  if (marked_around != adjacent[r][c]) return;  // Not enough information to auto explore.

  for (int dr = -1; dr <= 1; ++dr) {
    for (int dc = -1; dc <= 1; ++dc) {
      if (dr == 0 && dc == 0) continue;
      int nr = r + dr;
      int nc = c + dc;
      if (!InBounds(nr, nc) || state[nr][nc] != kUnvisited) continue;
      if (is_mine[nr][nc]) {
        // Should not happen under valid play, but keep rules consistent: stepping on a mine loses.
        state[nr][nc] = kVisited;
        game_state = -1;
        return;
      }
      FloodVisit(nr, nc);
    }
  }
  CheckWin();
}

/**
 * @brief The definition of function ExitGame()
 *
 * @details Outputs the result line and the "visit_count marked_mine_count" line, then terminates the process.
 *
 * @note If the player wins, ALL mines are considered correctly marked.
 */
void ExitGame() {
  using namespace server_detail;
  std::cout << (game_state == 1 ? "YOU WIN!" : "GAME OVER!") << std::endl;
  int marked = (game_state == 1) ? total_mines : correct_marks;
  std::cout << visited_non_mine << " " << marked << std::endl;
  if (!suppress_exit) {
    exit(0);  // Exit the game immediately
  }
}

/**
 * @brief The definition of function PrintMap()
 *
 * @details Prints the game map to stdout following the display rules in README.
 *   - Unvisited & unmarked: '?'
 *   - Visited non-mine: its adjacent mine count digit
 *   - Visited mine (only on a losing step): 'X'
 *   - Marked mine: '@'; marked non-mine (only on a losing step): 'X'
 *   - On victory: every mine is printed as '@' regardless of its marked status.
 *
 * @note Uses std::cout so it can be captured by the advanced task harness.
 */
void PrintMap() {
  using namespace server_detail;
  std::string out;
  out.reserve((columns + 1) * rows);
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < columns; ++c) {
      char ch;
      if (game_state == 1) {
        ch = is_mine[r][c] ? '@' : static_cast<char>('0' + adjacent[r][c]);
      } else if (state[r][c] == kUnvisited) {
        ch = '?';
      } else if (state[r][c] == kMarked) {
        ch = is_mine[r][c] ? '@' : 'X';
      } else {  // kVisited
        ch = is_mine[r][c] ? 'X' : static_cast<char>('0' + adjacent[r][c]);
      }
      out.push_back(ch);
    }
    out.push_back('\n');
  }
  std::cout << out;
}

#endif
