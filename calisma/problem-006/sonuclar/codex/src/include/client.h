#ifndef CLIENT_H
#define CLIENT_H

#include <algorithm>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <string>
#include <utility>
#include <vector>

extern int rows;         // The count of rows of the game map.
extern int columns;      // The count of columns of the game map.
extern int total_mines;  // The count of mines of the game map.

// You MUST NOT use any other external variables except for rows, columns and total_mines.

void Execute(int r, int c, int type);

namespace client_detail {

constexpr int kUnknown = -1;
constexpr int kMarked = -2;
constexpr int kMaxSize = 35;
constexpr int kExactEnumerationLimit = 18;
constexpr int kHeuristicRiskScale = 1000000;
constexpr int kDr[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
constexpr int kDc[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

struct Action {
  int row = -1;
  int column = -1;
  int type = 0;
  bool valid = false;
};

struct Constraint {
  std::vector<int> cells;
  int mines = 0;
};

struct Candidate {
  int row = -1;
  int column = -1;
  int risk_numerator = kHeuristicRiskScale;
  int risk_denominator = kHeuristicRiskScale;
  int utility = -1;
  bool valid = false;
};

std::vector<std::string> board;

bool IsDigit(char ch) {
  return '0' <= ch && ch <= '8';
}

bool InBounds(int r, int c) {
  return 0 <= r && r < rows && 0 <= c && c < columns;
}

std::vector<std::pair<int, int>> UnknownNeighbors(int r, int c, int &marked_neighbors) {
  std::vector<std::pair<int, int>> unknowns;
  marked_neighbors = 0;
  for (int i = 0; i < 8; ++i) {
    const int nr = r + kDr[i];
    const int nc = c + kDc[i];
    if (!InBounds(nr, nc)) {
      continue;
    }
    if (board[nr][nc] == '?') {
      unknowns.push_back({nr, nc});
    } else if (board[nr][nc] == '@') {
      ++marked_neighbors;
    }
  }
  return unknowns;
}

int CountUnknownAround(int r, int c) {
  int count = 0;
  for (int i = 0; i < 8; ++i) {
    const int nr = r + kDr[i];
    const int nc = c + kDc[i];
    if (InBounds(nr, nc) && board[nr][nc] == '?') {
      ++count;
    }
  }
  return count;
}

int CountUnknownNeighborsForUtility(int r, int c) {
  int score = 0;
  for (int i = 0; i < 8; ++i) {
    const int nr = r + kDr[i];
    const int nc = c + kDc[i];
    if (InBounds(nr, nc) && board[nr][nc] == '?') {
      ++score;
    }
  }
  return score;
}

Action MakeAction(int row, int column, int type) {
  Action action;
  action.row = row;
  action.column = column;
  action.type = type;
  action.valid = true;
  return action;
}

void ResetGameState() {
  board.assign(rows, std::string(columns, '?'));
}

void ExecuteAction(const Action &action) {
#ifdef CLIENT_DEBUG_LOG
  std::cerr << "ACTION " << action.row << " " << action.column << " " << action.type << std::endl;
#endif
  Execute(action.row, action.column, action.type);
}

bool BetterRisk(int num_a, int den_a, int util_a, int num_b, int den_b, int util_b) {
  const long long left = 1LL * num_a * den_b;
  const long long right = 1LL * num_b * den_a;
  if (left != right) {
    return left < right;
  }
  return util_a > util_b;
}

void UpdateBestCandidate(Candidate &best, int row, int column, int numerator, int denominator, int utility) {
  if (denominator <= 0) {
    return;
  }
  if (!best.valid || BetterRisk(numerator, denominator, utility, best.risk_numerator, best.risk_denominator, best.utility)) {
    best.row = row;
    best.column = column;
    best.risk_numerator = numerator;
    best.risk_denominator = denominator;
    best.utility = utility;
    best.valid = true;
  }
}

Action DirectReasoning() {
  Action auto_explore;
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < columns; ++c) {
      if (!IsDigit(board[r][c])) {
        continue;
      }
      int marked_neighbors = 0;
      auto unknowns = UnknownNeighbors(r, c, marked_neighbors);
      if (unknowns.empty()) {
        continue;
      }
      const int number = board[r][c] - '0';
      const int remaining_mines = number - marked_neighbors;
      if (remaining_mines == static_cast<int>(unknowns.size())) {
        return MakeAction(unknowns.front().first, unknowns.front().second, 1);
      }
      if (remaining_mines == 0 && !auto_explore.valid) {
        auto_explore = MakeAction(r, c, 2);
      }
    }
  }
  return auto_explore;
}

Action SubsetReasoning(const std::vector<Constraint> &constraints, const std::vector<std::pair<int, int>> &cells) {
  Action safe_action;
  for (size_t i = 0; i < constraints.size(); ++i) {
    for (size_t j = 0; j < constraints.size(); ++j) {
      if (i == j || constraints[i].cells.size() >= constraints[j].cells.size()) {
        continue;
      }
      if (!std::includes(constraints[j].cells.begin(), constraints[j].cells.end(), constraints[i].cells.begin(),
                         constraints[i].cells.end())) {
        continue;
      }
      std::vector<int> diff;
      std::set_difference(constraints[j].cells.begin(), constraints[j].cells.end(), constraints[i].cells.begin(),
                          constraints[i].cells.end(), std::back_inserter(diff));
      const int diff_mines = constraints[j].mines - constraints[i].mines;
      if (diff.empty()) {
        continue;
      }
      if (diff_mines == static_cast<int>(diff.size())) {
        const int row = cells[diff.front()].first;
        const int column = cells[diff.front()].second;
        return MakeAction(row, column, 1);
      }
      if (diff_mines == 0 && !safe_action.valid) {
        const int row = cells[diff.front()].first;
        const int column = cells[diff.front()].second;
        safe_action = MakeAction(row, column, 0);
      }
    }
  }
  return safe_action;
}

struct ExactAnalysisResult {
  long long model_count = 0;
  std::vector<long long> mine_count;
};

ExactAnalysisResult AnalyzeComponentExactly(const std::vector<int> &component_cells, const std::vector<int> &component_constraints,
                                            const std::vector<Constraint> &constraints, const std::vector<std::vector<int>> &cell_to_constraints) {
  ExactAnalysisResult result;
  const int cell_count = static_cast<int>(component_cells.size());
  result.mine_count.assign(cell_count, 0);

  std::vector<int> global_to_local(rows * columns, -1);
  for (int i = 0; i < cell_count; ++i) {
    global_to_local[component_cells[i]] = i;
  }

  std::vector<Constraint> local_constraints;
  local_constraints.reserve(component_constraints.size());
  for (int constraint_id : component_constraints) {
    Constraint local;
    local.mines = constraints[constraint_id].mines;
    for (int cell_id : constraints[constraint_id].cells) {
      local.cells.push_back(global_to_local[cell_id]);
    }
    local_constraints.push_back(std::move(local));
  }

  std::vector<std::vector<int>> variable_to_constraints(cell_count);
  for (int constraint_index = 0; constraint_index < static_cast<int>(local_constraints.size()); ++constraint_index) {
    for (int cell_id : local_constraints[constraint_index].cells) {
      variable_to_constraints[cell_id].push_back(constraint_index);
    }
  }

  std::vector<int> order(cell_count);
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&](int lhs, int rhs) {
    return variable_to_constraints[lhs].size() > variable_to_constraints[rhs].size();
  });

  std::vector<int> mines_used(local_constraints.size(), 0);
  std::vector<int> undecided(local_constraints.size(), 0);
  for (int i = 0; i < static_cast<int>(local_constraints.size()); ++i) {
    undecided[i] = static_cast<int>(local_constraints[i].cells.size());
  }
  std::vector<int> assignment(cell_count, 0);

  auto feasible = [&]() {
    for (int i = 0; i < static_cast<int>(local_constraints.size()); ++i) {
      if (mines_used[i] > local_constraints[i].mines) {
        return false;
      }
      if (mines_used[i] + undecided[i] < local_constraints[i].mines) {
        return false;
      }
    }
    return true;
  };

  auto dfs = [&](auto &&self, int index) -> void {
    if (index == cell_count) {
      ++result.model_count;
      for (int i = 0; i < cell_count; ++i) {
        result.mine_count[i] += assignment[i];
      }
      return;
    }

    const int variable = order[index];
    for (int value = 0; value <= 1; ++value) {
      assignment[variable] = value;
      for (int constraint_index : variable_to_constraints[variable]) {
        mines_used[constraint_index] += value;
        --undecided[constraint_index];
      }
      if (feasible()) {
        self(self, index + 1);
      }
      for (int constraint_index : variable_to_constraints[variable]) {
        mines_used[constraint_index] -= value;
        ++undecided[constraint_index];
      }
    }
  };

  if (feasible()) {
    dfs(dfs, 0);
  }
  return result;
}

