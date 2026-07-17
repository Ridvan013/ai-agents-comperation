#ifndef BOOKSTORE_LOG_SYSTEM_HPP
#define BOOKSTORE_LOG_SYSTEM_HPP

#include <string>
#include <vector>

#include "block_file.hpp"
#include "records.hpp"

namespace bookstore {

/// Financial history and the operation journal.
class LogSystem {
 public:
  void open();
  void close();

  /// Appends a completed transaction. Exactly one of the two amounts is
  /// non-zero: `buy` earns income, `import` costs money.
  void recordIncome(const std::string &user, int privilege, long long amount,
                    const std::string &text);
  void recordExpense(const std::string &user, int privilege, long long amount,
                     const std::string &text);

  /// Appends a non-financial action to the journal.
  void recordOperation(const std::string &user, int privilege, const std::string &text);

  void showFinance(const std::vector<std::string> &tokens);
  void printLog();
  void reportFinance();
  void reportEmployee();

 private:
  void appendFinance(long long income, long long expense);
  void appendLog(const LogRecord &record);

  /// Running totals after transaction `index`; zeroes for index < 0.
  FinanceRecord totalsAt(int index);

  /// Neither store ever frees a block, so BlockFile's block count is exactly
  /// the number of appended entries.
  int financeCount() { return finance_.blockCount(); }
  int logCount() { return log_.blockCount(); }

  BlockFile<EmptyMeta, FinanceRecord, 64> finance_;
  BlockFile<EmptyMeta, LogRecord, 64> log_;
};

}  // namespace bookstore

#endif  // BOOKSTORE_LOG_SYSTEM_HPP
