#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#endif

namespace {

constexpr const char *kDbDirectory = ".bookstore_db";
constexpr const char *kUserFileName = "users.dat";
constexpr const char *kBookFileName = "books.dat";
constexpr const char *kFinanceFileName = "finance.dat";
constexpr const char *kLogFileName = "operations.log";

using Offset = std::int64_t;

#pragma pack(push, 1)
struct UserRecord {
  char user_id[31];
  char password[31];
  char user_name[31];
  std::int32_t privilege;
  std::int32_t deleted;
};

struct BookRecord {
  char isbn[21];
  char name[61];
  char author[61];
  char keyword[61];
  std::int64_t price_cents;
  std::int32_t stock;
  std::int32_t deleted;
};

struct FinanceRecord {
  std::int64_t income_cents;
  std::int64_t expense_cents;
};
#pragma pack(pop)

struct Session {
  std::string user_id;
  Offset selected_book = -1;
};

struct ModifyRequest {
  bool has_isbn = false;
  bool has_name = false;
  bool has_author = false;
  bool has_keyword = false;
  bool has_price = false;
  std::string isbn;
  std::string name;
  std::string author;
  std::string keyword;
  std::int64_t price_cents = 0;
};

template <std::size_t N>
std::string FromArray(const char (&buffer)[N]) {
  std::size_t length = 0;
  while (length < N && buffer[length] != '\0') {
    ++length;
  }
  return std::string(buffer, length);
}

template <std::size_t N>
void ToArray(const std::string &value, char (&buffer)[N]) {
  std::fill(std::begin(buffer), std::end(buffer), '\0');
  const std::size_t copy_length = std::min<std::size_t>(value.size(), N - 1);
  std::copy_n(value.data(), copy_length, buffer);
}

bool IsVisibleAscii(char ch) { return ch >= 32 && ch <= 126; }

bool IsAllVisibleAscii(const std::string &text) {
  return std::all_of(text.begin(), text.end(),
                     [](char ch) { return IsVisibleAscii(ch); });
}

bool IsIdentifier(const std::string &text, std::size_t max_length) {
  if (text.empty() || text.size() > max_length) {
    return false;
  }
  return std::all_of(text.begin(), text.end(), [](char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
  });
}

bool IsGeneralField(const std::string &text, std::size_t max_length) {
  if (text.empty() || text.size() > max_length) {
    return false;
  }
  return IsAllVisibleAscii(text);
}

bool IsQuotedField(const std::string &text, std::size_t max_length) {
  if (text.empty() || text.size() > max_length) {
    return false;
  }
  return std::all_of(text.begin(), text.end(), [](char ch) {
    return IsVisibleAscii(ch) && ch != '"';
  });
}

bool ParseUnsignedInt(const std::string &text, std::int32_t &value) {
  if (text.empty() || text.size() > 10) {
    return false;
  }
  if (!std::all_of(text.begin(), text.end(),
                   [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)); })) {
    return false;
  }
  std::int64_t result = 0;
  for (char ch : text) {
    result = result * 10 + (ch - '0');
    if (result > std::numeric_limits<std::int32_t>::max()) {
      return false;
    }
  }
  value = static_cast<std::int32_t>(result);
  return true;
}

bool ParseMoney(const std::string &text, std::int64_t &cents) {
  if (text.empty() || text.size() > 13) {
    return false;
  }
  const std::size_t dot_position = text.find('.');
  if (dot_position == std::string::npos || dot_position == 0 ||
      dot_position + 3 != text.size()) {
    return false;
  }
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char ch = text[i];
    if (i == dot_position) {
      continue;
    }
    if (!std::isdigit(static_cast<unsigned char>(ch))) {
      return false;
    }
  }
  std::int64_t whole = 0;
  for (std::size_t i = 0; i < dot_position; ++i) {
    whole = whole * 10 + (text[i] - '0');
  }
  const std::int64_t fraction =
      (text[dot_position + 1] - '0') * 10 + (text[dot_position + 2] - '0');
  cents = whole * 100 + fraction;
  return true;
}

std::string MoneyToString(std::int64_t cents) {
  std::ostringstream output;
  output << (cents < 0 ? "-" : "");
  const std::int64_t absolute = std::llabs(cents);
  output << (absolute / 100) << '.' << std::setw(2) << std::setfill('0')
         << (absolute % 100);
  return output.str();
}