Action ExactOrHeuristicReasoning(const std::vector<Constraint> &constraints, const std::vector<std::pair<int, int>> &cells,
                                 const std::vector<std::vector<int>> &cell_to_constraints, int remaining_mines,
                                 const std::vector<std::vector<int>> &constraint_to_cells) {
  const int frontier_cells = static_cast<int>(cells.size());
  std::vector<bool> visited_cell(frontier_cells, false);
  std::vector<bool> visited_constraint(constraints.size(), false);
  Candidate best_guess;
  Action safe_action;

  for (int start = 0; start < frontier_cells; ++start) {
    if (visited_cell[start]) {
      continue;
    }

    std::queue<int> pending_cells;
    pending_cells.push(start);
    visited_cell[start] = true;
    std::vector<int> component_cells;
    std::vector<int> component_constraints;

    while (!pending_cells.empty()) {
      const int cell_id = pending_cells.front();
      pending_cells.pop();
      component_cells.push_back(cell_id);
      for (int constraint_id : cell_to_constraints[cell_id]) {
        if (visited_constraint[constraint_id]) {
          continue;
        }
        visited_constraint[constraint_id] = true;
        component_constraints.push_back(constraint_id);
        for (int neighbor_cell : constraint_to_cells[constraint_id]) {
          if (!visited_cell[neighbor_cell]) {
            visited_cell[neighbor_cell] = true;
            pending_cells.push(neighbor_cell);
          }
        }
      }
    }

    if (component_cells.empty()) {
      continue;
    }

    if (static_cast<int>(component_cells.size()) <= kExactEnumerationLimit) {
      const auto analysis = AnalyzeComponentExactly(component_cells, component_constraints, constraints, cell_to_constraints);
      if (analysis.model_count > 0) {
        for (int local_index = 0; local_index < static_cast<int>(component_cells.size()); ++local_index) {
          const int global_cell = component_cells[local_index];
          const int row = cells[global_cell].first;
          const int column = cells[global_cell].second;
          if (analysis.mine_count[local_index] == analysis.model_count) {
            return MakeAction(row, column, 1);
          }
          if (analysis.mine_count[local_index] == 0 && !safe_action.valid) {
            safe_action = MakeAction(row, column, 0);
          }
          UpdateBestCandidate(best_guess, row, column, static_cast<int>(analysis.mine_count[local_index]),
                              static_cast<int>(analysis.model_count), CountUnknownNeighborsForUtility(row, column));
        }
        continue;
      }
    }

    for (int global_cell : component_cells) {
      const int row = cells[global_cell].first;
      const int column = cells[global_cell].second;
      int numerator = 0;
      int denominator = 0;
      for (int constraint_id : cell_to_constraints[global_cell]) {
        numerator += constraints[constraint_id].mines;
        denominator += static_cast<int>(constraints[constraint_id].cells.size());
      }
      if (denominator == 0) {
        numerator = remaining_mines;
        denominator = rows * columns;
      }
      UpdateBestCandidate(best_guess, row, column, numerator, denominator, CountUnknownNeighborsForUtility(row, column));
    }
  }

  if (safe_action.valid) {
    return safe_action;
  }

  int total_marked = 0;
  int total_unknown = 0;
  std::vector<std::vector<bool>> is_frontier(rows, std::vector<bool>(columns, false));
  for (size_t i = 0; i < cells.size(); ++i) {
    is_frontier[cells[i].first][cells[i].second] = true;
  }

  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < columns; ++c) {
      if (board[r][c] == '@') {
        ++total_marked;
      } else if (board[r][c] == '?') {
        ++total_unknown;
      }
    }
  }

  const int global_remaining_mines = std::max(0, total_mines - total_marked);
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < columns; ++c) {
      if (board[r][c] != '?' || is_frontier[r][c]) {
        continue;
      }
      UpdateBestCandidate(best_guess, r, c, global_remaining_mines, std::max(1, total_unknown), CountUnknownNeighborsForUtility(r, c));
    }
  }

  if (best_guess.valid) {
    return MakeAction(best_guess.row, best_guess.column, 0);
  }

  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < columns; ++c) {
      if (board[r][c] == '?') {
        return MakeAction(r, c, 0);
      }
    }
  }

  return Action{};
}

}  // namespace client_detail

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
/**
 * @brief The definition of function InitGame()
 *
 * @details This function is designed to initialize the game. It should be called at the beginning of the game, which
 * will read the scale of the game map and the first step taken by the server (see README).
 */
