#include <iostream>
#include <string>

#include "ticket_system.hpp"

static void parse_args(const Vector<std::string> &toks,
                       HashMap<std::string> &args) {
  for (int i = 1; i + 1 < toks.size(); i += 2) {
    if (toks[i].size() == 2 && toks[i][0] == '-') {
      args.put(toks[i].substr(1), toks[i + 1]);
    }
  }
}

static std::string get_arg(HashMap<std::string> &args, const char *k,
                           const char *def = "") {
  std::string *p = args.find(k);
  if (p) return *p;
  return std::string(def);
}

static bool has_arg(HashMap<std::string> &args, const char *k) {
  return args.find(k) != nullptr;
}

int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  TicketSystem sys;
  sys.load();

  std::string line;
  while (std::getline(std::cin, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
      line.pop_back();
    }
    if (line.empty()) continue;
    Vector<std::string> toks;
    std::string cur;
    for (size_t i = 0; i <= line.size(); ++i) {
      if (i == line.size() || line[i] == ' ') {
        if (!cur.empty()) {
          toks.push_back(cur);
          cur.clear();
        }
      } else if (line[i] != '\r') {
        cur.push_back(line[i]);
      }
    }
    if (toks.empty()) continue;
    const std::string &cmd = toks[0];
    HashMap<std::string> args;
    parse_args(toks, args);

    if (cmd == "add_user") {
      sys.add_user(get_arg(args, "c"), get_arg(args, "u"), get_arg(args, "p"),
                   get_arg(args, "n"), get_arg(args, "m"),
                   std::stoi(get_arg(args, "g", "0")));
    } else if (cmd == "login") {
      sys.login(get_arg(args, "u"), get_arg(args, "p"));
    } else if (cmd == "logout") {
      sys.logout(get_arg(args, "u"));
    } else if (cmd == "query_profile") {
      sys.query_profile(get_arg(args, "c"), get_arg(args, "u"));
    } else if (cmd == "modify_profile") {
      sys.modify_profile(get_arg(args, "c"), get_arg(args, "u"), has_arg(args, "p"),
                         get_arg(args, "p"), has_arg(args, "n"), get_arg(args, "n"),
                         has_arg(args, "m"), get_arg(args, "m"), has_arg(args, "g"),
                         has_arg(args, "g") ? std::stoi(get_arg(args, "g")) : 0);
    } else if (cmd == "add_train") {
      sys.add_train(get_arg(args, "i"), std::stoi(get_arg(args, "n")),
                    std::stoi(get_arg(args, "m")), get_arg(args, "s"),
                    get_arg(args, "p"), get_arg(args, "x"), get_arg(args, "t"),
                    get_arg(args, "o"), get_arg(args, "d"), get_arg(args, "y")[0]);
    } else if (cmd == "release_train") {
      sys.release_train(get_arg(args, "i"));
    } else if (cmd == "query_train") {
      sys.query_train(get_arg(args, "i"), get_arg(args, "d"));
    } else if (cmd == "delete_train") {
      sys.delete_train(get_arg(args, "i"));
    } else if (cmd == "query_ticket") {
      std::string p = get_arg(args, "p", "time");
      sys.query_ticket(get_arg(args, "s"), get_arg(args, "t"), get_arg(args, "d"),
                       p != "cost");
    } else if (cmd == "query_transfer") {
      std::string p = get_arg(args, "p", "time");
      sys.query_transfer(get_arg(args, "s"), get_arg(args, "t"), get_arg(args, "d"),
                         p != "cost");
    } else if (cmd == "buy_ticket") {
      std::string q = get_arg(args, "q", "false");
      sys.buy_ticket(get_arg(args, "u"), get_arg(args, "i"), get_arg(args, "d"),
                     std::stoi(get_arg(args, "n")), get_arg(args, "f"),
                     get_arg(args, "t"), q == "true");
    } else if (cmd == "query_order") {
      sys.query_order(get_arg(args, "u"));
    } else if (cmd == "refund_ticket") {
      int n = 1;
      if (has_arg(args, "n")) n = std::stoi(get_arg(args, "n"));
      sys.refund_ticket(get_arg(args, "u"), n);
    } else if (cmd == "clean") {
      sys.clean();
    } else if (cmd == "exit") {
      sys.exit_cmd();
    }
  }
  return 0;
}

