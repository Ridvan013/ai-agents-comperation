#include "bookstore.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>

namespace {
const int USER_HEADER = 2 * sizeof(int);
const int BOOK_HEADER = 2 * sizeof(int);
const int FIN_HEADER = sizeof(int);

void set_cstr(char *dst, size_t n, const std::string &s) {
  std::memset(dst, 0, n);
  if (!s.empty()) std::strncpy(dst, s.c_str(), n - 1);
}
}  // namespace

Bookstore::Bookstore() : user_count_(0), book_count_(0), fin_count_(0) {
  open_files();
  init_root();
  rebuild_indexes();
}

Bookstore::~Bookstore() {
  if (user_file_.is_open()) user_file_.close();
  if (book_file_.is_open()) book_file_.close();
  if (fin_file_.is_open()) fin_file_.close();
  if (log_file_.is_open()) log_file_.close();
}

void Bookstore::open_files() {
  // users
  {
    std::ifstream chk("user.dat", std::ios::binary);
    if (!chk.good()) {
      chk.close();
      std::ofstream cre("user.dat", std::ios::binary);
      int z = 0;
      cre.write(reinterpret_cast<char *>(&z), sizeof(int));
      cre.write(reinterpret_cast<char *>(&z), sizeof(int));
      cre.close();
    }
  }
  user_file_.open("user.dat", std::ios::in | std::ios::out | std::ios::binary);
  user_file_.seekg(0);
  user_file_.read(reinterpret_cast<char *>(&user_count_), sizeof(int));

  // books
  {
    std::ifstream chk("book.dat", std::ios::binary);
    if (!chk.good()) {
      chk.close();
      std::ofstream cre("book.dat", std::ios::binary);
      int z = 0;
      cre.write(reinterpret_cast<char *>(&z), sizeof(int));
      cre.write(reinterpret_cast<char *>(&z), sizeof(int));
      cre.close();
    }
  }
  book_file_.open("book.dat", std::ios::in | std::ios::out | std::ios::binary);
  book_file_.seekg(0);
  book_file_.read(reinterpret_cast<char *>(&book_count_), sizeof(int));

  // finance
  {
    std::ifstream chk("finance.dat", std::ios::binary);
    if (!chk.good()) {
      chk.close();
      std::ofstream cre("finance.dat", std::ios::binary);
      int z = 0;
      cre.write(reinterpret_cast<char *>(&z), sizeof(int));
      cre.close();
    }
  }
  fin_file_.open("finance.dat", std::ios::in | std::ios::out | std::ios::binary);
  fin_file_.seekg(0);
  fin_file_.read(reinterpret_cast<char *>(&fin_count_), sizeof(int));

  // log
  {
    std::ifstream chk("oplog.dat");
    if (!chk.good()) {
      chk.close();
      std::ofstream cre("oplog.dat");
      cre.close();
    }
  }
  log_file_.open("oplog.dat", std::ios::in | std::ios::out | std::ios::app);
}

void Bookstore::init_root() {
  if (user_count_ == 0) {
    User root;
    std::memset(&root, 0, sizeof(root));
    set_cstr(root.userid, sizeof(root.userid), "root");
    set_cstr(root.password, sizeof(root.password), "sjtu");
    set_cstr(root.username, sizeof(root.username), "root");
    root.privilege = 7;
    root.active = 1;
    append_user(root);
  }
}

void Bookstore::rebuild_indexes() {
  user_idx_.clear();
  isbn_idx_.clear();
  name_idx_.clear();
  author_idx_.clear();
  keyword_idx_.clear();

  for (int i = 0; i < user_count_; ++i) {
    User u;
    if (!read_user(i, u)) continue;
    if (u.active) user_idx_[std::string(u.userid)] = i;
  }
  for (int i = 0; i < book_count_; ++i) {
    Book b;
    if (!read_book(i, b)) continue;
    if (b.active) index_add_book(i, b);
  }
}

bool Bookstore::read_user(int idx, User &u) {
  if (idx < 0 || idx >= user_count_) return false;
  user_file_.clear();
  user_file_.seekg(USER_HEADER + (long long)idx * sizeof(User));
  user_file_.read(reinterpret_cast<char *>(&u), sizeof(User));
  return (bool)user_file_;
}