std::vector<std::string> SplitKeywords(const std::string &keywords) {
  std::vector<std::string> parts;
  std::string current;
  for (char ch : keywords) {
    if (ch == '|') {
      parts.push_back(current);
      current.clear();
    } else {
      current.push_back(ch);
    }
  }
  parts.push_back(current);
  return parts;
}

bool ValidateKeywords(const std::string &keywords, bool allow_multiple) {
  if (!IsQuotedField(keywords, 60)) {
    return false;
  }
  const std::vector<std::string> parts = SplitKeywords(keywords);
  if (!allow_multiple && parts.size() != 1) {
    return false;
  }
  std::set<std::string> unique_parts;
  for (const std::string &part : parts) {
    if (part.empty() || !unique_parts.insert(part).second) {
      return false;
    }
  }
  return true;
}

bool TokenizeLine(const std::string &line, std::vector<std::string> &tokens) {
  tokens.clear();
  std::string current;
  bool inside_quotes = false;
  for (char ch : line) {
    if (!IsVisibleAscii(ch)) {
      return false;
    }
    if (ch == '"') {
      inside_quotes = !inside_quotes;
      current.push_back(ch);
      continue;
    }
    if (ch == ' ' && !inside_quotes) {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      continue;
    }
    current.push_back(ch);
  }
  if (inside_quotes) {
    return false;
  }
  if (!current.empty()) {
    tokens.push_back(current);
  }
  return true;
}

std::string StripQuotes(const std::string &text, bool &ok) {
  ok = text.size() >= 2 && text.front() == '"' && text.back() == '"';
  if (!ok) {
    return {};
  }
  return text.substr(1, text.size() - 2);
}

class UserStore {
 public:
  explicit UserStore(const std::string &file_path)
      : file_path_(file_path) {}

  void Initialize() {
    EnsureFileExists();
    file_.open(file_path_, std::ios::in | std::ios::out | std::ios::binary);
    RebuildIndex();
    if (index_.empty()) {
      UserRecord root{};
      ToArray(std::string("root"), root.user_id);
      ToArray(std::string("sjtu"), root.password);
      ToArray(std::string("root"), root.user_name);
      root.privilege = 7;
      root.deleted = 0;
      AppendRecord(root);
      RebuildIndex();
    }
  }

  bool Exists(const std::string &user_id) const {
    return index_.find(user_id) != index_.end();
  }

  bool Read(const std::string &user_id, UserRecord &record) {
    const auto iter = index_.find(user_id);
    if (iter == index_.end()) {
      return false;
    }
    return ReadAt(iter->second, record);
  }

  bool ReadAt(Offset offset, UserRecord &record) {
    file_.clear();
    file_.seekg(offset);
    file_.read(reinterpret_cast<char *>(&record), sizeof(record));
    return static_cast<bool>(file_);
  }

  bool Create(const UserRecord &record) {
    const std::string user_id = FromArray(record.user_id);
    if (Exists(user_id)) {
      return false;
    }
    const Offset offset = AppendRecord(record);
    index_[user_id] = offset;
    return true;
  }

  bool Update(const std::string &user_id, const UserRecord &record) {
    const auto iter = index_.find(user_id);
    if (iter == index_.end()) {
      return false;
    }
    WriteAt(iter->second, record);
    return true;
  }

  bool Delete(const std::string &user_id) {
    const auto iter = index_.find(user_id);
    if (iter == index_.end()) {
      return false;
    }
    UserRecord record{};
    if (!ReadAt(iter->second, record)) {
      return false;
    }
    record.deleted = 1;
    WriteAt(iter->second, record);
    index_.erase(iter);
    return true;
  }

  std::int32_t PrivilegeOf(const std::string &user_id) {
    UserRecord record{};
    return Read(user_id, record) ? record.privilege : 0;
  }

 private:
  void EnsureFileExists() {
    std::ifstream input(file_path_, std::ios::binary);
    if (!input.good()) {
      std::ofstream create(file_path_, std::ios::binary);
    }
  }

  void RebuildIndex() {
    index_.clear();
    file_.clear();
    file_.seekg(0);
    Offset offset = 0;
    UserRecord record{};
    while (file_.read(reinterpret_cast<char *>(&record), sizeof(record))) {
      if (record.deleted == 0) {
        index_[FromArray(record.user_id)] = offset;
      }
      offset += static_cast<Offset>(sizeof(record));
    }
    file_.clear();
  }

  Offset AppendRecord(const UserRecord &record) {
    file_.clear();
    file_.seekp(0, std::ios::end);
    const Offset offset = static_cast<Offset>(file_.tellp());
    file_.write(reinterpret_cast<const char *>(&record), sizeof(record));
    file_.flush();
    return offset;
  }