void InitGame() {
  client_detail::ResetGameState();
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
  client_detail::board.assign(rows, std::string(columns, '?'));
  for (int r = 0; r < rows; ++r) {
    std::cin >> client_detail::board[r];
  }
}

/**
 * @brief The definition of function Decide()
 *
 * @details This function is designed to decide the next step when playing the client's (or player's) role. Open up your
 * mind and make your decision here! Caution: you can only execute once in this function.
 */
void Decide() {
  using namespace client_detail;

  Action action = DirectReasoning();
  if (action.valid) {
    ExecuteAction(action);
    return;
  }

  std::vector<std::pair<int, int>> frontier_cells;
  std::vector<std::vector<int>> cell_id(rows, std::vector<int>(columns, -1));
  std::vector<Constraint> constraints;

  int marked_cells = 0;
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < columns; ++c) {
      if (board[r][c] == '@') {
        ++marked_cells;
      }
      if (!IsDigit(board[r][c])) {
        continue;
      }
      int marked_neighbors = 0;
      auto unknowns = UnknownNeighbors(r, c, marked_neighbors);
      if (unknowns.empty()) {
        continue;
      }
      Constraint constraint;
      constraint.mines = board[r][c] - '0' - marked_neighbors;
      if (constraint.mines < 0 || constraint.mines > static_cast<int>(unknowns.size())) {
        continue;
      }
      for (size_t i = 0; i < unknowns.size(); ++i) {
        const int nr = unknowns[i].first;
        const int nc = unknowns[i].second;
        if (cell_id[nr][nc] == -1) {
          cell_id[nr][nc] = static_cast<int>(frontier_cells.size());
          frontier_cells.push_back({nr, nc});
        }
        constraint.cells.push_back(cell_id[nr][nc]);
      }
      std::sort(constraint.cells.begin(), constraint.cells.end());
      constraint.cells.erase(std::unique(constraint.cells.begin(), constraint.cells.end()), constraint.cells.end());
      constraints.push_back(std::move(constraint));
    }
  }

  action = SubsetReasoning(constraints, frontier_cells);
  if (action.valid) {
    ExecuteAction(action);
    return;
  }

  std::vector<std::vector<int>> cell_to_constraints(frontier_cells.size());
  std::vector<std::vector<int>> constraint_to_cells(constraints.size());
  for (int constraint_id = 0; constraint_id < static_cast<int>(constraints.size()); ++constraint_id) {
    constraint_to_cells[constraint_id] = constraints[constraint_id].cells;
    for (int cell : constraints[constraint_id].cells) {
      cell_to_constraints[cell].push_back(constraint_id);
    }
  }

  action = ExactOrHeuristicReasoning(constraints, frontier_cells, cell_to_constraints, total_mines - marked_cells, constraint_to_cells);
  if (!action.valid) {
    action = MakeAction(0, 0, 0);
  }
  ExecuteAction(action);
}

#endif