void Bookstore::write_user(int idx, const User &u) {
  user_file_.clear();
  user_file_.seekp(USER_HEADER + (long long)idx * sizeof(User));
  user_file_.write(reinterpret_cast<const char *>(&u), sizeof(User));
  user_file_.flush();
}

int Bookstore::append_user(const User &u) {
  int idx = user_count_;
  user_file_.clear();
  user_file_.seekp(USER_HEADER + (long long)idx * sizeof(User));
  user_file_.write(reinterpret_cast<const char *>(&u), sizeof(User));
  ++user_count_;
  user_file_.seekp(0);
  user_file_.write(reinterpret_cast<char *>(&user_count_), sizeof(int));
  user_file_.flush();
  if (u.active) user_idx_[std::string(u.userid)] = idx;
  return idx;
}

bool Bookstore::read_book(int idx, Book &b) {
  if (idx < 0 || idx >= book_count_) return false;
  book_file_.clear();
  book_file_.seekg(BOOK_HEADER + (long long)idx * sizeof(Book));
  book_file_.read(reinterpret_cast<char *>(&b), sizeof(Book));
  return (bool)book_file_;
}

void Bookstore::write_book(int idx, const Book &b) {
  book_file_.clear();
  book_file_.seekp(BOOK_HEADER + (long long)idx * sizeof(Book));
  book_file_.write(reinterpret_cast<const char *>(&b), sizeof(Book));
  book_file_.flush();
}

int Bookstore::append_book(const Book &b) {
  int idx = book_count_;
  book_file_.clear();
  book_file_.seekp(BOOK_HEADER + (long long)idx * sizeof(Book));
  book_file_.write(reinterpret_cast<const char *>(&b), sizeof(Book));
  ++book_count_;
  book_file_.seekp(0);
  book_file_.write(reinterpret_cast<char *>(&book_count_), sizeof(int));
  book_file_.flush();
  if (b.active) index_add_book(idx, b);
  return idx;
}

void Bookstore::append_finance(long long income, long long expense) {
  fin_file_.clear();
  fin_file_.seekp(FIN_HEADER + (long long)fin_count_ * sizeof(long long) * 2);
  fin_file_.write(reinterpret_cast<char *>(&income), sizeof(long long));
  fin_file_.write(reinterpret_cast<char *>(&expense), sizeof(long long));
  ++fin_count_;
  fin_file_.seekp(0);
  fin_file_.write(reinterpret_cast<char *>(&fin_count_), sizeof(int));
  fin_file_.flush();
}

void Bookstore::append_log(const std::string &msg) {
  log_file_ << msg << "\n";
  log_file_.flush();
}

void Bookstore::index_add_book(int idx, const Book &b) {
  std::string isbn(b.isbn);
  isbn_idx_[isbn] = idx;
  if (b.name[0]) name_idx_.insert(std::make_pair(std::string(b.name), idx));
  if (b.author[0]) author_idx_.insert(std::make_pair(std::string(b.author), idx));
  if (b.keyword[0]) {
    std::vector<std::string> parts;
    split_keywords(std::string(b.keyword), parts, false);
    for (size_t i = 0; i < parts.size(); ++i)
      keyword_idx_.insert(std::make_pair(parts[i], idx));
  }
}

void Bookstore::index_remove_book(int idx, const Book &b) {
  isbn_idx_.erase(std::string(b.isbn));
  if (b.name[0]) {
    std::pair<std::multimap<std::string, int>::iterator,
              std::multimap<std::string, int>::iterator>
        range = name_idx_.equal_range(std::string(b.name));
    for (std::multimap<std::string, int>::iterator it = range.first;
         it != range.second;) {
      if (it->second == idx)
        name_idx_.erase(it++);
      else
        ++it;
    }
  }
  if (b.author[0]) {
    std::pair<std::multimap<std::string, int>::iterator,
              std::multimap<std::string, int>::iterator>
        range = author_idx_.equal_range(std::string(b.author));
    for (std::multimap<std::string, int>::iterator it = range.first;
         it != range.second;) {
      if (it->second == idx)
        author_idx_.erase(it++);
      else
        ++it;
    }
  }
  if (b.keyword[0]) {
    std::vector<std::string> parts;
    split_keywords(std::string(b.keyword), parts, false);
    for (size_t i = 0; i < parts.size(); ++i) {
      std::pair<std::multimap<std::string, int>::iterator,
                std::multimap<std::string, int>::iterator>
          range = keyword_idx_.equal_range(parts[i]);
      for (std::multimap<std::string, int>::iterator it = range.first;
           it != range.second;) {
        if (it->second == idx)
          keyword_idx_.erase(it++);
        else
          ++it;
      }
    }
  }
}