  void WriteAt(Offset offset, const UserRecord &record) {
    file_.clear();
    file_.seekp(offset);
    file_.write(reinterpret_cast<const char *>(&record), sizeof(record));
    file_.flush();
  }

  std::string file_path_;
  std::fstream file_;
  std::unordered_map<std::string, Offset> index_;
};

class BookStore {
 public:
  explicit BookStore(const std::string &file_path)
      : file_path_(file_path) {}

  void Initialize() {
    EnsureFileExists();
    file_.open(file_path_, std::ios::in | std::ios::out | std::ios::binary);
    RebuildIndices();
  }

  bool ExistsByIsbn(const std::string &isbn) const {
    return isbn_index_.find(isbn) != isbn_index_.end();
  }

  bool ReadByIsbn(const std::string &isbn, BookRecord &record) {
    const auto iter = isbn_index_.find(isbn);
    if (iter == isbn_index_.end()) {
      return false;
    }
    return ReadAt(iter->second, record);
  }

  bool GetOffsetByIsbn(const std::string &isbn, Offset &offset) const {
    const auto iter = isbn_index_.find(isbn);
    if (iter == isbn_index_.end()) {
      return false;
    }
    offset = iter->second;
    return true;
  }

  bool ReadAt(Offset offset, BookRecord &record) {
    file_.clear();
    file_.seekg(offset);
    file_.read(reinterpret_cast<char *>(&record), sizeof(record));
    return static_cast<bool>(file_);
  }

  Offset EnsureBook(const std::string &isbn) {
    const auto iter = isbn_index_.find(isbn);
    if (iter != isbn_index_.end()) {
      return iter->second;
    }
    BookRecord record{};
    ToArray(isbn, record.isbn);
    record.price_cents = 0;
    record.stock = 0;
    record.deleted = 0;
    const Offset offset = AppendRecord(record);
    isbn_index_[isbn] = offset;
    return offset;
  }

  bool UpdateAt(Offset offset, const BookRecord &new_record) {
    BookRecord old_record{};
    if (!ReadAt(offset, old_record)) {
      return false;
    }
    RemoveFromSecondaryIndex(old_record);
    if (FromArray(old_record.isbn) != FromArray(new_record.isbn)) {
      isbn_index_.erase(FromArray(old_record.isbn));
      isbn_index_[FromArray(new_record.isbn)] = offset;
    }
    WriteAt(offset, new_record);
    AddToSecondaryIndex(new_record, offset);
    return true;
  }

  std::vector<Offset> ListAllByIsbn() const {
    std::vector<Offset> result;
    result.reserve(isbn_index_.size());
    for (std::map<std::string, Offset>::const_iterator it = isbn_index_.begin();
         it != isbn_index_.end(); ++it) {
      result.push_back(it->second);
    }
    return result;
  }

  std::vector<Offset> FindByName(const std::string &name) const {
    return CollectFromMultiIndex(name_index_, name);
  }

  std::vector<Offset> FindByAuthor(const std::string &author) const {
    return CollectFromMultiIndex(author_index_, author);
  }

  std::vector<Offset> FindByKeyword(const std::string &keyword) const {
    return CollectFromMultiIndex(keyword_index_, keyword);
  }

 private:
  void EnsureFileExists() {
    std::ifstream input(file_path_, std::ios::binary);
    if (!input.good()) {
      std::ofstream create(file_path_, std::ios::binary);
    }
  }

  void RebuildIndices() {
    isbn_index_.clear();
    name_index_.clear();
    author_index_.clear();
    keyword_index_.clear();
    file_.clear();
    file_.seekg(0);
    Offset offset = 0;
    BookRecord record{};
    while (file_.read(reinterpret_cast<char *>(&record), sizeof(record))) {
      if (record.deleted == 0) {
        isbn_index_[FromArray(record.isbn)] = offset;
        AddToSecondaryIndex(record, offset);
      }
      offset += static_cast<Offset>(sizeof(record));
    }
    file_.clear();
  }

  Offset AppendRecord(const BookRecord &record) {
    file_.clear();
    file_.seekp(0, std::ios::end);
    const Offset offset = static_cast<Offset>(file_.tellp());
    file_.write(reinterpret_cast<const char *>(&record), sizeof(record));
    file_.flush();
    AddToSecondaryIndex(record, offset);
    return offset;
  }

  void WriteAt(Offset offset, const BookRecord &record) {
    file_.clear();
    file_.seekp(offset);
    file_.write(reinterpret_cast<const char *>(&record), sizeof(record));
    file_.flush();
  }

