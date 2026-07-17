#include "log_system.hpp"

#include <iostream>
#include <map>

namespace bookstore {

void LogSystem::open() {
  finance_.open("finance.dat", EmptyMeta{0});
  log_.open("log.dat", EmptyMeta{0});
}

void LogSystem::close() {
  finance_.close();
  log_.close();
}

FinanceRecord LogSystem::totalsAt(int index) {
  if (index < 0) return FinanceRecord{0, 0};
  FinanceRecord record;
  finance_.read(index, record);
  return record;
}

void LogSystem::appendFinance(long long income, long long expense) {
  FinanceRecord record = totalsAt(financeCount() - 1);
  record.totalIncome += income;
  record.totalExpense += expense;
  finance_.write(finance_.allocate(), record);
}

void LogSystem::appendLog(const LogRecord &record) { log_.write(log_.allocate(), record); }

void LogSystem::recordIncome(const std::string &user, int privilege, long long amount,
                             const std::string &text) {
  appendFinance(amount, 0);
  LogRecord record{};
  record.kind = kLogIncome;
  record.privilege = privilege;
  record.amount = amount;
  record.user = AccountStr(user);
  record.text = FixedStr<128>(text);
  appendLog(record);
}

void LogSystem::recordExpense(const std::string &user, int privilege, long long amount,
                              const std::string &text) {
  appendFinance(0, amount);
  LogRecord record{};
  record.kind = kLogExpense;
  record.privilege = privilege;
  record.amount = amount;
  record.user = AccountStr(user);
  record.text = FixedStr<128>(text);
  appendLog(record);
}

void LogSystem::recordOperation(const std::string &user, int privilege, const std::string &text) {
  LogRecord record{};
  record.kind = kLogOperation;
  record.privilege = privilege;
  record.amount = 0;
  record.user = AccountStr(user);
  record.text = FixedStr<128>(text);
  appendLog(record);
}

void LogSystem::showFinance(const std::vector<std::string> &tokens) {
  failIf(tokens.size() != 2 && tokens.size() != 3);
  const int count = financeCount();

  int wanted = count;
  if (tokens.size() == 3) {
    long long requested;
    failIf(!parseCount(tokens[2], requested));
    failIf(requested > count);
    if (requested == 0) {
      std::cout << '\n';
      return;
    }
    wanted = static_cast<int>(requested);
  }

  // Running totals turn "the last `wanted` transactions" into a subtraction.
  const FinanceRecord latest = totalsAt(count - 1);
  const FinanceRecord before = totalsAt(count - wanted - 1);
  std::cout << "+ " << formatMoney(latest.totalIncome - before.totalIncome) << " - "
            << formatMoney(latest.totalExpense - before.totalExpense) << '\n';
}

void LogSystem::printLog() {
  const int count = logCount();
  std::cout << "===== System Log (" << count << " entries) =====\n";
  LogRecord record;
  for (int i = 0; i < count; ++i) {
    log_.read(i, record);
    const std::string user = record.user.empty() ? std::string("<guest>") : record.user.str();
    std::cout << '#' << (i + 1) << " [" << user << " {" << record.privilege << "}] ";
    switch (record.kind) {
      case kLogIncome:
        std::cout << "income " << formatMoney(record.amount) << " : " << record.text.str();
        break;
      case kLogExpense:
        std::cout << "expense " << formatMoney(record.amount) << " : " << record.text.str();
        break;
      default:
        std::cout << record.text.str();
        break;
    }
    std::cout << '\n';
  }
  std::cout << "===== End of Log =====\n";
}

void LogSystem::reportFinance() {
  const int count = financeCount();
  std::cout << "===== Finance Report =====\n";
  std::cout << "Transactions: " << count << '\n';

  const int entries = logCount();
  LogRecord record;
  int index = 0;
  for (int i = 0; i < entries; ++i) {
    log_.read(i, record);
    if (record.kind != kLogIncome && record.kind != kLogExpense) continue;
    ++index;
    std::cout << "  [" << index << "] " << (record.kind == kLogIncome ? '+' : '-') << ' '
              << formatMoney(record.amount) << "  by "
              << (record.user.empty() ? std::string("<guest>") : record.user.str()) << "  ("
              << record.text.str() << ")\n";
  }

  const FinanceRecord totals = totalsAt(count - 1);
  std::cout << "--------------------------\n";
  std::cout << "Total income : " << formatMoney(totals.totalIncome) << '\n';
  std::cout << "Total expense: " << formatMoney(totals.totalExpense) << '\n';
  std::cout << "Net profit   : " << formatMoney(totals.totalIncome - totals.totalExpense) << '\n';
  std::cout << "===== End of Report =====\n";
}

void LogSystem::reportEmployee() {
  struct Summary {
    long long operations = 0;
    long long income = 0;
    std::map<std::string, long long> byCommand;
  };

  // Aggregating in a single pass keeps the whole journal off the heap; the map
  // is bounded by the number of staff accounts, not by the number of entries.
  std::map<std::string, Summary> staff;
  const int count = logCount();
  LogRecord record;
  for (int i = 0; i < count; ++i) {
    log_.read(i, record);
    if (record.privilege < 3) continue;  // Employees and the owner only.
    Summary &summary = staff[record.user.str()];
    ++summary.operations;
    if (record.kind == kLogIncome) summary.income += record.amount;

    const std::string text = record.text.str();
    const size_t space = text.find(' ');
    summary.byCommand[text.substr(0, space)] += 1;
  }

  std::cout << "===== Employee Report =====\n";
  for (const auto &entry : staff) {
    std::cout << "Employee: " << entry.first << '\n';
    std::cout << "  operations: " << entry.second.operations << '\n';
    std::cout << "  income generated: " << formatMoney(entry.second.income) << '\n';
    for (const auto &command : entry.second.byCommand)
      std::cout << "    " << command.first << " x" << command.second << '\n';
  }
  if (staff.empty()) std::cout << "(no employee activity recorded)\n";
  std::cout << "===== End of Report =====\n";
}

}  // namespace bookstore
