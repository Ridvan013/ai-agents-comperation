#ifndef CLIENT_H
#define CLIENT_H

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

extern int rows;         // The count of rows of the game map.
extern int columns;      // The count of columns of the game map.
extern int total_mines;  // The count of mines of the game map.

// You MUST NOT use any other external variables except for rows, columns and total_mines.

void Execute(int r, int c, int type);

// ---------------------------------------------------------------------------
// Internal client state and the reasoning engine.
//
// The client keeps its own belief of the board (parsed in ReadMap) and, on
// every Decide() call, performs exact constraint reasoning to find grids that
// are provably safe or provably mines. Provably safe grids are visited,
// provably mine grids are marked (which locks in score even if a later guess
// fails). When no certain move exists, it estimates each hidden grid's mine
// probability and visits the least dangerous one.
// ---------------------------------------------------------------------------
namespace client_detail {

constexpr int kMaxN = 32;
// Enumeration safety caps: components larger than this (or search trees bigger
// than the node cap) are handled heuristically instead of exhaustively.
constexpr int kCompVarCap = 24;
constexpr long long kNodeCap = 2000000;

int cell_status[kMaxN][kMaxN];  // 0 = hidden, 1 = revealed number, 2 = flagged mine.
int cell_number[kMaxN][kMaxN];  // Revealed adjacent-mine count when status == 1.

std::vector<std::pair<int, int>> safe_queue;  // Grids proven safe, waiting to be visited.
std::vector<std::pair<int, int>> mine_queue;  // Grids proven to be mines, waiting to be flagged.

bool has_guess;      // Whether a fallback guess has been prepared.
int guess_r, guess_c;

// Classification of the most recent action, used only by the local test harness
// to validate reasoning (0 = proven-safe visit, 1 = proven-mine flag, 2 = guess).
int last_action_kind = -1;

inline bool InBounds(int r, int c) { return r >= 0 && r < rows && c >= 0 && c < columns; }

inline void ResetState() {
  for (int r = 0; r < kMaxN; ++r) {
    for (int c = 0; c < kMaxN; ++c) {
      cell_status[r][c] = 0;
      cell_number[r][c] = 0;
    }
  }
  safe_queue.clear();
  mine_queue.clear();
  has_guess = false;
  guess_r = guess_c = -1;
}

// A local constraint of one connected component: the listed variable slots must
// contain exactly `req` mines.
struct Constraint {
  std::vector<int> vars;  // Indices into the component's variable list.
  int req;
};

// A connected group of hidden frontier grids linked through shared constraints.
struct Component {
  std::vector<int> cells;         // Global variable ids of the component's grids.
  std::vector<Constraint> cons;   // Constraints acting on those grids.
  // Filled by enumeration:
  std::vector<long long> sol_by_mines;             // solutions[t] = #assignments using t mines.
  std::vector<std::vector<long long>> mine_by_t;   // mine_by_t[localVar][t] = #solutions with that grid a mine.
  std::vector<bool> feasible_t;                    // Whether mine-count t survives the global constraint.
  bool enumerated;                                 // False if the component was too large to solve exactly.
  int max_mines;                                   // cells.size().
};

// Backtracking enumeration state (per component).
struct Enumerator {
  const Component *comp;
  int n;
  std::vector<int> assign;                 // Current assignment (0/1) per local var.
  std::vector<std::vector<int>> var_cons;  // Constraints touching each local var.
  std::vector<int> con_size, con_req, con_assigned, con_mines;
  long long nodes;
  bool aborted;

  void Init(const Component *c) {
    comp = c;
    n = static_cast<int>(c->cells.size());
    assign.assign(n, 0);
    var_cons.assign(n, {});
    int m = static_cast<int>(c->cons.size());
    con_size.assign(m, 0);
    con_req.assign(m, 0);
    con_assigned.assign(m, 0);
    con_mines.assign(m, 0);
    for (int i = 0; i < m; ++i) {
      con_size[i] = static_cast<int>(c->cons[i].vars.size());
      con_req[i] = c->cons[i].req;
      for (int v : c->cons[i].vars) var_cons[v].push_back(i);
    }
    nodes = 0;
    aborted = false;
  }