  static void AddSingleIndex(std::map<std::string, std::set<std::string>> &index,
                             const std::string &key, const std::string &isbn) {
    if (!key.empty()) {
      index[key].insert(isbn);
    }
  }

  void AddToSecondaryIndex(const BookRecord &record, Offset offset) {
    (void)offset;
    const std::string isbn = FromArray(record.isbn);
    AddSingleIndex(name_index_, FromArray(record.name), isbn);
    AddSingleIndex(author_index_, FromArray(record.author), isbn);
    const std::string keywords = FromArray(record.keyword);
    if (!keywords.empty()) {
      for (const std::string &keyword : SplitKeywords(keywords)) {
        keyword_index_[keyword].insert(isbn);
      }
    }
  }

  void RemoveSingleIndex(std::map<std::string, std::set<std::string>> &index,
                         const std::string &key, const std::string &isbn) {
    if (key.empty()) {
      return;
    }
    auto iter = index.find(key);
    if (iter == index.end()) {
      return;
    }
    iter->second.erase(isbn);
    if (iter->second.empty()) {
      index.erase(iter);
    }
  }

  void RemoveFromSecondaryIndex(const BookRecord &record) {
    const std::string isbn = FromArray(record.isbn);
    RemoveSingleIndex(name_index_, FromArray(record.name), isbn);
    RemoveSingleIndex(author_index_, FromArray(record.author), isbn);
    const std::string keywords = FromArray(record.keyword);
    if (!keywords.empty()) {
      for (const std::string &keyword : SplitKeywords(keywords)) {
        RemoveSingleIndex(keyword_index_, keyword, isbn);
      }
    }
  }

  std::vector<Offset> CollectFromMultiIndex(
      const std::map<std::string, std::set<std::string>> &index,
      const std::string &key) const {
    std::vector<Offset> result;
    const auto iter = index.find(key);
    if (iter == index.end()) {
      return result;
    }
    result.reserve(iter->second.size());
    for (const std::string &isbn : iter->second) {
      result.push_back(isbn_index_.at(isbn));
    }
    return result;
  }

  std::string file_path_;
  std::fstream file_;
  std::map<std::string, Offset> isbn_index_;
  std::map<std::string, std::set<std::string>> name_index_;
  std::map<std::string, std::set<std::string>> author_index_;
  std::map<std::string, std::set<std::string>> keyword_index_;
};

class FinanceStore {
 public:
  explicit FinanceStore(const std::string &file_path)
      : file_path_(file_path) {}

  void Initialize() {
    EnsureFileExists();
    file_.open(file_path_, std::ios::in | std::ios::out | std::ios::binary);
    transactions_.clear();
    file_.clear();
    file_.seekg(0);
    FinanceRecord record{};
    while (file_.read(reinterpret_cast<char *>(&record), sizeof(record))) {
      transactions_.push_back(record);
    }
    file_.clear();
  }

  void Append(std::int64_t income_cents, std::int64_t expense_cents) {
    FinanceRecord record{income_cents, expense_cents};
    file_.clear();
    file_.seekp(0, std::ios::end);
    file_.write(reinterpret_cast<const char *>(&record), sizeof(record));
    file_.flush();
    transactions_.push_back(record);
  }

  std::size_t Count() const { return transactions_.size(); }

  std::pair<std::int64_t, std::int64_t> SumLast(std::size_t count) const {
    std::int64_t income = 0;
    std::int64_t expense = 0;
    const std::size_t begin = transactions_.size() - count;
    for (std::size_t i = begin; i < transactions_.size(); ++i) {
      income += transactions_[i].income_cents;
      expense += transactions_[i].expense_cents;
    }
    return {income, expense};
  }

  std::pair<std::int64_t, std::int64_t> SumAll() const {
    std::int64_t income = 0;
    std::int64_t expense = 0;
    for (const FinanceRecord &record : transactions_) {
      income += record.income_cents;
      expense += record.expense_cents;
    }
    return {income, expense};
  }

  const std::vector<FinanceRecord> &All() const { return transactions_; }

 private:
  void EnsureFileExists() {
    std::ifstream input(file_path_, std::ios::binary);
    if (!input.good()) {
      std::ofstream create(file_path_, std::ios::binary);
    }
  }

  std::string file_path_;
  std::fstream file_;
  std::vector<FinanceRecord> transactions_;
};

class LogStore {
 public:
  explicit LogStore(const std::string &file_path)
      : file_path_(file_path) {}