int Bookstore::priv() const {
  if (stack_.empty()) return 0;
  return stack_.back().privilege;
}

void Bookstore::invalid() { std::cout << "Invalid\n"; }

std::string Bookstore::format_money(long long cents) {
  std::ostringstream oss;
  if (cents < 0) {
    oss << "-";
    cents = -cents;
  }
  oss << (cents / 100) << "." << std::setw(2) << std::setfill('0') << (cents % 100);
  return oss.str();
}

void Bookstore::print_money(long long cents) {
  std::cout << format_money(cents);
}

bool Bookstore::is_visible_ascii(unsigned char c) { return c >= 32 && c <= 126; }

bool Bookstore::valid_id(const std::string &s) {
  if (s.empty() || s.size() > 30) return false;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') || c == '_'))
      return false;
  }
  return true;
}

bool Bookstore::valid_username(const std::string &s) {
  if (s.empty() || s.size() > 30) return false;
  for (size_t i = 0; i < s.size(); ++i)
    if (!is_visible_ascii((unsigned char)s[i])) return false;
  return true;
}

bool Bookstore::valid_isbn(const std::string &s) {
  if (s.empty() || s.size() > 20) return false;
  for (size_t i = 0; i < s.size(); ++i)
    if (!is_visible_ascii((unsigned char)s[i])) return false;
  return true;
}

bool Bookstore::valid_book_str(const std::string &s, int maxlen) {
  if (s.empty() || (int)s.size() > maxlen) return false;
  for (size_t i = 0; i < s.size(); ++i) {
    unsigned char c = (unsigned char)s[i];
    if (!is_visible_ascii(c) || c == '"') return false;
  }
  return true;
}

bool Bookstore::valid_quantity(const std::string &s, int &out) {
  if (s.empty() || s.size() > 10) return false;
  for (size_t i = 0; i < s.size(); ++i)
    if (s[i] < '0' || s[i] > '9') return false;
  // parse carefully
  long long v = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    v = v * 10 + (s[i] - '0');
    if (v > 2147483647LL) return false;
  }
  if (v <= 0) return false;
  out = (int)v;
  return true;
}

bool Bookstore::valid_price(const std::string &s, long long &cents, bool positive) {
  if (s.empty() || s.size() > 13) return false;
  int dot = -1;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '.') {
      if (dot != -1) return false;
      dot = (int)i;
    } else if (s[i] < '0' || s[i] > '9') {
      return false;
    }
  }
  if (dot == 0 || dot == (int)s.size() - 1) return false;
  if (dot != -1 && (int)s.size() - dot - 1 > 2) return false;

  long long whole = 0, frac = 0;
  if (dot == -1) {
    for (size_t i = 0; i < s.size(); ++i) {
      whole = whole * 10 + (s[i] - '0');
      if (whole > 99999999999LL) return false;
    }
    cents = whole * 100;
  } else {
    for (int i = 0; i < dot; ++i) {
      whole = whole * 10 + (s[i] - '0');
      if (whole > 99999999999LL) return false;
    }
    int flen = (int)s.size() - dot - 1;
    for (int i = 0; i < flen; ++i) frac = frac * 10 + (s[dot + 1 + i] - '0');
    if (flen == 1) frac *= 10;
    cents = whole * 100 + frac;
  }
  if (positive && cents <= 0) return false;
  if (!positive && cents < 0) return false;
  return true;
}

bool Bookstore::valid_privilege(const std::string &s, int &out) {
  if (s.size() != 1 || s[0] < '0' || s[0] > '9') return false;
  out = s[0] - '0';
  return out == 1 || out == 3 || out == 7;
}

