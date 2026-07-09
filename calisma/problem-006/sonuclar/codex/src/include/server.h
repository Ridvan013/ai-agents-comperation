#ifndef SERVER_H
#define SERVER_H

#include <array>
#include <cstdlib>
#include <iostream>
#include <queue>
#include <string>

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

namespace server_detail {

constexpr int kMaxSize = 35;
constexpr std::array<int, 8> kDr{-1, -1, -1, 0, 0, 1, 1, 1};
constexpr std::array<int, 8> kDc{-1, 0, 1, -1, 1, -1, 0, 1};

bool has_mine[kMaxSize][kMaxSize];
bool visited[kMaxSize][kMaxSize];
bool marked[kMaxSize][kMaxSize];
int adjacent_mines[kMaxSize][kMaxSize];
int visited_safe_cells;
int correctly_marked_mines;

bool InBounds(int r, int c) {
  return 0 <= r && r < rows && 0 <= c && c < columns;
}

void ResetState() {
  total_mines = 0;
  game_state = 0;
  visited_safe_cells = 0;
  correctly_marked_mines = 0;
  for (int r = 0; r < kMaxSize; ++r) {
    for (int c = 0; c < kMaxSize; ++c) {
      has_mine[r][c] = false;
      visited[r][c] = false;
      marked[r][c] = false;
      adjacent_mines[r][c] = 0;
    }
  }
}

void UpdateWinState() {
  if (visited_safe_cells == rows * columns - total_mines) {
    game_state = 1;
  }
}

void RevealRegion(int start_r, int start_c) {
  if (!InBounds(start_r, start_c) || has_mine[start_r][start_c] || visited[start_r][start_c] || marked[start_r][start_c]) {
    return;
  }

  std::queue<std::pair<int, int>> bfs;
  bfs.push(std::make_pair(start_r, start_c));
  while (!bfs.empty()) {
    const std::pair<int, int> current = bfs.front();
    bfs.pop();
    const int r = current.first;
    const int c = current.second;
    if (!InBounds(r, c) || has_mine[r][c] || visited[r][c] || marked[r][c]) {
      continue;
    }

    visited[r][c] = true;
    ++visited_safe_cells;
    if (adjacent_mines[r][c] != 0) {
      continue;
    }

    for (int i = 0; i < 8; ++i) {
      bfs.push(std::make_pair(r + kDr[i], c + kDc[i]));
    }
  }
}

int CountMarkedAround(int r, int c) {
  int count = 0;
  for (int i = 0; i < 8; ++i) {
    const int nr = r + kDr[i];
    const int nc = c + kDc[i];
    if (InBounds(nr, nc) && marked[nr][nc]) {
      ++count;
    }
  }
  return count;
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
  std::cin >> rows >> columns;
  server_detail::ResetState();
  for (int r = 0; r < rows; ++r) {
    std::string line;
    std::cin >> line;
    for (int c = 0; c < columns; ++c) {
      server_detail::has_mine[r][c] = (line[c] == 'X');
      total_mines += server_detail::has_mine[r][c] ? 1 : 0;
    }
  }

  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < columns; ++c) {
      if (server_detail::has_mine[r][c]) {
        continue;
      }
      int count = 0;
      for (int i = 0; i < 8; ++i) {
        const int nr = r + server_detail::kDr[i];
        const int nc = c + server_detail::kDc[i];
        if (server_detail::InBounds(nr, nc) && server_detail::has_mine[nr][nc]) {
          ++count;
        }
      }
      server_detail::adjacent_mines[r][c] = count;
    }
  }
}

/**
 * @brief The definition of function VisitBlock(int, int)
 *
 * @details This function is designed to visit a block in the game map. We take the 3 * 3 game map above as an example.
 * At the beginning, if you call VisitBlock(0, 0), the return value would be 0 (game continues), and the game map would
 * be
 *     1??
 *     ???
 *     ???
 * If you call VisitBlock(0, 1) after that, the return value would be -1 (game ends and the players loses) , and the
 * game map would be
 *     1X?
 *     ???
 *     ???
 * If you call VisitBlock(0, 2), VisitBlock(2, 0), VisitBlock(1, 2) instead, the return value of the last operation
 * would be 1 (game ends and the player wins), and the game map would be
 *     1@1
 *     122
 *     01@
 *
 * @param r The row coordinate (0-based) of the block to be visited.
 * @param c The column coordinate (0-based) of the block to be visited.
 *
 * @note You should edit the value of game_state in this function. Precisely, edit it to
 *    0  if the game continues after visit that block, or that block has already been visited before.
 *    1  if the game ends and the player wins.
 *    -1 if the game ends and the player loses.
 *
 * @note For invalid operation, you should not do anything.
 */
void VisitBlock(int r, int c) {
  if (game_state != 0 || !server_detail::InBounds(r, c) || server_detail::visited[r][c] || server_detail::marked[r][c]) {
    return;
  }
  if (server_detail::has_mine[r][c]) {
    server_detail::visited[r][c] = true;
    game_state = -1;
    return;
  }

  server_detail::RevealRegion(r, c);
  server_detail::UpdateWinState();
}