  void Initialize() {
    std::ifstream input(file_path_);
    if (!input.good()) {
      std::ofstream create(file_path_);
    }
  }

  void Append(const std::string &line) {
    std::ofstream output(file_path_, std::ios::app);
    output << line << '\n';
  }

  std::vector<std::string> ReadAll() const {
    std::vector<std::string> lines;
    std::ifstream input(file_path_);
    std::string line;
    while (std::getline(input, line)) {
      lines.push_back(line);
    }
    return lines;
  }

 private:
  std::string file_path_;
};

class BookstoreSystem {
 public:
  BookstoreSystem()
      : db_root_(kDbDirectory),
        users_(JoinPath(db_root_, kUserFileName)),
        books_(JoinPath(db_root_, kBookFileName)),
        finances_(JoinPath(db_root_, kFinanceFileName)),
        logs_(JoinPath(db_root_, kLogFileName)) {
    EnsureDirectory(db_root_);
    users_.Initialize();
    books_.Initialize();
    finances_.Initialize();
    logs_.Initialize();
  }

  bool Execute(const std::string &line, std::ostream &output, bool &should_exit) {
    should_exit = false;
    std::vector<std::string> tokens;
    if (!TokenizeLine(line, tokens)) {
      return false;
    }
    if (tokens.empty()) {
      return true;
    }
    const std::string &command = tokens.front();
    if (command == "quit" || command == "exit") {
      if (tokens.size() != 1) {
        return false;
      }
      should_exit = true;
      return true;
    }
    if (command == "su") {
      return HandleSu(tokens);
    }
    if (command == "logout") {
      return HandleLogout();
    }
    if (command == "register") {
      return HandleRegister(tokens);
    }
    if (command == "passwd") {
      return HandlePasswd(tokens);
    }
    if (command == "useradd") {
      return HandleUserAdd(tokens);
    }
    if (command == "delete") {
      return HandleDelete(tokens);
    }
    if (command == "show") {
      return HandleShow(tokens, output);
    }
    if (command == "buy") {
      return HandleBuy(tokens, output);
    }
    if (command == "select") {
      return HandleSelect(tokens);
    }
    if (command == "modify") {
      return HandleModify(tokens);
    }
    if (command == "import") {
      return HandleImport(tokens);
    }
    if (command == "log") {
      return HandleLog(tokens, output);
    }
    if (command == "report") {
      return HandleReport(tokens, output);
    }
    return false;
  }

 private:
  static std::string JoinPath(const std::string &directory,
                              const std::string &file_name) {
    return directory + "/" + file_name;
  }

  static void EnsureDirectory(const std::string &directory) {
#ifdef _WIN32
    _mkdir(directory.c_str());
#else
    mkdir(directory.c_str(), 0777);
#endif
  }

  std::int32_t CurrentPrivilege() {
    if (sessions_.empty()) {
      return 0;
    }
    return users_.PrivilegeOf(sessions_.back().user_id);
  }

  std::string CurrentActor() const {
    return sessions_.empty() ? std::string("guest") : sessions_.back().user_id;
  }

  bool RequirePrivilege(std::int32_t privilege) {
    return CurrentPrivilege() >= privilege;
  }

  bool IsUserLoggedIn(const std::string &user_id) const {
    return std::any_of(sessions_.begin(), sessions_.end(), [&](const Session &session) {
      return session.user_id == user_id;
    });
  }

  Offset SelectedBookOffset() const {
    return sessions_.empty() ? -1 : sessions_.back().selected_book;
  }

  void LogOperation(const std::string &category, const std::string &detail) {
    logs_.Append("[" + category + "] " + CurrentActor() + " | " + detail);
  }

  bool HandleSu(const std::vector<std::string> &tokens) {
    if (tokens.size() != 2 && tokens.size() != 3) {
      return false;
    }
    if (!IsIdentifier(tokens[1], 30)) {
      return false;
    }
    UserRecord record{};
    if (!users_.Read(tokens[1], record)) {
      return false;
    }
    if (tokens.size() == 2) {
      if (CurrentPrivilege() <= record.privilege) {
        return false;
      }
    } else {
      if (!IsIdentifier(tokens[2], 30) || tokens[2] != FromArray(record.password)) {
        return false;
      }
    }
    sessions_.push_back(Session{tokens[1], -1});
    LogOperation("account", "su " + tokens[1]);
    return true;
  }

  bool HandleLogout() {
    if (!RequirePrivilege(1) || sessions_.empty()) {
      return false;
    }
    LogOperation("account", "logout");
    sessions_.pop_back();
    return true;
  }

