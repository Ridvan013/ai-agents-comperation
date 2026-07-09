#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <utility>
#include <vector>
#include <random>
#include <algorithm>

extern int rows;         // The count of rows of the game map.
extern int columns;      // The count of columns of the game map.
extern int total_mines;  // The count of mines of the game map.

const int C_MAX_SIZE = 35;
char c_map[C_MAX_SIZE][C_MAX_SIZE];
std::mt19937 rng(42);

/**
 * @brief The definition of function Execute(int, int, bool)
 *
 * @details This function is designed to take a step when player the client's (or player's) role, and the implementation
 * of it has been finished by TA. (I hope my comments in code would be easy to understand T_T) If you do not understand
 * the contents, please ask TA for help immediately!!!
 *
 * @param r The row coordinate (0-based) of the block to be visited.
 * @param c The column coordinate (0-based) of the block to be visited.
 * @param type The type of operation to a certain block.
 * If type == 0, we'll execute VisitBlock(row, column).
 * If type == 1, we'll execute MarkMine(row, column).
 * If type == 2, we'll execute AutoExplore(row, column).
 * You should not call this function with other type values.
 */
void Execute(int r, int c, int type);

/**
 * @brief The definition of function InitGame()
 *
 * @details This function is designed to initialize the game. It should be called at the beginning of the game, which
 * will read the scale of the game map and the first step taken by the server (see README).
 */
void InitGame() {
  for (int i = 0; i < C_MAX_SIZE; i++) {
    for (int j = 0; j < C_MAX_SIZE; j++) {
      c_map[i][j] = '?';
    }
  }
  int first_row, first_column;
  std::cin >> first_row >> first_column;
  Execute(first_row, first_column, 0);
}

/**
 * @brief The definition of function ReadMap()
 *
 * @details This function is designed to read the game map from stdin when playing the client's (or player's) role.
 * Since the client (or player) can only get the limited information of the game map, so if there is a 3 * 3 map as
 * above and only the block (2, 0) has been visited, the stdin would be
 *     ???
 *     12?
 *     01?
 */
void ReadMap() {
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      std::cin >> c_map[i][j];
    }
  }
}

/**
 * @brief The definition of function Decide()
 *
 * @details This function is designed to decide the next step when playing the client's (or player's) role. Open up your
 * mind and make your decision here! Caution: you can only execute once in this function.
 */
