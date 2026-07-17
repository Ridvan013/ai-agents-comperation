#ifndef BOOKSTORE_ACCOUNT_SYSTEM_HPP
#define BOOKSTORE_ACCOUNT_SYSTEM_HPP

#include <string>
#include <vector>

#include "block_file.hpp"
#include "bplus_tree.hpp"
#include "records.hpp"

namespace bookstore {

/// Accounts, privileges and the nested login stack.
class AccountSystem {
 public:
  /// One `su` that has not been undone by a `logout`. The selected book belongs
  /// to the frame rather than to the account, so that logging out restores the
  /// selection of the account underneath, and a second login of the same
  /// account starts with nothing selected.
  struct LoginFrame {
    std::string userid;
    int privilege;
    int selectedBook;  // Book record id, or -1.
  };

  void open();
  void close();

  int currentPrivilege() const { return stack_.empty() ? 0 : stack_.back().privilege; }
  std::string currentUserId() const { return stack_.empty() ? std::string() : stack_.back().userid; }
  bool loggedIn() const { return !stack_.empty(); }

  int selectedBook() const { return stack_.empty() ? -1 : stack_.back().selectedBook; }
  void setSelectedBook(int id) {
    if (!stack_.empty()) stack_.back().selectedBook = id;
  }

  /// Aborts with `Invalid` unless the current account is privileged enough.
  void requirePrivilege(int level) const { failIf(currentPrivilege() < level); }

  // Command handlers; each throws InvalidCommand on any failure.
  void su(const std::vector<std::string> &tokens);
  void logout(const std::vector<std::string> &tokens);
  void registerAccount(const std::vector<std::string> &tokens);
  void changePassword(const std::vector<std::string> &tokens);
  void addAccount(const std::vector<std::string> &tokens);
  void deleteAccount(const std::vector<std::string> &tokens);

 private:
  /// Looks a user up by id; returns its record id, or -1 when absent.
  int locate(const std::string &userid, UserRecord &out);
  void create(const std::string &userid, const std::string &password, const std::string &username,
              int privilege);
  bool isLoggedIn(const std::string &userid) const;

  BlockFile<EmptyMeta, UserRecord> users_;
  BPlusTree<AccountStr, int> index_;
  std::vector<LoginFrame> stack_;
};

}  // namespace bookstore

#endif  // BOOKSTORE_ACCOUNT_SYSTEM_HPP