  bool HandleRegister(const std::vector<std::string> &tokens) {
    if (tokens.size() != 4) {
      return false;
    }
    if (!IsIdentifier(tokens[1], 30) || !IsIdentifier(tokens[2], 30) ||
        !IsGeneralField(tokens[3], 30)) {
      return false;
    }
    UserRecord record{};
    ToArray(tokens[1], record.user_id);
    ToArray(tokens[2], record.password);
    ToArray(tokens[3], record.user_name);
    record.privilege = 1;
    record.deleted = 0;
    if (!users_.Create(record)) {
      return false;
    }
    LogOperation("account", "register " + tokens[1]);
    return true;
  }

  bool HandlePasswd(const std::vector<std::string> &tokens) {
    if (!RequirePrivilege(1)) {
      return false;
    }
    if (tokens.size() != 3 && tokens.size() != 4) {
      return false;
    }
    if (!IsIdentifier(tokens[1], 30)) {
      return false;
    }
    UserRecord record{};
    if (!users_.Read(tokens[1], record)) {
      return false;
    }
    const bool is_root = CurrentPrivilege() == 7;
    std::string new_password;
    if (tokens.size() == 3) {
      if (!is_root || !IsIdentifier(tokens[2], 30)) {
        return false;
      }
      new_password = tokens[2];
    } else {
      if (!IsIdentifier(tokens[2], 30) || !IsIdentifier(tokens[3], 30)) {
        return false;
      }
      if (tokens[2] != FromArray(record.password)) {
        return false;
      }
      new_password = tokens[3];
    }
    ToArray(new_password, record.password);
    if (!users_.Update(tokens[1], record)) {
      return false;
    }
    LogOperation("account", "passwd " + tokens[1]);
    return true;
  }

  bool HandleUserAdd(const std::vector<std::string> &tokens) {
    if (!RequirePrivilege(3) || tokens.size() != 5) {
      return false;
    }
    if (!IsIdentifier(tokens[1], 30) || !IsIdentifier(tokens[2], 30) ||
        !IsGeneralField(tokens[4], 30)) {
      return false;
    }
    if (tokens[3].size() != 1 || (tokens[3] != "1" && tokens[3] != "3" && tokens[3] != "7")) {
      return false;
    }
    const std::int32_t privilege = tokens[3][0] - '0';
    if (privilege >= CurrentPrivilege()) {
      return false;
    }
    UserRecord record{};
    ToArray(tokens[1], record.user_id);
    ToArray(tokens[2], record.password);
    ToArray(tokens[4], record.user_name);
    record.privilege = privilege;
    record.deleted = 0;
    if (!users_.Create(record)) {
      return false;
    }
    LogOperation("account", "useradd " + tokens[1]);
    return true;
  }

  bool HandleDelete(const std::vector<std::string> &tokens) {
    if (!RequirePrivilege(7) || tokens.size() != 2 || !IsIdentifier(tokens[1], 30)) {
      return false;
    }
    if (IsUserLoggedIn(tokens[1])) {
      return false;
    }
    if (!users_.Delete(tokens[1])) {
      return false;
    }
    LogOperation("account", "delete " + tokens[1]);
    return true;
  }

  bool HandleShow(const std::vector<std::string> &tokens, std::ostream &output) {
    if (tokens.size() >= 2 && tokens[1] == "finance") {
      return HandleShowFinance(tokens, output);
    }
    if (!RequirePrivilege(1) || tokens.size() > 2) {
      return false;
    }
    std::vector<Offset> matches;
    if (tokens.size() == 1) {
      matches = books_.ListAllByIsbn();
    } else {
      const std::string &token = tokens[1];
      if (token.rfind("-ISBN=", 0) == 0) {
        const std::string isbn = token.substr(6);
        if (!IsGeneralField(isbn, 20)) {
          return false;
        }
        Offset offset = -1;
        if (books_.GetOffsetByIsbn(isbn, offset)) {
          matches.push_back(offset);
        }
      } else if (token.rfind("-name=", 0) == 0) {
        bool ok = false;
        const std::string name = StripQuotes(token.substr(6), ok);
        if (!ok || !IsQuotedField(name, 60)) {
          return false;
        }
        matches = books_.FindByName(name);
      } else if (token.rfind("-author=", 0) == 0) {
        bool ok = false;
        const std::string author = StripQuotes(token.substr(8), ok);
        if (!ok || !IsQuotedField(author, 60)) {
          return false;
        }
        matches = books_.FindByAuthor(author);
      } else if (token.rfind("-keyword=", 0) == 0) {
        bool ok = false;
        const std::string keyword = StripQuotes(token.substr(9), ok);
        if (!ok || !ValidateKeywords(keyword, false)) {
          return false;
        }
        matches = books_.FindByKeyword(keyword);
      } else {
        return false;
      }
    }
    for (Offset offset : matches) {
      BookRecord record{};
      if (!books_.ReadAt(offset, record)) {
        return false;
      }
      output << FromArray(record.isbn) << '\t' << FromArray(record.name) << '\t'
             << FromArray(record.author) << '\t' << FromArray(record.keyword) << '\t'
             << MoneyToString(record.price_cents) << '\t' << record.stock << '\n';
    }
    if (matches.empty()) {
      output << '\n';
    }
    LogOperation("book", "show");
    return true;
  }