void Decide() {
  // 1. Basic Reasoning (Baseline 1)
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      if (c_map[i][j] >= '0' && c_map[i][j] <= '8') {
        int target = c_map[i][j] - '0';
        int marked = 0;
        std::vector<std::pair<int, int>> unknowns;
        for (int dx = -1; dx <= 1; ++dx) {
          for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue;
            int nx = i + dx, ny = j + dy;
            if (nx >= 0 && nx < rows && ny >= 0 && ny < columns) {
              if (c_map[nx][ny] == '@') marked++;
              else if (c_map[nx][ny] == '?') unknowns.push_back({nx, ny});
            }
          }
        }
        
        int needed = target - marked;
        if (unknowns.empty()) continue;
        
        if (needed == 0) {
          Execute(i, j, 2); // AutoExplore is much more efficient
          return;
        }
        
        if (needed == unknowns.size()) {
          Execute(unknowns[0].first, unknowns[0].second, 1);
          return;
        }
      }
    }
  }

  // 2. Subset Rules (Baseline 2)
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      if (c_map[i][j] >= '0' && c_map[i][j] <= '8') {
        int targetA = c_map[i][j] - '0';
        int markedA = 0;
        std::vector<std::pair<int, int>> unkA;
        for (int dx = -1; dx <= 1; ++dx) {
          for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue;
            int nx = i + dx, ny = j + dy;
            if (nx >= 0 && nx < rows && ny >= 0 && ny < columns) {
              if (c_map[nx][ny] == '@') markedA++;
              else if (c_map[nx][ny] == '?') unkA.push_back({nx, ny});
            }
          }
        }
        if (unkA.empty()) continue;
        int needA = targetA - markedA;
        
        for (int i2 = std::max(0, i - 2); i2 <= std::min(rows - 1, i + 2); ++i2) {
          for (int j2 = std::max(0, j - 2); j2 <= std::min(columns - 1, j + 2); ++j2) {
            if ((i == i2 && j == j2) || !(c_map[i2][j2] >= '0' && c_map[i2][j2] <= '8')) continue;
            
            int targetB = c_map[i2][j2] - '0';
            int markedB = 0;
            std::vector<std::pair<int, int>> unkB;
            for (int dx = -1; dx <= 1; ++dx) {
              for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) continue;
                int nx = i2 + dx, ny = j2 + dy;
                if (nx >= 0 && nx < rows && ny >= 0 && ny < columns) {
                  if (c_map[nx][ny] == '@') markedB++;
                  else if (c_map[nx][ny] == '?') unkB.push_back({nx, ny});
                }
              }
            }
            if (unkB.empty()) continue;
            int needB = targetB - markedB;
            
            bool is_subset = true;
            for (auto pa : unkA) {
              bool found = false;
              for (auto pb : unkB) {
                if (pa == pb) { found = true; break; }
              }
              if (!found) { is_subset = false; break; }
            }
            
            if (is_subset && unkA.size() < unkB.size()) {
              int diff_need = needB - needA;
              std::vector<std::pair<int, int>> diff_unk;
              for (auto pb : unkB) {
                bool found = false;
                for (auto pa : unkA) {
                  if (pa == pb) { found = true; break; }
                }
                if (!found) diff_unk.push_back(pb);
              }
              
              if (diff_need == 0 && !diff_unk.empty()) {
                Execute(diff_unk[0].first, diff_unk[0].second, 0);
                return;
              }
              if (diff_need == diff_unk.size() && !diff_unk.empty()) {
                Execute(diff_unk[0].first, diff_unk[0].second, 1);
                return;
              }
            }
          }
        }
      }
    }
  }
  
  // 3. Guesser
  int global_marked = 0;
  int global_unknown = 0;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      if (c_map[i][j] == '@') global_marked++;
      if (c_map[i][j] == '?') global_unknown++;
    }
  }
  
  double best_prob = 1.0;
  int best_r = -1, best_c = -1;
  
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      if (c_map[i][j] == '?') {
        double prob = 0.0;
        bool is_frontier = false;
        
        for (int dx = -1; dx <= 1; ++dx) {
          for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue;
            int nx = i + dx, ny = j + dy;
            if (nx >= 0 && nx < rows && ny >= 0 && ny < columns) {
              if (c_map[nx][ny] >= '0' && c_map[nx][ny] <= '8') {
                is_frontier = true;
                int target = c_map[nx][ny] - '0';
                int marked = 0;
                int unk_cnt = 0;
                for (int ddx = -1; ddx <= 1; ++ddx) {
                  for (int ddy = -1; ddy <= 1; ++ddy) {
                    if (ddx == 0 && ddy == 0) continue;
                    int nnx = nx + ddx, nny = ny + ddy;
                    if (nnx >= 0 && nnx < rows && nny >= 0 && nny < columns) {
                      if (c_map[nnx][nny] == '@') marked++;
                      else if (c_map[nnx][nny] == '?') unk_cnt++;
                    }
                  }
                }
                double p = (double)(target - marked) / unk_cnt;
                if (p > prob) prob = p;
              }
            }
          }
        }
        
        if (!is_frontier) {
          prob = global_unknown > 0 ? (double)(total_mines - global_marked) / global_unknown : 0;
        }
        
        if (prob < best_prob) {
          best_prob = prob;
          best_r = i;
          best_c = j;
        } else if (prob == best_prob) {
          if (rng() % 2 == 0) {
            best_r = i;
            best_c = j;
          }
        }
      }
    }
  }
  
  if (best_r != -1) {
    Execute(best_r, best_c, 0);
  } else {
    Execute(0, 0, 0);
  }
}

#endif