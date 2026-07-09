#ifndef CLIENT_H
#define CLIENT_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <utility>
#include <vector>

extern int rows;         // The count of rows of the game map.
extern int columns;      // The count of columns of the game map.
extern int total_mines;  // The count of mines of the game map.

// You MUST NOT use any other external variables except for rows, columns and total_mines.

void Execute(int r, int c, int type);

/*
 * ---------------------------------------------------------------------------
 *  Client strategy
 * ---------------------------------------------------------------------------
 *  We keep our own view of the board in `grid`:
 *      -1  unknown       (printed as '?')
 *      -2  known mine    (printed as '@', i.e. a cell we have marked)
 *      0..8 a revealed number
 *
 *  Every turn (Decide) we execute exactly one action. To avoid recomputing the
 *  whole analysis on every single turn we keep a queue of actions that are
 *  provably correct (`pending`). Only when that queue is empty do we run the
 *  solver again.
 *
 *  The solver works in three tiers:
 *    1. Trivial per-cell / global deductions (cheap).
 *    2. Exact constraint-satisfaction over the frontier, split into connected
 *       components. Cells that are a mine in *every* solution of their
 *       component are marked; cells that are a mine in *no* solution are
 *       visited. This finds every logically forced move.
 *    3. When nothing is forced we must guess: we compute each unknown cell's
 *       probability of being a mine and visit the safest one.
 * ---------------------------------------------------------------------------
 */

namespace mc {  // all client internals live here to avoid clashing with server.h
const int kN = 32;

int grid[kN][kN];  // -1 unknown, -2 known mine, 0..8 revealed number

struct Action {
  int r, c, type;  // type: 0 = visit, 1 = mark
};
std::vector<Action> pending;

inline bool InBounds(int r, int c) { return r >= 0 && r < rows && c >= 0 && c < columns; }

void ResetState() {
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      grid[i][j] = -1;
    }
  }
  pending.clear();
}

// Count how many of the 8 neighbours of (r, c) currently hold the value v.
int CountNeighbourState(int r, int c, int v) {
  int cnt = 0;
  for (int dr = -1; dr <= 1; ++dr) {
    for (int dc = -1; dc <= 1; ++dc) {
      if (dr == 0 && dc == 0) continue;
      int nr = r + dr, nc = c + dc;
      if (InBounds(nr, nc) && grid[nr][nc] == v) ++cnt;
    }
  }
  return cnt;
}

// Is an action still meaningful given the current board?
bool StillValid(const Action &a) {
  if (a.type == 0) return grid[a.r][a.c] == -1;  // visit an unknown cell
  return grid[a.r][a.c] == -1;                   // mark an unknown cell
}

// ----------------------------- tier 1 & globals ---------------------------

// Add a visit/mark action if the target is still unknown and not already queued.
void EnqueueUnique(int r, int c, int type, std::vector<std::vector<char>> &queued) {
  if (grid[r][c] != -1) return;
  char bit = (type == 0) ? 1 : 2;
  if (queued[r][c] & bit) return;
  queued[r][c] |= bit;
  pending.push_back({r, c, type});
}

// ----------------------------- tier 2: exact CSP --------------------------

// A constraint: the listed frontier cells (local indices) contain exactly
// `need` mines.
struct Constraint {
  std::vector<int> cells;
  int need;
};

// Enumerate every solution of one component and fill the mine-probability and
// certainty information for its cells. `sol_count` and `mine_sol` use double so
// that they never overflow; component size is capped so the values stay exact.
struct EnumResult {
  double sol_count = 0;
  std::vector<double> mine_sol;  // per local cell: #solutions where it is a mine
};

long long g_node_budget;