  bool HandleShowFinance(const std::vector<std::string> &tokens, std::ostream &output) {
    if (!RequirePrivilege(7) || (tokens.size() != 2 && tokens.size() != 3)) {
      return false;
    }
    if (tokens.size() == 2) {
      const std::pair<std::int64_t, std::int64_t> totals = finances_.SumAll();
      const std::int64_t income = totals.first;
      const std::int64_t expense = totals.second;
      output << "+ " << MoneyToString(income) << " - " << MoneyToString(expense) << '\n';
      LogOperation("finance", "show finance");
      return true;
    }
    std::int32_t count = 0;
    if (!ParseUnsignedInt(tokens[2], count)) {
      return false;
    }
    if (count == 0) {
      output << '\n';
      LogOperation("finance", "show finance 0");
      return true;
    }
    if (static_cast<std::size_t>(count) > finances_.Count()) {
      return false;
    }
    const std::pair<std::int64_t, std::int64_t> totals =
        finances_.SumLast(static_cast<std::size_t>(count));
    const std::int64_t income = totals.first;
    const std::int64_t expense = totals.second;
    output << "+ " << MoneyToString(income) << " - " << MoneyToString(expense) << '\n';
    LogOperation("finance", "show finance " + tokens[2]);
    return true;
  }

  bool HandleBuy(const std::vector<std::string> &tokens, std::ostream &output) {
    if (!RequirePrivilege(1) || tokens.size() != 3 || !IsGeneralField(tokens[1], 20)) {
      return false;
    }
    std::int32_t quantity = 0;
    if (!ParseUnsignedInt(tokens[2], quantity) || quantity <= 0) {
      return false;
    }
    BookRecord record{};
    if (!books_.ReadByIsbn(tokens[1], record) || record.stock < quantity) {
      return false;
    }
    record.stock -= quantity;
    Offset offset = -1;
    if (!books_.GetOffsetByIsbn(tokens[1], offset)) {
      return false;
    }
    if (!books_.UpdateAt(offset, record)) {
      return false;
    }
    const std::int64_t total = record.price_cents * quantity;
    finances_.Append(total, 0);
    output << MoneyToString(total) << '\n';
    LogOperation("finance", "buy " + tokens[1] + " x" + tokens[2]);
    return true;
  }

  bool HandleSelect(const std::vector<std::string> &tokens) {
    if (!RequirePrivilege(3) || tokens.size() != 2 || !IsGeneralField(tokens[1], 20)) {
      return false;
    }
    if (sessions_.empty()) {
      return false;
    }
    sessions_.back().selected_book = books_.EnsureBook(tokens[1]);
    LogOperation("book", "select " + tokens[1]);
    return true;
  }