  bool SetVar(int v, int val) {  // Returns false if a constraint becomes violated.
    assign[v] = val;
    for (int ci : var_cons[v]) {
      ++con_assigned[ci];
      con_mines[ci] += val;
      if (con_mines[ci] > con_req[ci]) return false;
      if (con_mines[ci] + (con_size[ci] - con_assigned[ci]) < con_req[ci]) return false;
    }
    return true;
  }

  void UnsetVar(int v, int val) {
    for (int ci : var_cons[v]) {
      --con_assigned[ci];
      con_mines[ci] -= val;
    }
    assign[v] = 0;
  }

  void Dfs(int idx, int mines, Component *out) {
    if (aborted) return;
    if (++nodes > kNodeCap) { aborted = true; return; }
    if (idx == n) {
      ++out->sol_by_mines[mines];
      for (int v = 0; v < n; ++v)
        if (assign[v]) ++out->mine_by_t[v][mines];
      return;
    }
    for (int val = 0; val <= 1; ++val) {
      bool ok = SetVar(idx, val);
      if (ok) Dfs(idx + 1, mines + val, out);
      UnsetVar(idx, val);
      if (aborted) return;
    }
  }
};

// Count convolution over mine totals (long double keeps a huge dynamic range).
inline std::vector<long double> ConvLD(const std::vector<long double> &a,
                                       const std::vector<long double> &b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  std::vector<long double> res(a.size() + b.size() - 1, 0.0L);
  for (int i = 0; i < static_cast<int>(a.size()); ++i) {
    if (a[i] == 0.0L) continue;
    for (int j = 0; j < static_cast<int>(b.size()); ++j)
      res[i + j] += a[i] * b[j];
  }
  return res;
}

// Boolean convolution: reachable[k] means some choice sums to k.
inline std::vector<char> ConvReach(const std::vector<char> &a, const std::vector<char> &b) {
  if (a.empty()) return b;
  if (b.empty()) return a;
  std::vector<char> res(a.size() + b.size() - 1, 0);
  for (int i = 0; i < static_cast<int>(a.size()); ++i) {
    if (!a[i]) continue;
    for (int j = 0; j < static_cast<int>(b.size()); ++j)
      if (b[j]) res[i + j] = 1;
  }
  return res;
}

// The whole reasoning pass. Fills safe_queue / mine_queue with certain moves;
// if none are found, prepares a probability-minimising guess.
inline void Solve() {
  safe_queue.clear();
  mine_queue.clear();
  has_guess = false;

  // Assign an id to every hidden grid.
  static int var_id[kMaxN][kMaxN];
  std::vector<std::pair<int, int>> cells;
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < columns; ++c) {
      var_id[r][c] = -1;
      if (cell_status[r][c] == 0) {
        var_id[r][c] = static_cast<int>(cells.size());
        cells.emplace_back(r, c);
      }
    }
  int nvar = static_cast<int>(cells.size());
  if (nvar == 0) return;  // Nothing hidden: the game is already decided.

  int flagged = 0;
  for (int r = 0; r < rows; ++r)
    for (int c = 0; c < columns; ++c)
      if (cell_status[r][c] == 2) ++flagged;

