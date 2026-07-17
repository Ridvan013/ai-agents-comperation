#ifndef BOOKSTORE_RECORDS_HPP
#define BOOKSTORE_RECORDS_HPP

#include "common.hpp"

namespace bookstore {

/// BlockFile requires a metadata struct; these stores keep no extra state
/// beyond what BlockFile's own header already tracks.
struct EmptyMeta {
  int reserved;
};

struct BookRecord {
  IsbnStr isbn;
  TextStr name;
  TextStr author;
  TextStr keyword;
  long long price;  // Cents.
  long long stock;  // Repeated imports can exceed a 32-bit count.
};

struct UserRecord {
  AccountStr userid;
  AccountStr password;
  AccountStr username;
  int privilege;
};

/// Financial history is stored as running totals rather than per-transaction
/// deltas, so `show finance [Count]` is answered with two block reads and a
/// subtraction instead of a scan over the whole history.
struct FinanceRecord {
  long long totalIncome;
  long long totalExpense;
};

enum LogKind {
  kLogOperation = 0,
  kLogIncome = 1,
  kLogExpense = 2,
};

struct LogRecord {
  int kind;
  int privilege;      // Privilege of the acting account at the time.
  long long amount;   // Cents; only meaningful for the finance kinds.
  AccountStr user;    // Empty for actions taken by a guest.
  FixedStr<128> text;
};

}  // namespace bookstore

#endif  // BOOKSTORE_RECORDS_HPP
