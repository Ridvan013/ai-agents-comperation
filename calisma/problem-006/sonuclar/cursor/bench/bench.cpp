// Local benchmark harness for the advanced Minesweeper client.
//
// This file is NOT part of the submission. It reproduces the interaction that
// the provided advanced.cpp performs (Execute redirects the server's PrintMap
// output into the client's ReadMap), but supplies its own map generator so it
// compiles on older toolchains that lack C++17 inline variables (the shipped
// generator.h uses one). It then plays many random games and reports the
// average score fraction (p + q) / (n * m).

#include <cstdint>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "server.h"
#include "client.h"

// ---- Execute: identical mechanism to the shipped advanced.cpp ----
static bool g_bench_batch = true;
long long g_bug_mark_nonmine = 0;  // Client marked a grid that is NOT a mine (would lose).
long long g_bug_visit_mine_claimed_safe = 0;

void Execute(int row, int column, int type) {
  if (type == 1 && !server_detail::is_mine[row][column]) ++g_bug_mark_nonmine;
  if (type == 0 && client_detail::last_action_kind == 0 && server_detail::is_mine[row][column])
    ++g_bug_visit_mine_claimed_safe;
  if (type == 0) {
    VisitBlock(row, column);
  } else if (type == 1) {
    MarkMine(row, column);
  } else if (type == 2) {
    AutoExplore(row, column);
  }
  if (game_state != 0) {
    std::ostringstream sink;
    std::streambuf *ob = std::cout.rdbuf(sink.rdbuf());
    ExitGame();  // suppress_exit is set, so this only prints the summary (discarded here).
    std::cout.rdbuf(ob);
    if (g_bench_batch) return;
  }
  std::ostringstream oss;
  std::streambuf *old_out = std::cout.rdbuf();
  std::cout.rdbuf(oss.rdbuf());
  PrintMap();
  std::cout.rdbuf(old_out);
  std::string str = oss.str();
  std::istringstream iss(str);
  std::streambuf *old_in = std::cin.rdbuf();
  std::cin.rdbuf(iss.rdbuf());
  ReadMap();
  std::cin.rdbuf(old_in);
}

// ---- Map generation mirroring generator.h::GenerateMap ----
static std::mt19937_64 g_gen;

static int RandInt(int lo, int hi) {
  std::uniform_int_distribution<int> dist(lo, hi);
  return dist(g_gen);
}

static std::string GenerateMapString(int n, int m, int mine_count, int min_dist,
                                     int &first_r, int &first_c) {
  std::vector<std::pair<int, int>> avail;
  std::vector<std::vector<char>> mine(n, std::vector<char>(m, 0));
  int r0 = RandInt(1, n - 2);
  int c0 = RandInt(1, m - 2);
  for (int i = 0; i < n; ++i)
    for (int j = 0; j < m; ++j)
      if (std::abs(r0 - i) + std::abs(c0 - j) > min_dist) avail.emplace_back(i, j);
  for (int i = 0; i < mine_count && !avail.empty(); ++i) {
    int idx = RandInt(0, static_cast<int>(avail.size()) - 1);
    mine[avail[idx].first][avail[idx].second] = 1;
    avail.erase(avail.begin() + idx);
  }
  std::ostringstream oss;
  oss << n << " " << m << "\n";
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < m; ++j) oss << (mine[i][j] ? 'X' : '.');
    oss << "\n";
  }
  first_r = r0;
  first_c = c0;
  return oss.str();
}

int main(int argc, char **argv) {
  server_detail::suppress_exit = true;  // Keep playing across many games.

  struct Cfg { int n, m, mines, min_dist; };
  std::vector<Cfg> cfgs = {
      {9, 9, 10, 1},   {16, 16, 40, 1}, {16, 30, 99, 1},
      {30, 30, 150, 1}, {20, 20, 80, 1}, {12, 12, 25, 1},
  };
  int games_per_cfg = 200;
  uint64_t seed = 12345;
  int min_dist_override = -1;
  if (argc > 1) games_per_cfg = std::atoi(argv[1]);
  if (argc > 2) seed = std::strtoull(argv[2], nullptr, 10);
  if (argc > 3) min_dist_override = std::atoi(argv[3]);
  if (min_dist_override >= 0)
    for (auto &cfg : cfgs) cfg.min_dist = min_dist_override;
  g_gen.seed(seed);

  double grand_total = 0.0;
  int grand_games = 0;
  int grand_wins = 0;

  for (auto &cfg : cfgs) {
    double sum_frac = 0.0;
    int wins = 0;
    for (int gcount = 0; gcount < games_per_cfg; ++gcount) {
      int fr, fc;
      std::string map_str = GenerateMapString(cfg.n, cfg.m, cfg.mines, cfg.min_dist, fr, fc);
      // Feed map to server via InitMap, then feed "first move" to client via InitGame.
      std::string full = map_str + std::to_string(fr) + " " + std::to_string(fc) + "\n";
      std::istringstream iss(full);
      std::streambuf *old_in = std::cin.rdbuf(iss.rdbuf());
      InitMap();
      InitGame();
      while (game_state == 0) {
        Decide();
      }
      std::cin.rdbuf(old_in);

      int q = server_detail::visited_non_mine;
      int p = (game_state == 1) ? total_mines : server_detail::correct_marks;
      double frac = static_cast<double>(p + q) / (cfg.n * cfg.m);
      sum_frac += frac;
      if (game_state == 1) ++wins;
      game_state = 0;
    }
    double avg = sum_frac / games_per_cfg * 100.0;
    std::cerr << cfg.n << "x" << cfg.m << " mines=" << cfg.mines
              << "  avg_score=" << avg << "%  win_rate="
              << (100.0 * wins / games_per_cfg) << "%\n";
    grand_total += sum_frac;
    grand_games += games_per_cfg;
    grand_wins += wins;
  }
  std::cerr << "OVERALL avg_score=" << (grand_total / grand_games * 100.0)
            << "%  win_rate=" << (100.0 * grand_wins / grand_games) << "%\n";
  std::cerr << "DIAG bug_mark_nonmine=" << g_bug_mark_nonmine
            << " bug_visit_mine_claimed_safe=" << g_bug_visit_mine_claimed_safe << "\n";
  return 0;
}