  // Build raw constraints from every revealed number.
  struct Raw { std::vector<int> vars; int req; };
  std::vector<Raw> raw;
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < columns; ++c) {
      if (cell_status[r][c] != 1) continue;
      int req = cell_number[r][c];
      std::vector<int> vs;
      for (int dr = -1; dr <= 1; ++dr)
        for (int dc = -1; dc <= 1; ++dc) {
          if (dr == 0 && dc == 0) continue;
          int nr = r + dr, nc = c + dc;
          if (!InBounds(nr, nc)) continue;
          if (cell_status[nr][nc] == 2) --req;                 // Already flagged mine.
          else if (cell_status[nr][nc] == 0) vs.push_back(var_id[nr][nc]);
        }
      if (!vs.empty()) raw.push_back({vs, req});
    }
  }

  // determined[v]: -1 unknown, 0 safe, 1 mine.
  std::vector<int> determined(nvar, -1);
  std::vector<char> in_constraint(nvar, 0);
  for (const auto &rc : raw)
    for (int v : rc.vars) in_constraint[v] = 1;

  // ---- Trivial propagation (single-constraint rule) ----
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto &rc : raw) {
      int req = rc.req, unknown = 0;
      for (int v : rc.vars) {
        if (determined[v] == 1) --req;
        else if (determined[v] == -1) ++unknown;
      }
      if (unknown == 0) continue;
      if (req == 0) {
        for (int v : rc.vars)
          if (determined[v] == -1) { determined[v] = 0; changed = true; }
      } else if (req == unknown) {
        for (int v : rc.vars)
          if (determined[v] == -1) { determined[v] = 1; changed = true; }
      }
    }
  }

  int mines_det = 0;
  for (int v = 0; v < nvar; ++v)
    if (determined[v] == 1) ++mines_det;
  int R = total_mines - flagged - mines_det;  // Mines still to place among undetermined grids.

  // Undetermined grids split into frontier (in a constraint) and interior.
  std::vector<int> undetermined;
  int interior = 0;
  for (int v = 0; v < nvar; ++v) {
    if (determined[v] != -1) continue;
    undetermined.push_back(v);
    if (!in_constraint[v]) ++interior;
  }

  // ---- Cheap global rules that work regardless of component sizes ----
  if (R == 0) {  // No mines left: every undetermined grid is safe.
    for (int v : undetermined) determined[v] = 0;
  } else if (R == static_cast<int>(undetermined.size())) {  // All remaining are mines.
    for (int v : undetermined) determined[v] = 1;
  }

  // ---- Build connected components over the remaining frontier ----
  std::vector<int> comp_of(nvar, -1);
  std::vector<Component> comps;
  {
    // Adjacency through shared constraints (only undetermined, in-constraint vars).
    std::vector<std::vector<int>> adj(nvar);
    for (auto &rc : raw) {
      std::vector<int> live;
      for (int v : rc.vars)
        if (determined[v] == -1) live.push_back(v);
      for (size_t i = 0; i + 1 < live.size(); ++i)
        for (size_t j = i + 1; j < live.size(); ++j) {
          adj[live[i]].push_back(live[j]);
          adj[live[j]].push_back(live[i]);
        }
    }
    for (int v = 0; v < nvar; ++v) {
      if (determined[v] != -1 || !in_constraint[v] || comp_of[v] != -1) continue;
      int id = static_cast<int>(comps.size());
      comps.emplace_back();
      std::vector<int> stack = {v};
      comp_of[v] = id;
      while (!stack.empty()) {
        int u = stack.back();
        stack.pop_back();
        comps[id].cells.push_back(u);
        for (int w : adj[u])
          if (comp_of[w] == -1 && determined[w] == -1) { comp_of[w] = id; stack.push_back(w); }
      }
    }
    // Map constraints (restricted to undetermined vars) onto components.
    for (auto &rc : raw) {
      std::vector<int> live;
      int req = rc.req;
      for (int v : rc.vars) {
        if (determined[v] == 1) --req;
        else if (determined[v] == -1) live.push_back(v);
      }
      if (live.empty()) continue;
      int id = comp_of[live[0]];
      Constraint con;
      con.req = req;
      con.vars.reserve(live.size());
      for (int v : live) {
        // Local index within the component.
        int local = -1;
        auto &cc = comps[id].cells;
        for (size_t k = 0; k < cc.size(); ++k)
          if (cc[k] == v) { local = static_cast<int>(k); break; }
        con.vars.push_back(local);
      }
      comps[id].cons.push_back(con);
    }
  }

  // ---- Enumerate each component exactly (within caps) ----
  bool all_enumerated = true;
  for (auto &comp : comps) {
    comp.max_mines = static_cast<int>(comp.cells.size());
    comp.sol_by_mines.assign(comp.max_mines + 1, 0);
    comp.mine_by_t.assign(comp.max_mines, std::vector<long long>(comp.max_mines + 1, 0));
    if (comp.max_mines > kCompVarCap) {
      comp.enumerated = false;
      all_enumerated = false;
      continue;
    }
    Enumerator en;
    en.Init(&comp);
    en.Dfs(0, 0, &comp);
    if (en.aborted) {
      comp.enumerated = false;
      all_enumerated = false;
    } else {
      comp.enumerated = true;
    }
  }

  // ---- Global coupling of component mine-counts + interior via mine total R ----
  // Determine, for each component, which mine-counts t are globally feasible.
  bool global_ok = all_enumerated;
  std::vector<std::vector<char>> achievable(comps.size());
  for (size_t i = 0; i < comps.size(); ++i) {
    achievable[i].assign(comps[i].max_mines + 1, 0);
    if (comps[i].enumerated)
      for (int t = 0; t <= comps[i].max_mines; ++t)
        if (comps[i].sol_by_mines[t] > 0) achievable[i][t] = 1;
  }

  if (global_ok) {
    int K = static_cast<int>(comps.size());
    // Prefix / suffix reachable-sum convolutions for leave-one-out queries.
    std::vector<std::vector<char>> pref(K + 1), suf(K + 1);
    pref[0] = std::vector<char>(1, 1);
    for (int i = 0; i < K; ++i) pref[i + 1] = ConvReach(pref[i], achievable[i]);
    suf[K] = std::vector<char>(1, 1);
    for (int i = K - 1; i >= 0; --i) suf[i] = ConvReach(achievable[i], suf[i + 1]);

    auto reach_has = [](const std::vector<char> &v, int lo, int hi) {
      lo = std::max(lo, 0);
      hi = std::min(hi, static_cast<int>(v.size()) - 1);
      for (int s = lo; s <= hi; ++s)
        if (v[s]) return true;
      return false;
    };

    for (int i = 0; i < K; ++i) {
      std::vector<char> other = ConvReach(pref[i], suf[i + 1]);
      comps[i].feasible_t.assign(comps[i].max_mines + 1, false);
      for (int t = 0; t <= comps[i].max_mines; ++t) {
        if (!achievable[i][t]) continue;
        // Need s in `other` with 0 <= R - t - s <= interior.
        if (reach_has(other, R - t - interior, R - t)) comps[i].feasible_t[t] = true;
      }
    }
    // Feasible interior mine counts.
    int min_j = interior + 1, max_j = -1;
    for (int j = 0; j <= interior; ++j)
      if (reach_has(pref[K], R - j, R - j)) { min_j = std::min(min_j, j); max_j = std::max(max_j, j); }
    if (max_j < 0) { min_j = max_j = 0; }  // Degenerate; keep safe defaults.
    if (interior > 0) {
      if (max_j == 0) {  // Interior definitely all safe.
        for (int v : undetermined)
          if (!in_constraint[v]) determined[v] = 0;
      } else if (min_j == interior) {  // Interior definitely all mines.
        for (int v : undetermined)
          if (!in_constraint[v]) determined[v] = 1;
      }
    }
  } else {
    // No global coupling available: fall back to local feasibility per component.
    for (auto &comp : comps) {
      comp.feasible_t.assign(comp.max_mines + 1, false);
      if (comp.enumerated)
        for (int t = 0; t <= comp.max_mines; ++t)
          if (comp.sol_by_mines[t] > 0) comp.feasible_t[t] = true;
    }
  }

  // ---- Derive certain safe / mine grids from enumerated components ----
  // A grid is safe if it is a mine in NO globally-feasible solution, and a mine
  // if it is a mine in EVERY such solution. This uses exact integer counts.
  for (auto &comp : comps) {
    if (!comp.enumerated) continue;
    long long feas_sol = 0;
    for (int t = 0; t <= comp.max_mines; ++t)
      if (comp.feasible_t[t]) feas_sol += comp.sol_by_mines[t];
    if (feas_sol == 0) continue;
    for (int k = 0; k < comp.max_mines; ++k) {
      long long mine_sol = 0;
      for (int t = 0; t <= comp.max_mines; ++t)
        if (comp.feasible_t[t]) mine_sol += comp.mine_by_t[k][t];
      int gv = comp.cells[k];
      if (determined[gv] != -1) continue;
      if (mine_sol == 0) determined[gv] = 0;              // Never a mine -> safe.
      else if (mine_sol == feas_sol) determined[gv] = 1;  // Always a mine.
    }
  }

  // Collect all determined grids into the action queues.
  for (int v = 0; v < nvar; ++v) {
    if (determined[v] == 0) safe_queue.push_back(cells[v]);
    else if (determined[v] == 1) mine_queue.push_back(cells[v]);
  }
  if (!safe_queue.empty() || !mine_queue.empty()) return;  // Certain moves available.

  // ---- No certainty: pick the hidden grid with the lowest mine probability ----
  // A candidate is (probability, neighbour_count). Ties are broken toward fewer
  // grid neighbours, which are likelier to reveal a 0 and trigger a cascade.
  const double kEps = 1e-9;
  double best_prob = 2.0;
  int best_nb = 99, best_r = -1, best_c = -1;
  auto neighbours = [](int r, int c) {
    int cnt = 0;
    for (int dr = -1; dr <= 1; ++dr)
      for (int dc = -1; dc <= 1; ++dc) {
        if (dr == 0 && dc == 0) continue;
        if (InBounds(r + dr, c + dc)) ++cnt;
      }
    return cnt;
  };
  auto consider = [&](double p, int r, int c) {
    int nb = neighbours(r, c);
    if (p < best_prob - kEps || (p < best_prob + kEps && nb < best_nb)) {
      best_prob = p;
      best_nb = nb;
      best_r = r;
      best_c = c;
    }
  };

  if (global_ok) {
    int K = static_cast<int>(comps.size());
    // Long-double generating functions of each component (solutions by #mines).
    std::vector<std::vector<long double>> G(K);
    for (int i = 0; i < K; ++i) {
      G[i].assign(comps[i].max_mines + 1, 0.0L);
      for (int t = 0; t <= comps[i].max_mines; ++t) G[i][t] = static_cast<long double>(comps[i].sol_by_mines[t]);
    }
    std::vector<std::vector<long double>> preL(K + 1), sufL(K + 1);
    preL[0] = std::vector<long double>(1, 1.0L);
    for (int i = 0; i < K; ++i) preL[i + 1] = ConvLD(preL[i], G[i]);
    sufL[K] = std::vector<long double>(1, 1.0L);
    for (int i = K - 1; i >= 0; --i) sufL[i] = ConvLD(G[i], sufL[i + 1]);
    const std::vector<long double> &comp_all = preL[K];

    // Binomial coefficients C(interior, x) for the interior cells.
    std::vector<long double> binom(interior + 1, 0.0L);
    binom[0] = 1.0L;
    for (int x = 1; x <= interior; ++x) binom[x] = binom[x - 1] * (interior - x + 1) / x;
    auto binom_at = [&](int x) -> long double { return (x >= 0 && x <= interior) ? binom[x] : 0.0L; };

    // Total number of globally valid configurations.
    long double total = 0.0L;
    for (int f = 0; f < static_cast<int>(comp_all.size()); ++f) total += comp_all[f] * binom_at(R - f);

    if (total > 0.0L) {
      // Frontier probabilities per component.
      for (int i = 0; i < K; ++i) {
        std::vector<long double> rest = ConvLD(preL[i], sufL[i + 1]);
        std::vector<long double> H(comps[i].max_mines + 1, 0.0L);  // Ways to complete if comp i uses t mines.
        for (int t = 0; t <= comps[i].max_mines; ++t) {
          long double w = 0.0L;
          for (int s = 0; s < static_cast<int>(rest.size()); ++s) w += rest[s] * binom_at(R - t - s);
          H[t] = w;
        }
        for (int k = 0; k < comps[i].max_mines; ++k) {
          int gv = comps[i].cells[k];
          if (determined[gv] != -1) continue;
          long double num = 0.0L;
          for (int t = 0; t <= comps[i].max_mines; ++t) num += static_cast<long double>(comps[i].mine_by_t[k][t]) * H[t];
          consider(static_cast<double>(num / total), cells[gv].first, cells[gv].second);
        }
      }
      // Interior probability: expected (mines in interior)/interior over all configs.
      if (interior > 0) {
        long double weighted = 0.0L;
        for (int f = 0; f < static_cast<int>(comp_all.size()); ++f)
          weighted += comp_all[f] * binom_at(R - f) * static_cast<long double>(R - f);
        double p_int = static_cast<double>(weighted / total / interior);
        for (int v : undetermined)
          if (!in_constraint[v]) consider(p_int, cells[v].first, cells[v].second);
      }
    }
  }

  // Fallback probability for grids without an exact estimate (huge components).
  if (best_r == -1) {
    double p = (!undetermined.empty() && R >= 0)
                   ? static_cast<double>(R) / static_cast<double>(undetermined.size())
                   : 0.5;
    for (int v : undetermined) consider(p, cells[v].first, cells[v].second);
  }
  if (best_r == -1) {  // Absolute fallback: first hidden grid.
    best_r = cells[0].first;
    best_c = cells[0].second;
  }
  has_guess = true;
  guess_r = best_r;
  guess_c = best_c;
}