void DfsEnumerate(int pos, int m, const std::vector<Constraint> &cons,
                  const std::vector<std::vector<int>> &cell_cons, std::vector<char> &assign,
                  std::vector<int> &cur_mines, std::vector<int> &cur_assigned, EnumResult &res) {
  if (g_node_budget-- <= 0) return;
  if (pos == m) {
    res.sol_count += 1.0;
    for (int i = 0; i < m; ++i) {
      if (assign[i]) res.mine_sol[i] += 1.0;
    }
    return;
  }
  for (int val = 0; val <= 1; ++val) {
    assign[pos] = static_cast<char>(val);
    bool ok = true;
    for (int ci : cell_cons[pos]) {
      cur_assigned[ci] += 1;
      cur_mines[ci] += val;
      int need = cons[ci].need;
      int len = static_cast<int>(cons[ci].cells.size());
      if (cur_mines[ci] > need) ok = false;
      else if (cur_mines[ci] + (len - cur_assigned[ci]) < need) ok = false;
      else if (cur_assigned[ci] == len && cur_mines[ci] != need) ok = false;
    }
    if (ok) DfsEnumerate(pos + 1, m, cons, cell_cons, assign, cur_mines, cur_assigned, res);
    for (int ci : cell_cons[pos]) {
      cur_assigned[ci] -= 1;
      cur_mines[ci] -= val;
    }
  }
}

// ----------------------------- solver -------------------------------------

