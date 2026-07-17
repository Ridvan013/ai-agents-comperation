#ifndef BOOKSTORE_BOOKSTORE_HPP
#define BOOKSTORE_BOOKSTORE_HPP

#include <string>
#include <vector>

#include "account_system.hpp"
#include "book_system.hpp"
#include "log_system.hpp"

namespace bookstore {

/// Splits a command line on spaces. Runs of spaces collapse, and leading and
/// trailing spaces are ignored, per the spec's whitespace rules.
std::vector<std::string> tokenize(const std::string &line);

/// Owns the three subsystems and routes commands between them.
class Bookstore {
 public:
  Bookstore() : books_(accounts_, logs_) {}

  void open();
  void close();

  /// Runs one input line, printing whatever it produces. Returns false when the
  /// line was `quit` or `exit` and the system should shut down.
  bool executeLine(const std::string &line);

 private:
  void dispatch(const std::vector<std::string> &tokens);

  AccountSystem accounts_;
  LogSystem logs_;
  BookSystem books_;
};

}  // namespace bookstore

#endif  // BOOKSTORE_BOOKSTORE_HPP