// Pop grids from a queue, skipping ones already resolved on the board.
inline bool PopValid(std::vector<std::pair<int, int>> &q, int &r, int &c) {
  while (!q.empty()) {
    auto p = q.back();
    q.pop_back();
    if (cell_status[p.first][p.second] == 0) { r = p.first; c = p.second; return true; }
  }
  return false;
}

}  // namespace client_detail

/**
 * @brief Initialise the client. Resets ALL state (TestBatch calls this repeatedly),
 * reads the scale and the first (safe) step provided by the input data, then plays it.
 */
void InitGame() {
  client_detail::ResetState();
  int first_row, first_column;
  std::cin >> first_row >> first_column;
  Execute(first_row, first_column, 0);
}

/**
 * @brief Read the currently revealed map from stdin into the client's belief grid.
 *
 * @details Characters: '?' hidden, '0'..'8' revealed number, '@' a mine we flagged.
 */
void ReadMap() {
  using namespace client_detail;
  for (int r = 0; r < rows; ++r) {
    std::string line;
    std::cin >> line;
    for (int c = 0; c < columns; ++c) {
      char ch = line[c];
      if (ch == '?') {
        cell_status[r][c] = 0;
      } else if (ch == '@') {
        cell_status[r][c] = 2;
      } else if (ch >= '0' && ch <= '8') {
        cell_status[r][c] = 1;
        cell_number[r][c] = ch - '0';
      } else {  // 'X' only appears on a finished (lost) board, which we never read further.
        cell_status[r][c] = 1;
        cell_number[r][c] = 0;
      }
    }
  }
}

/**
 * @brief Decide and perform exactly one operation.
 *
 * @details Priority: drain already-proven safe visits, then proven mine flags;
 * otherwise run the reasoning engine; if it yields no certainty, visit the grid
 * with the lowest estimated mine probability.
 */
void Decide() {
  using namespace client_detail;
  int r, c;
  if (PopValid(safe_queue, r, c)) { last_action_kind = 0; Execute(r, c, 0); return; }
  if (PopValid(mine_queue, r, c)) { last_action_kind = 1; Execute(r, c, 1); return; }

  Solve();

  if (PopValid(safe_queue, r, c)) { last_action_kind = 0; Execute(r, c, 0); return; }
  if (PopValid(mine_queue, r, c)) { last_action_kind = 1; Execute(r, c, 1); return; }

  if (has_guess && cell_status[guess_r][guess_c] == 0) {
    last_action_kind = 2;
    Execute(guess_r, guess_c, 0);
    return;
  }

  // Ultimate fallback (should not normally be reached): visit the first hidden grid.
  for (int rr = 0; rr < rows; ++rr)
    for (int cc = 0; cc < columns; ++cc)
      if (cell_status[rr][cc] == 0) { last_action_kind = 2; Execute(rr, cc, 0); return; }
}

#endif