/**
 * @brief The definition of function MarkMine(int, int)
 *
 * @details This function is designed to mark a mine in the game map.
 * If the block being marked is a mine, show it as "@".
 * If the block being marked isn't a mine, END THE GAME immediately. (NOTE: This is not the same rule as the real
 * game) And you don't need to
 *
 * For example, if we use the same map as before, and the current state is:
 *     1?1
 *     ???
 *     ???
 * If you call MarkMine(0, 1), you marked the right mine. Then the resulting game map is:
 *     1@1
 *     ???
 *     ???
 * If you call MarkMine(1, 0), you marked the wrong mine(There's no mine in grid (1, 0)).
 * The game_state would be -1 and game ends immediately. The game map would be:
 *     1?1
 *     X??
 *     ???
 * This is different from the Minesweeper you've played. You should beware of that.
 *
 * @param r The row coordinate (0-based) of the block to be marked.
 * @param c The column coordinate (0-based) of the block to be marked.
 *
 * @note You should edit the value of game_state in this function. Precisely, edit it to
 *    0  if the game continues after visit that block, or that block has already been visited before.
 *    1  if the game ends and the player wins.
 *    -1 if the game ends and the player loses.
 *
 * @note For invalid operation, you should not do anything.
 */
void MarkMine(int r, int c) {
  if (game_state != 0 || !server_detail::InBounds(r, c) || server_detail::visited[r][c] || server_detail::marked[r][c]) {
    return;
  }

  server_detail::marked[r][c] = true;
  if (!server_detail::has_mine[r][c]) {
    game_state = -1;
    return;
  }

  ++server_detail::correctly_marked_mines;
  server_detail::UpdateWinState();
}

/**
 * @brief The definition of function AutoExplore(int, int)
 *
 * @details This function is designed to auto-visit adjacent blocks of a certain block.
 * See README.md for more information
 *
 * For example, if we use the same map as before, and the current map is:
 *     ?@?
 *     ?2?
 *     ??@
 * Then auto explore is available only for block (1, 1). If you call AutoExplore(1, 1), the resulting map will be:
 *     1@1
 *     122
 *     01@
 * And the game ends (and player wins).
 */
void AutoExplore(int r, int c) {
  if (game_state != 0 || !server_detail::InBounds(r, c) || !server_detail::visited[r][c] || server_detail::has_mine[r][c]) {
    return;
  }
  if (server_detail::CountMarkedAround(r, c) != server_detail::adjacent_mines[r][c]) {
    return;
  }

  for (int i = 0; i < 8; ++i) {
    const int nr = r + server_detail::kDr[i];
    const int nc = c + server_detail::kDc[i];
    if (!server_detail::InBounds(nr, nc) || server_detail::has_mine[nr][nc]) {
      continue;
    }
    server_detail::RevealRegion(nr, nc);
  }
  server_detail::UpdateWinState();
}

/**
 * @brief The definition of function ExitGame()
 *
 * @details This function is designed to exit the game.
 * It outputs a line according to the result, and a line of two integers, visit_count and marked_mine_count,
 * representing the number of blocks visited and the number of marked mines taken respectively.
 *
 * @note If the player wins, we consider that ALL mines are correctly marked.
 */
void ExitGame() {
  if (game_state == 1) {
    std::cout << "YOU WIN!" << std::endl;
    std::cout << server_detail::visited_safe_cells << " " << total_mines << std::endl;
  } else {
    std::cout << "GAME OVER!" << std::endl;
    std::cout << server_detail::visited_safe_cells << " " << server_detail::correctly_marked_mines << std::endl;
  }
  exit(0);  // Exit the game immediately
}

/**
 * @brief The definition of function PrintMap()
 *
 * @details This function is designed to print the game map to stdout. We take the 3 * 3 game map above as an example.
 * At the beginning, if you call PrintMap(), the stdout would be
 *    ???
 *    ???
 *    ???
 * If you call VisitBlock(2, 0) and PrintMap() after that, the stdout would be
 *    ???
 *    12?
 *    01?
 * If you call VisitBlock(0, 1) and PrintMap() after that, the stdout would be
 *    ?X?
 *    12?
 *    01?
 * If the player visits all blocks without mine and call PrintMap() after that, the stdout would be
 *    1@1
 *    122
 *    01@
 * (You may find the global variable game_state useful when implementing this function.)
 *
 * @note Use std::cout to print the game map, especially when you want to try the advanced task!!!
 */
void PrintMap() {
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < columns; ++c) {
      if (game_state == 1 && server_detail::has_mine[r][c]) {
        std::cout << '@';
      } else if (server_detail::marked[r][c]) {
        std::cout << (server_detail::has_mine[r][c] ? '@' : 'X');
      } else if (!server_detail::visited[r][c]) {
        std::cout << '?';
      } else if (server_detail::has_mine[r][c]) {
        std::cout << 'X';
      } else {
        std::cout << server_detail::adjacent_mines[r][c];
      }
    }
    std::cout << std::endl;
  }
}

#endif