  bool HandleModify(const std::vector<std::string> &tokens) {
    if (!RequirePrivilege(3) || tokens.size() < 2 || SelectedBookOffset() < 0) {
      return false;
    }
    ModifyRequest request;
    for (std::size_t i = 1; i < tokens.size(); ++i) {
      const std::string &token = tokens[i];
      if (token.rfind("-ISBN=", 0) == 0) {
        if (request.has_isbn) {
          return false;
        }
        request.has_isbn = true;
        request.isbn = token.substr(6);
        if (!IsGeneralField(request.isbn, 20)) {
          return false;
        }
      } else if (token.rfind("-name=", 0) == 0) {
        if (request.has_name) {
          return false;
        }
        bool ok = false;
        request.name = StripQuotes(token.substr(6), ok);
        if (!ok || !IsQuotedField(request.name, 60)) {
          return false;
        }
        request.has_name = true;
      } else if (token.rfind("-author=", 0) == 0) {
        if (request.has_author) {
          return false;
        }
        bool ok = false;
        request.author = StripQuotes(token.substr(8), ok);
        if (!ok || !IsQuotedField(request.author, 60)) {
          return false;
        }
        request.has_author = true;
      } else if (token.rfind("-keyword=", 0) == 0) {
        if (request.has_keyword) {
          return false;
        }
        bool ok = false;
        request.keyword = StripQuotes(token.substr(9), ok);
        if (!ok || !ValidateKeywords(request.keyword, true)) {
          return false;
        }
        request.has_keyword = true;
      } else if (token.rfind("-price=", 0) == 0) {
        if (request.has_price) {
          return false;
        }
        if (!ParseMoney(token.substr(7), request.price_cents)) {
          return false;
        }
        request.has_price = true;
      } else {
        return false;
      }
    }
    BookRecord record{};
    if (!books_.ReadAt(SelectedBookOffset(), record)) {
      return false;
    }
    const std::string original_isbn = FromArray(record.isbn);
    if (request.has_isbn) {
      if (request.isbn == original_isbn) {
        return false;
      }
      if (books_.ExistsByIsbn(request.isbn)) {
        return false;
      }
      ToArray(request.isbn, record.isbn);
    }
    if (request.has_name) {
      ToArray(request.name, record.name);
    }
    if (request.has_author) {
      ToArray(request.author, record.author);
    }
    if (request.has_keyword) {
      ToArray(request.keyword, record.keyword);
    }
    if (request.has_price) {
      record.price_cents = request.price_cents;
    }
    if (!books_.UpdateAt(SelectedBookOffset(), record)) {
      return false;
    }
    LogOperation("book", "modify " + original_isbn);
    return true;
  }

  bool HandleImport(const std::vector<std::string> &tokens) {
    if (!RequirePrivilege(3) || tokens.size() != 3 || SelectedBookOffset() < 0) {
      return false;
    }
    std::int32_t quantity = 0;
    std::int64_t total_cost = 0;
    if (!ParseUnsignedInt(tokens[1], quantity) || quantity <= 0 ||
        !ParseMoney(tokens[2], total_cost) || total_cost <= 0) {
      return false;
    }
    BookRecord record{};
    if (!books_.ReadAt(SelectedBookOffset(), record)) {
      return false;
    }
    if (quantity > std::numeric_limits<std::int32_t>::max() - record.stock) {
      return false;
    }
    record.stock += quantity;
    if (!books_.UpdateAt(SelectedBookOffset(), record)) {
      return false;
    }
    finances_.Append(0, total_cost);
    LogOperation("finance", "import " + FromArray(record.isbn) + " x" + tokens[1]);
    return true;
  }

  bool HandleLog(const std::vector<std::string> &tokens, std::ostream &output) {
    if (!RequirePrivilege(7) || tokens.size() != 1) {
      return false;
    }
    const std::vector<std::string> lines = logs_.ReadAll();
    for (const std::string &line : lines) {
      output << line << '\n';
    }
    if (lines.empty()) {
      output << '\n';
    }
    return true;
  }

  bool HandleReport(const std::vector<std::string> &tokens, std::ostream &output) {
    if (!RequirePrivilege(7) || tokens.size() != 2) {
      return false;
    }
    if (tokens[1] == "finance") {
      const auto all = finances_.All();
      output << "Finance Report\n";
      output << "Transactions: " << all.size() << '\n';
      for (std::size_t i = 0; i < all.size(); ++i) {
        output << "#" << (i + 1) << " + " << MoneyToString(all[i].income_cents)
               << " - " << MoneyToString(all[i].expense_cents) << '\n';
      }
      return true;
    }
    if (tokens[1] == "employee") {
      output << "Employee Report\n";
      for (const std::string &line : logs_.ReadAll()) {
        if (line.find("[account]") != std::string::npos ||
            line.find("[book]") != std::string::npos ||
            line.find("[finance]") != std::string::npos) {
          output << line << '\n';
        }
      }
      return true;
    }
    return false;
  }

  std::string db_root_;
  UserStore users_;
  BookStore books_;
  FinanceStore finances_;
  LogStore logs_;
  std::vector<Session> sessions_;
};

}  // namespace

int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  BookstoreSystem system;
  std::string line;
  while (std::getline(std::cin, line)) {
    bool should_exit = false;
    if (!system.Execute(line, std::cout, should_exit)) {
      std::cout << "Invalid\n";
    }
    if (should_exit) {
      break;
    }
  }
  return 0;
}