// Run the full solver, pushing either provably-correct actions or a single best
// guess onto `pending`.
void Solve() {
  std::vector<std::vector<char>> queued(rows, std::vector<char>(columns, 0));

  int flagged = 0, unknown = 0;
  for (int i = 0; i < rows; ++i)
    for (int j = 0; j < columns; ++j) {
      if (grid[i][j] == -2) ++flagged;
      else if (grid[i][j] == -1) ++unknown;
    }
  int remaining_mines = total_mines - flagged;

  if (unknown == 0) return;

  // Global endgame shortcuts.
  if (remaining_mines <= 0) {
    for (int i = 0; i < rows; ++i)
      for (int j = 0; j < columns; ++j)
        if (grid[i][j] == -1) EnqueueUnique(i, j, 0, queued);
    if (!pending.empty()) return;
  }
  if (remaining_mines == unknown) {
    for (int i = 0; i < rows; ++i)
      for (int j = 0; j < columns; ++j)
        if (grid[i][j] == -1) EnqueueUnique(i, j, 1, queued);
    if (!pending.empty()) return;
  }

  // Tier 1: trivial per-cell deductions.
  bool trivial = false;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      int k = grid[i][j];
      if (k < 0) continue;  // only numbered cells impose constraints
      int f = CountNeighbourState(i, j, -2);
      int u = CountNeighbourState(i, j, -1);
      if (u == 0) continue;
      if (k == f) {  // all unknown neighbours are safe
        for (int dr = -1; dr <= 1; ++dr)
          for (int dc = -1; dc <= 1; ++dc) {
            int nr = i + dr, nc = j + dc;
            if ((dr || dc) && InBounds(nr, nc) && grid[nr][nc] == -1) {
              EnqueueUnique(nr, nc, 0, queued);
              trivial = true;
            }
          }
      } else if (k - f == u) {  // all unknown neighbours are mines
        for (int dr = -1; dr <= 1; ++dr)
          for (int dc = -1; dc <= 1; ++dc) {
            int nr = i + dr, nc = j + dc;
            if ((dr || dc) && InBounds(nr, nc) && grid[nr][nc] == -1) {
              EnqueueUnique(nr, nc, 1, queued);
              trivial = true;
            }
          }
      }
    }
  }
  if (trivial) return;

  // Tier 2: build the frontier and its constraints.
  std::vector<int> cell_id(rows * columns, -1);
  std::vector<std::pair<int, int>> frontier;  // frontier cells (unknown, adjacent to a number)
  auto IdOf = [&](int r, int c) -> int {
    int idx = r * columns + c;
    if (cell_id[idx] == -1) {
      cell_id[idx] = static_cast<int>(frontier.size());
      frontier.emplace_back(r, c);
    }
    return cell_id[idx];
  };

  std::vector<Constraint> constraints;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < columns; ++j) {
      int k = grid[i][j];
      if (k < 0) continue;
      int f = CountNeighbourState(i, j, -2);
      Constraint con;
      for (int dr = -1; dr <= 1; ++dr)
        for (int dc = -1; dc <= 1; ++dc) {
          int nr = i + dr, nc = j + dc;
          if ((dr || dc) && InBounds(nr, nc) && grid[nr][nc] == -1) con.cells.push_back(IdOf(nr, nc));
        }
      if (con.cells.empty()) continue;
      con.need = k - f;
      constraints.push_back(std::move(con));
    }
  }

  int fn = static_cast<int>(frontier.size());
  std::vector<double> prob(fn, -1.0);  // mine probability per frontier cell

  if (fn > 0) {
    // Union-find to split the frontier into independent components.
    std::vector<int> parent(fn);
    for (int i = 0; i < fn; ++i) parent[i] = i;
    std::function<int(int)> find = [&](int x) {
      while (parent[x] != x) {
        parent[x] = parent[parent[x]];
        x = parent[x];
      }
      return x;
    };
    for (const auto &con : constraints)
      for (size_t t = 1; t < con.cells.size(); ++t) parent[find(con.cells[t])] = find(con.cells[0]);

    // Group frontier cells and constraints by component root.
    std::vector<std::vector<int>> comp_cells(fn);
    for (int i = 0; i < fn; ++i) comp_cells[find(i)].push_back(i);
    std::vector<std::vector<int>> comp_cons(fn);
    for (int ci = 0; ci < static_cast<int>(constraints.size()); ++ci)
      comp_cons[find(constraints[ci].cells[0])].push_back(ci);

    const int kCompCap = 24;    // max cells to enumerate exactly
    g_node_budget = 8'000'000;  // global safety budget for one Solve

    for (int root = 0; root < fn; ++root) {
      if (comp_cells[root].empty()) continue;
      std::vector<int> &cells = comp_cells[root];
      int m = static_cast<int>(cells.size());

      // Map global frontier index -> local index within the component.
      std::vector<int> local(fn, -1);
      for (int i = 0; i < m; ++i) local[cells[i]] = i;

      // Build local constraints.
      std::vector<Constraint> lcons;
      for (int ci : comp_cons[root]) {
        Constraint lc;
        lc.need = constraints[ci].need;
        for (int g : constraints[ci].cells) lc.cells.push_back(local[g]);
        lcons.push_back(std::move(lc));
      }

      if (m <= kCompCap) {
        std::vector<std::vector<int>> cell_cons(m);
        for (int ci = 0; ci < static_cast<int>(lcons.size()); ++ci)
          for (int lc : lcons[ci].cells) cell_cons[lc].push_back(ci);

        EnumResult res;
        res.mine_sol.assign(m, 0.0);
        std::vector<char> assign(m, 0);
        std::vector<int> cur_mines(lcons.size(), 0), cur_assigned(lcons.size(), 0);
        DfsEnumerate(0, m, lcons, cell_cons, assign, cur_mines, cur_assigned, res);

        if (res.sol_count > 0) {
          for (int i = 0; i < m; ++i) prob[cells[i]] = res.mine_sol[i] / res.sol_count;
        } else {
          for (int i = 0; i < m; ++i) prob[cells[i]] = 0.5;  // unreachable in valid games
        }
      } else {
        // Component too large: fall back to a local estimate (no certainty).
        std::vector<double> sum(m, 0.0);
        std::vector<int> cnt(m, 0);
        for (const auto &lc : lcons) {
          double p = static_cast<double>(lc.need) / static_cast<double>(lc.cells.size());
          for (int lci : lc.cells) {
            sum[lci] += p;
            cnt[lci] += 1;
          }
        }
        for (int i = 0; i < m; ++i) prob[cells[i]] = cnt[i] ? sum[i] / cnt[i] : 0.5;
      }
    }

    // Enqueue every forced move discovered by enumeration.
    bool forced = false;
    for (int i = 0; i < fn; ++i) {
      if (prob[i] <= 1e-12) {
        EnqueueUnique(frontier[i].first, frontier[i].second, 0, queued);
        forced = true;
      } else if (prob[i] >= 1.0 - 1e-12) {
        EnqueueUnique(frontier[i].first, frontier[i].second, 1, queued);
        forced = true;
      }
    }
    if (forced) return;
  }

  // Tier 3: nothing is forced, we have to guess. Compare the safest frontier
  // cell against the "outside" cells (unknown, touching no number).
  double expected_frontier_mines = 0.0;
  int frontier_unknown = fn;
  for (int i = 0; i < fn; ++i)
    if (prob[i] >= 0) expected_frontier_mines += prob[i];

  int outside = unknown - frontier_unknown;
  double outside_prob = 1.0;
  if (outside > 0) {
    double rem = remaining_mines - expected_frontier_mines;
    outside_prob = rem / outside;
    if (outside_prob < 0) outside_prob = 0;
    if (outside_prob > 1) outside_prob = 1;
  }

  // Best frontier candidate.
  int best_r = -1, best_c = -1;
  double best_p = 2.0;
  int best_info = -1;
  for (int i = 0; i < fn; ++i) {
    double p = prob[i] >= 0 ? prob[i] : 1.0;
    int r = frontier[i].first, c = frontier[i].second;
    int info = 8 - CountNeighbourState(r, c, -1);  // more revealed neighbours = more info
    if (p < best_p - 1e-9 || (p < best_p + 1e-9 && info > best_info)) {
      best_p = p;
      best_r = r;
      best_c = c;
      best_info = info;
    }
  }

  if (outside > 0 && outside_prob < best_p - 1e-9) {
    // Guess an outside cell instead; corners/edges tend to open up more area.
    int pick_r = -1, pick_c = -1;
    for (int i = 0; i < rows && pick_r == -1; ++i)
      for (int j = 0; j < columns; ++j) {
        if (grid[i][j] == -1 && cell_id[i * columns + j] == -1) {
          pick_r = i;
          pick_c = j;
          break;
        }
      }
    if (pick_r != -1) {
      pending.push_back({pick_r, pick_c, 0});
      return;
    }
  }

  if (best_r != -1) {
    pending.push_back({best_r, best_c, 0});
    return;
  }

  // Absolute fallback: visit any unknown cell (keeps the game progressing).
  for (int i = 0; i < rows; ++i)
    for (int j = 0; j < columns; ++j)
      if (grid[i][j] == -1) {
        pending.push_back({i, j, 0});
        return;
      }
}

}  // namespace mc

