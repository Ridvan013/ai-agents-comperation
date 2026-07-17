#include "bookstore.hpp"

#include <iostream>

namespace bookstore {

namespace {

/// Commands worth journalling: everything that changes state, minus `buy` and
/// `import`, which record themselves together with their financial entry.
/// Pure queries are left out so the journal stays a record of what was done.
bool isJournalled(const std::string &command) {
  return command == "su" || command == "logout" || command == "register" || command == "passwd" ||
         command == "useradd" || command == "delete" || command == "select" || command == "modify";
}

std::string joinTokens(const std::vector<std::string> &tokens) {
  std::string joined;
  for (size_t i = 0; i < tokens.size(); ++i) {
    if (i) joined += ' ';
    joined += tokens[i];
  }
  return joined;
}

}  // namespace

std::vector<std::string> tokenize(const std::string &line) {
  std::vector<std::string> tokens;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && line[i] == ' ') ++i;
    if (i >= line.size()) break;
    size_t begin = i;
    while (i < line.size() && line[i] != ' ') ++i;
    tokens.emplace_back(line, begin, i - begin);
  }
  return tokens;
}

void Bookstore::open() {
  accounts_.open();
  logs_.open();
  books_.open();
}

void Bookstore::close() {
  books_.close();
  logs_.close();
  accounts_.close();
}

bool Bookstore::executeLine(const std::string &line) {
  const std::vector<std::string> tokens = tokenize(line);
  if (tokens.empty()) return true;  // A blank line is legal and does nothing.

  // Capture the actor up front: `su` and `logout` move the login stack, but the
  // journal should attribute the command to whoever issued it.
  const std::string actor = accounts_.currentUserId();
  const int privilege = accounts_.currentPrivilege();

  try {
    if (tokens[0] == "quit" || tokens[0] == "exit") {
      failIf(tokens.size() != 1);
      return false;
    }
    dispatch(tokens);
    if (isJournalled(tokens[0])) logs_.recordOperation(actor, privilege, joinTokens(tokens));
  } catch (const InvalidCommand &) {
    std::cout << "Invalid\n";
  }
  return true;
}

/// The privilege levels below mirror the command table in the specification.
void Bookstore::dispatch(const std::vector<std::string> &tokens) {
  const std::string &command = tokens[0];

  if (command == "su") {
    accounts_.su(tokens);
  } else if (command == "logout") {
    accounts_.requirePrivilege(1);
    accounts_.logout(tokens);
  } else if (command == "register") {
    accounts_.registerAccount(tokens);
  } else if (command == "passwd") {
    accounts_.requirePrivilege(1);
    accounts_.changePassword(tokens);
  } else if (command == "useradd") {
    accounts_.requirePrivilege(3);
    accounts_.addAccount(tokens);
  } else if (command == "delete") {
    accounts_.requirePrivilege(7);
    accounts_.deleteAccount(tokens);
  } else if (command == "show") {
    // `show finance` is a log command that happens to share the keyword.
    if (tokens.size() >= 2 && tokens[1] == "finance") {
      accounts_.requirePrivilege(7);
      logs_.showFinance(tokens);
    } else {
      accounts_.requirePrivilege(1);
      books_.show(tokens);
    }
  } else if (command == "buy") {
    accounts_.requirePrivilege(1);
    books_.buy(tokens);
  } else if (command == "select") {
    accounts_.requirePrivilege(3);
    books_.select(tokens);
  } else if (command == "modify") {
    accounts_.requirePrivilege(3);
    books_.modify(tokens);
  } else if (command == "import") {
    accounts_.requirePrivilege(3);
    books_.importBooks(tokens);
  } else if (command == "log") {
    accounts_.requirePrivilege(7);
    failIf(tokens.size() != 1);
    logs_.printLog();
  } else if (command == "report") {
    accounts_.requirePrivilege(7);
    failIf(tokens.size() != 2);
    if (tokens[1] == "finance")
      logs_.reportFinance();
    else if (tokens[1] == "employee")
      logs_.reportEmployee();
    else
      fail();
  } else {
    fail();
  }
}

}  // namespace bookstore
