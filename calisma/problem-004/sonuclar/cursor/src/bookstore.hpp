#ifndef BOOKSTORE_HPP
#define BOOKSTORE_HPP

#include <fstream>
#include <map>
#include <string>
#include <vector>

class Bookstore {
 public:
  Bookstore();
  ~Bookstore();

  // returns false if program should terminate
  bool process_line(const std::string &line);

 private:
  struct User {
    char userid[31];
    char password[31];
    char username[31];
    int privilege;
    int active;
  };

  struct Book {
    char isbn[21];
    char name[61];
    char author[61];
    char keyword[61];
    long long price_cents;
    int stock;
    int active;
  };

  struct LoginFrame {
    std::string userid;
    int privilege;
    std::string selected_isbn;
    bool has_selection;
  };

  std::vector<LoginFrame> stack_;
  std::fstream user_file_;
  std::fstream book_file_;
  std::fstream fin_file_;
  std::fstream log_file_;

  int user_count_;
  int book_count_;
  int fin_count_;

  std::map<std::string, int> user_idx_;
  std::map<std::string, int> isbn_idx_;
  std::multimap<std::string, int> name_idx_;
  std::multimap<std::string, int> author_idx_;
  std::multimap<std::string, int> keyword_idx_;

  void open_files();
  void init_root();
  void rebuild_indexes();

  bool read_user(int idx, User &u);
  void write_user(int idx, const User &u);
  int append_user(const User &u);

  bool read_book(int idx, Book &b);
  void write_book(int idx, const Book &b);
  int append_book(const Book &b);

  void append_finance(long long income, long long expense);
  void append_log(const std::string &msg);

  int priv() const;
  void invalid();
  static void print_money(long long cents);
  static std::string format_money(long long cents);

  static bool valid_id(const std::string &s);
  static bool valid_username(const std::string &s);
  static bool valid_isbn(const std::string &s);
  static bool valid_book_str(const std::string &s, int maxlen);
  static bool valid_quantity(const std::string &s, int &out);
  static bool valid_price(const std::string &s, long long &cents, bool positive);
  static bool valid_privilege(const std::string &s, int &out);
  static bool split_keywords(const std::string &kw, std::vector<std::string> &parts,
                             bool check_dup);
  static bool is_visible_ascii(unsigned char c);
  static std::vector<std::string> tokenize(const std::string &line);

  void index_add_book(int idx, const Book &b);
  void index_remove_book(int idx, const Book &b);

  void cmd_su(const std::vector<std::string> &args);
  void cmd_logout(const std::vector<std::string> &args);
  void cmd_register(const std::vector<std::string> &args);
  void cmd_passwd(const std::vector<std::string> &args);
  void cmd_useradd(const std::vector<std::string> &args);
  void cmd_delete(const std::vector<std::string> &args);
  void cmd_show(const std::vector<std::string> &args);
  void cmd_buy(const std::vector<std::string> &args);
  void cmd_select(const std::vector<std::string> &args);
  void cmd_modify(const std::vector<std::string> &args);
  void cmd_import(const std::vector<std::string> &args);
  void cmd_show_finance(const std::vector<std::string> &args);
  void cmd_log(const std::vector<std::string> &args);
  void cmd_report_finance(const std::vector<std::string> &args);
  void cmd_report_employee(const std::vector<std::string> &args);

  bool parse_typed_arg(const std::string &token, std::string &key, std::string &val);
  void print_book(const Book &b);
};

#endif