/**
 * @brief Initialize the game: reset our state, then perform the guaranteed-safe
 * first move provided by the input.
 */
void InitGame() {
  mc::ResetState();
  int first_row, first_column;
  std::cin >> first_row >> first_column;
  Execute(first_row, first_column, 0);
}

/**
 * @brief Read the current map from stdin into our own view of the board.
 */
void ReadMap() {
  for (int i = 0; i < rows; ++i) {
    std::string line;
    std::cin >> line;
    for (int j = 0; j < columns; ++j) {
      char ch = line[j];
      if (ch == '?') mc::grid[i][j] = -1;
      else if (ch == '@') mc::grid[i][j] = -2;
      else mc::grid[i][j] = ch - '0';
    }
  }
}

/**
 * @brief Decide and perform exactly one action this turn.
 */
void Decide() {
  // Drop queued actions that the board has already resolved.
  while (!mc::pending.empty() && !mc::StillValid(mc::pending.back())) mc::pending.pop_back();

  if (mc::pending.empty()) mc::Solve();

  if (mc::pending.empty()) {
    // Should not happen while unknown cells remain, but stay safe.
    for (int i = 0; i < rows; ++i)
      for (int j = 0; j < columns; ++j)
        if (mc::grid[i][j] == -1) {
          Execute(i, j, 0);
          return;
        }
    return;
  }

  mc::Action a = mc::pending.back();
  mc::pending.pop_back();
  Execute(a.r, a.c, a.type);
}

#endif