bool Bookstore::split_keywords(const std::string &kw,
                               std::vector<std::string> &parts, bool check_dup) {
  parts.clear();
  if (kw.empty()) return false;
  std::string cur;
  for (size_t i = 0; i < kw.size(); ++i) {
    if (kw[i] == '|') {
      if (cur.empty()) return false;
      parts.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(kw[i]);
    }
  }
  if (cur.empty()) return false;
  parts.push_back(cur);
  if (check_dup) {
    std::set<std::string> seen;
    for (size_t i = 0; i < parts.size(); ++i) {
      if (!seen.insert(parts[i]).second) return false;
    }
  }
  return true;
}

std::vector<std::string> Bookstore::tokenize(const std::string &line) {
  std::vector<std::string> tokens;
  std::string cur;
  bool in_quote = false;
  for (size_t i = 0; i < line.size(); ++i) {
    char c = line[i];
    if (c == '\r' || c == '\n') continue;
    if (c == '"') {
      in_quote = !in_quote;
      cur.push_back(c);
    } else if (c == ' ' && !in_quote) {
      if (!cur.empty()) {
        tokens.push_back(cur);
        cur.clear();
      }
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) tokens.push_back(cur);
  return tokens;
}

bool Bookstore::parse_typed_arg(const std::string &token, std::string &key,
                                std::string &val) {
  // forms: -ISBN=xxx  -name="xxx"  -author="xxx"  -keyword="xxx"  -price=xxx
  if (token.size() < 2 || token[0] != '-') return false;
  size_t eq = token.find('=');
  if (eq == std::string::npos || eq == 1) return false;
  key = token.substr(1, eq - 1);
  std::string raw = token.substr(eq + 1);
  if (key == "ISBN" || key == "price") {
    if (raw.empty()) return false;
    // no quotes
    if (raw[0] == '"') return false;
    val = raw;
    return true;
  }
  if (key == "name" || key == "author" || key == "keyword") {
    if (raw.size() < 2 || raw[0] != '"' || raw[raw.size() - 1] != '"')
      return false;
    val = raw.substr(1, raw.size() - 2);
    // quotes inside already stripped; val may be empty -> caller checks
    return true;
  }
  return false;
}

void Bookstore::print_book(const Book &b) {
  std::cout << b.isbn << "\t" << b.name << "\t" << b.author << "\t" << b.keyword
            << "\t" << format_money(b.price_cents) << "\t" << b.stock << "\n";
}

bool Bookstore::process_line(const std::string &line) {
  std::vector<std::string> tokens = tokenize(line);
  if (tokens.empty()) return true;  // blank line OK

  const std::string &cmd = tokens[0];
  if (cmd == "quit" || cmd == "exit") {
    if (tokens.size() != 1) {
      invalid();
      return true;
    }
    return false;
  }
  if (cmd == "su") {
    cmd_su(tokens);
  } else if (cmd == "logout") {
    cmd_logout(tokens);
  } else if (cmd == "register") {
    cmd_register(tokens);
  } else if (cmd == "passwd") {
    cmd_passwd(tokens);
  } else if (cmd == "useradd") {
    cmd_useradd(tokens);
  } else if (cmd == "delete") {
    cmd_delete(tokens);
  } else if (cmd == "show") {
    if (tokens.size() >= 2 && tokens[1] == "finance") {
      cmd_show_finance(tokens);
    } else {
      cmd_show(tokens);
    }
  } else if (cmd == "buy") {
    cmd_buy(tokens);
  } else if (cmd == "select") {
    cmd_select(tokens);
  } else if (cmd == "modify") {
    cmd_modify(tokens);
  } else if (cmd == "import") {
    cmd_import(tokens);
  } else if (cmd == "log") {
    cmd_log(tokens);
  } else if (cmd == "report") {
    if (tokens.size() == 2 && tokens[1] == "finance")
      cmd_report_finance(tokens);
    else if (tokens.size() == 2 && tokens[1] == "employee")
      cmd_report_employee(tokens);
    else
      invalid();
  } else {
    invalid();
  }
  return true;
}

void Bookstore::cmd_su(const std::vector<std::string> &args) {
  if (args.size() != 2 && args.size() != 3) {
    invalid();
    return;
  }
  const std::string &uid = args[1];
  if (!valid_id(uid)) {
    invalid();
    return;
  }
  std::map<std::string, int>::iterator it = user_idx_.find(uid);
  if (it == user_idx_.end()) {
    invalid();
    return;
  }
  User u;
  read_user(it->second, u);
  bool need_pass = true;
  if (priv() > u.privilege) need_pass = false;
  if (args.size() == 2) {
    if (need_pass) {
      invalid();
      return;
    }
  } else {
    if (!valid_id(args[2])) {
      invalid();
      return;
    }
    if (std::string(u.password) != args[2]) {
      invalid();
      return;
    }
  }
  LoginFrame frame;
  frame.userid = uid;
  frame.privilege = u.privilege;
  frame.has_selection = false;
  frame.selected_isbn.clear();
  stack_.push_back(frame);
  append_log("su " + uid);
}

void Bookstore::cmd_logout(const std::vector<std::string> &args) {
  if (args.size() != 1) {
    invalid();
    return;
  }
  if (priv() < 1) {
    invalid();
    return;
  }
  stack_.pop_back();
  append_log("logout");
}

void Bookstore::cmd_register(const std::vector<std::string> &args) {
  if (args.size() != 4) {
    invalid();
    return;
  }
  // privilege 0+
  if (!valid_id(args[1]) || !valid_id(args[2]) || !valid_username(args[3])) {
    invalid();
    return;
  }
  if (user_idx_.count(args[1])) {
    invalid();
    return;
  }
  User u;
  std::memset(&u, 0, sizeof(u));
  set_cstr(u.userid, sizeof(u.userid), args[1]);
  set_cstr(u.password, sizeof(u.password), args[2]);
  set_cstr(u.username, sizeof(u.username), args[3]);
  u.privilege = 1;
  u.active = 1;
  append_user(u);
  append_log("register " + args[1]);
}

void Bookstore::cmd_passwd(const std::vector<std::string> &args) {
  if (priv() < 1) {
    invalid();
    return;
  }
  if (args.size() != 3 && args.size() != 4) {
    invalid();
    return;
  }
  if (!valid_id(args[1])) {
    invalid();
    return;
  }
  std::map<std::string, int>::iterator it = user_idx_.find(args[1]);
  if (it == user_idx_.end()) {
    invalid();
    return;
  }
  User u;
  read_user(it->second, u);
  std::string newpass;
  if (args.size() == 3) {
    // omit current: only privilege 7
    if (priv() != 7) {
      invalid();
      return;
    }
    if (!valid_id(args[2])) {
      invalid();
      return;
    }
    newpass = args[2];
  } else {
    if (!valid_id(args[2]) || !valid_id(args[3])) {
      invalid();
      return;
    }
    if (std::string(u.password) != args[2]) {
      invalid();
      return;
    }
    newpass = args[3];
  }
  set_cstr(u.password, sizeof(u.password), newpass);
  write_user(it->second, u);
  append_log("passwd " + args[1]);
}

void Bookstore::cmd_useradd(const std::vector<std::string> &args) {
  if (priv() < 3) {
    invalid();
    return;
  }
  if (args.size() != 5) {
    invalid();
    return;
  }
  if (!valid_id(args[1]) || !valid_id(args[2]) || !valid_username(args[4])) {
    invalid();
    return;
  }
  int np;
  if (!valid_privilege(args[3], np)) {
    invalid();
    return;
  }
  if (np >= priv()) {
    invalid();
    return;
  }
  if (user_idx_.count(args[1])) {
    invalid();
    return;
  }
  User u;
  std::memset(&u, 0, sizeof(u));
  set_cstr(u.userid, sizeof(u.userid), args[1]);
  set_cstr(u.password, sizeof(u.password), args[2]);
  set_cstr(u.username, sizeof(u.username), args[4]);
  u.privilege = np;
  u.active = 1;
  append_user(u);
  append_log("useradd " + args[1]);
}

void Bookstore::cmd_delete(const std::vector<std::string> &args) {
  if (priv() < 7) {
    invalid();
    return;
  }
  if (args.size() != 2) {
    invalid();
    return;
  }
  if (!valid_id(args[1])) {
    invalid();
    return;
  }
  std::map<std::string, int>::iterator it = user_idx_.find(args[1]);
  if (it == user_idx_.end()) {
    invalid();
    return;
  }
  for (size_t i = 0; i < stack_.size(); ++i) {
    if (stack_[i].userid == args[1]) {
      invalid();
      return;
    }
  }
  User u;
  read_user(it->second, u);
  u.active = 0;
  write_user(it->second, u);
  user_idx_.erase(it);
  append_log("delete " + args[1]);
}

void Bookstore::cmd_show(const std::vector<std::string> &args) {
  if (priv() < 1) {
    invalid();
    return;
  }
  if (args.size() > 2) {
    invalid();
    return;
  }
  std::vector<int> ids;
  if (args.size() == 1) {
    // all books
    for (std::map<std::string, int>::iterator it = isbn_idx_.begin();
         it != isbn_idx_.end(); ++it)
      ids.push_back(it->second);
  } else {
    std::string key, val;
    if (!parse_typed_arg(args[1], key, val)) {
      invalid();
      return;
    }
    if (key == "ISBN") {
      if (!valid_isbn(val)) {
        invalid();
        return;
      }
      std::map<std::string, int>::iterator it = isbn_idx_.find(val);
      if (it != isbn_idx_.end()) ids.push_back(it->second);
    } else if (key == "name") {
      if (!valid_book_str(val, 60)) {
        invalid();
        return;
      }
      std::pair<std::multimap<std::string, int>::iterator,
                std::multimap<std::string, int>::iterator>
          range = name_idx_.equal_range(val);
      for (std::multimap<std::string, int>::iterator it = range.first;
           it != range.second; ++it)
        ids.push_back(it->second);
    } else if (key == "author") {
      if (!valid_book_str(val, 60)) {
        invalid();
        return;
      }
      std::pair<std::multimap<std::string, int>::iterator,
                std::multimap<std::string, int>::iterator>
          range = author_idx_.equal_range(val);
      for (std::multimap<std::string, int>::iterator it = range.first;
           it != range.second; ++it)
        ids.push_back(it->second);
    } else if (key == "keyword") {
      if (!valid_book_str(val, 60)) {
        invalid();
        return;
      }
      // single keyword only
      if (val.find('|') != std::string::npos) {
        invalid();
        return;
      }
      std::pair<std::multimap<std::string, int>::iterator,
                std::multimap<std::string, int>::iterator>
          range = keyword_idx_.equal_range(val);
      for (std::multimap<std::string, int>::iterator it = range.first;
           it != range.second; ++it)
        ids.push_back(it->second);
    } else {
      invalid();
      return;
    }
  }
  // unique + sort by ISBN
  std::vector<std::pair<std::string, int> > sorted;
  std::set<int> seen;
  for (size_t i = 0; i < ids.size(); ++i) {
    if (!seen.insert(ids[i]).second) continue;
    Book b;
    read_book(ids[i], b);
    if (!b.active) continue;
    sorted.push_back(std::make_pair(std::string(b.isbn), ids[i]));
  }
  std::sort(sorted.begin(), sorted.end());
  if (sorted.empty()) {
    std::cout << "\n";
    return;
  }
  for (size_t i = 0; i < sorted.size(); ++i) {
    Book b;
    read_book(sorted[i].second, b);
    print_book(b);
  }
}

void Bookstore::cmd_buy(const std::vector<std::string> &args) {
  if (priv() < 1) {
    invalid();
    return;
  }
  if (args.size() != 3) {
    invalid();
    return;
  }
  if (!valid_isbn(args[1])) {
    invalid();
    return;
  }
  int qty;
  if (!valid_quantity(args[2], qty)) {
    invalid();
    return;
  }
  std::map<std::string, int>::iterator it = isbn_idx_.find(args[1]);
  if (it == isbn_idx_.end()) {
    invalid();
    return;
  }
  Book b;
  read_book(it->second, b);
  if (b.stock < qty) {
    invalid();
    return;
  }
  long long total = b.price_cents * qty;
  b.stock -= qty;
  write_book(it->second, b);
  append_finance(total, 0);
  std::cout << format_money(total) << "\n";
  append_log("buy " + args[1] + " " + args[2]);
}

void Bookstore::cmd_select(const std::vector<std::string> &args) {
  if (priv() < 3) {
    invalid();
    return;
  }
  if (args.size() != 2) {
    invalid();
    return;
  }
  if (!valid_isbn(args[1])) {
    invalid();
    return;
  }
  std::map<std::string, int>::iterator it = isbn_idx_.find(args[1]);
  if (it == isbn_idx_.end()) {
    Book b;
    std::memset(&b, 0, sizeof(b));
    set_cstr(b.isbn, sizeof(b.isbn), args[1]);
    b.price_cents = 0;
    b.stock = 0;
    b.active = 1;
    append_book(b);
  }
  stack_.back().has_selection = true;
  stack_.back().selected_isbn = args[1];
  append_log("select " + args[1]);
}

void Bookstore::cmd_modify(const std::vector<std::string> &args) {
  if (priv() < 3) {
    invalid();
    return;
  }
  if (args.size() < 2) {
    invalid();
    return;
  }
  if (stack_.empty() || !stack_.back().has_selection) {
    invalid();
    return;
  }
  std::map<std::string, bool> seen_key;
  std::string new_isbn, new_name, new_author, new_keyword;
  long long new_price = 0;
  bool has_isbn = false, has_name = false, has_author = false, has_keyword = false,
       has_price = false;

  for (size_t i = 1; i < args.size(); ++i) {
    std::string key, val;
    if (!parse_typed_arg(args[i], key, val)) {
      invalid();
      return;
    }
    if (seen_key[key]) {
      invalid();
      return;
    }
    seen_key[key] = true;
    if (key == "ISBN") {
      if (!valid_isbn(val)) {
        invalid();
        return;
      }
      new_isbn = val;
      has_isbn = true;
    } else if (key == "name") {
      if (!valid_book_str(val, 60)) {
        invalid();
        return;
      }
      new_name = val;
      has_name = true;
    } else if (key == "author") {
      if (!valid_book_str(val, 60)) {
        invalid();
        return;
      }
      new_author = val;
      has_author = true;
    } else if (key == "keyword") {
      if (!valid_book_str(val, 60)) {
        invalid();
        return;
      }
      std::vector<std::string> parts;
      if (!split_keywords(val, parts, true)) {
        invalid();
        return;
      }
      new_keyword = val;
      has_keyword = true;
    } else if (key == "price") {
      if (!valid_price(val, new_price, false)) {
        invalid();
        return;
      }
      has_price = true;
    } else {
      invalid();
      return;
    }
  }

  std::string cur_isbn = stack_.back().selected_isbn;
  std::map<std::string, int>::iterator it = isbn_idx_.find(cur_isbn);
  if (it == isbn_idx_.end()) {
    invalid();
    return;
  }
  int book_idx = it->second;
  Book b;
  read_book(book_idx, b);

  if (has_isbn) {
    if (new_isbn == cur_isbn) {
      invalid();
      return;
    }
    if (isbn_idx_.count(new_isbn)) {
      invalid();
      return;
    }
  }

  // apply (save index first — index_remove_book erases isbn_idx_ entries)
  index_remove_book(book_idx, b);
  if (has_isbn) set_cstr(b.isbn, sizeof(b.isbn), new_isbn);
  if (has_name) set_cstr(b.name, sizeof(b.name), new_name);
  if (has_author) set_cstr(b.author, sizeof(b.author), new_author);
  if (has_keyword) set_cstr(b.keyword, sizeof(b.keyword), new_keyword);
  if (has_price) b.price_cents = new_price;
  write_book(book_idx, b);
  index_add_book(book_idx, b);

  if (has_isbn) stack_.back().selected_isbn = new_isbn;
  append_log("modify");
}

void Bookstore::cmd_import(const std::vector<std::string> &args) {
  if (priv() < 3) {
    invalid();
    return;
  }
  if (args.size() != 3) {
    invalid();
    return;
  }
  if (stack_.empty() || !stack_.back().has_selection) {
    invalid();
    return;
  }
  int qty;
  if (!valid_quantity(args[1], qty)) {
    invalid();
    return;
  }
  long long cost;
  if (!valid_price(args[2], cost, true)) {
    invalid();
    return;
  }
  std::map<std::string, int>::iterator it =
      isbn_idx_.find(stack_.back().selected_isbn);
  if (it == isbn_idx_.end()) {
    invalid();
    return;
  }
  Book b;
  read_book(it->second, b);
  // stock overflow check
  if ((long long)b.stock + qty > 2147483647LL) {
    invalid();
    return;
  }
  b.stock += qty;
  write_book(it->second, b);
  append_finance(0, cost);
  append_log("import");
}

void Bookstore::cmd_show_finance(const std::vector<std::string> &args) {
  if (priv() < 7) {
    invalid();
    return;
  }
  // show finance  OR  show finance [Count]
  if (args.size() != 2 && args.size() != 3) {
    invalid();
    return;
  }
  int count = fin_count_;
  if (args.size() == 3) {
    // Count: digits, max 10, can be 0
    const std::string &s = args[2];
    if (s.empty() || s.size() > 10) {
      invalid();
      return;
    }
    for (size_t i = 0; i < s.size(); ++i)
      if (s[i] < '0' || s[i] > '9') {
        invalid();
        return;
      }
    long long v = 0;
    for (size_t i = 0; i < s.size(); ++i) {
      v = v * 10 + (s[i] - '0');
      if (v > 2147483647LL) {
        invalid();
        return;
      }
    }
    count = (int)v;
    if (count > fin_count_) {
      invalid();
      return;
    }
    if (count == 0) {
      std::cout << "\n";
      return;
    }
  }
  long long income = 0, expense = 0;
  int start = fin_count_ - count;
  for (int i = start; i < fin_count_; ++i) {
    long long inc = 0, exp = 0;
    fin_file_.clear();
    fin_file_.seekg(FIN_HEADER + (long long)i * sizeof(long long) * 2);
    fin_file_.read(reinterpret_cast<char *>(&inc), sizeof(long long));
    fin_file_.read(reinterpret_cast<char *>(&exp), sizeof(long long));
    income += inc;
    expense += exp;
  }
  std::cout << "+ " << format_money(income) << " - " << format_money(expense)
            << "\n";
}

void Bookstore::cmd_log(const std::vector<std::string> &args) {
  if (priv() < 7) {
    invalid();
    return;
  }
  if (args.size() != 1) {
    invalid();
    return;
  }
  std::cout << "==== System Log ====\n";
  log_file_.flush();
  std::ifstream in("oplog.dat");
  std::string line;
  while (std::getline(in, line)) std::cout << line << "\n";
  std::cout << "==== Finance (" << fin_count_ << " txs) ====\n";
  for (int i = 0; i < fin_count_; ++i) {
    long long inc = 0, exp = 0;
    fin_file_.clear();
    fin_file_.seekg(FIN_HEADER + (long long)i * sizeof(long long) * 2);
    fin_file_.read(reinterpret_cast<char *>(&inc), sizeof(long long));
    fin_file_.read(reinterpret_cast<char *>(&exp), sizeof(long long));
    if (inc)
      std::cout << "income " << format_money(inc) << "\n";
    else
      std::cout << "expense " << format_money(exp) << "\n";
  }
}

void Bookstore::cmd_report_finance(const std::vector<std::string> &args) {
  if (priv() < 7) {
    invalid();
    return;
  }
  if (args.size() != 2) {
    invalid();
    return;
  }
  long long income = 0, expense = 0;
  for (int i = 0; i < fin_count_; ++i) {
    long long inc = 0, exp = 0;
    fin_file_.clear();
    fin_file_.seekg(FIN_HEADER + (long long)i * sizeof(long long) * 2);
    fin_file_.read(reinterpret_cast<char *>(&inc), sizeof(long long));
    fin_file_.read(reinterpret_cast<char *>(&exp), sizeof(long long));
    income += inc;
    expense += exp;
  }
  std::cout << "Finance Report\n";
  std::cout << "Income: " << format_money(income) << "\n";
  std::cout << "Expense: " << format_money(expense) << "\n";
  std::cout << "Net: " << format_money(income - expense) << "\n";
}

void Bookstore::cmd_report_employee(const std::vector<std::string> &args) {
  if (priv() < 7) {
    invalid();
    return;
  }
  if (args.size() != 2) {
    invalid();
    return;
  }
  std::cout << "Employee Report\n";
  for (std::map<std::string, int>::iterator it = user_idx_.begin();
       it != user_idx_.end(); ++it) {
    User u;
    read_user(it->second, u);
    if (u.privilege == 3)
      std::cout << u.userid << "\t" << u.username << "\n";
  }
}
