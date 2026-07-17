#include "book_system.hpp"

#include <iostream>

namespace bookstore {

namespace {

/// If `arg` starts with `prefix`, stores the remainder in `value`.
bool matchOption(const std::string &arg, const char *prefix, std::string &value) {
  const size_t length = std::strlen(prefix);
  if (arg.size() < length || std::memcmp(arg.data(), prefix, length) != 0) return false;
  value = arg.substr(length);
  return true;
}

/// Removes the double quotes required around name/author/keyword arguments.
std::string unquote(const std::string &raw) {
  failIf(raw.size() < 2 || raw.front() != '"' || raw.back() != '"');
  return raw.substr(1, raw.size() - 2);
}

}  // namespace

void BookSystem::open() {
  books_.open("books.dat", EmptyMeta{0});
  isbnIndex_.open("isbn.idx");
  nameIndex_.open("name.idx");
  authorIndex_.open("author.idx");
  keywordIndex_.open("keyword.idx");
}

void BookSystem::close() {
  books_.close();
  isbnIndex_.close();
  nameIndex_.close();
  authorIndex_.close();
  keywordIndex_.close();
}

void BookSystem::indexBook(const BookRecord &book, int id) {
  isbnIndex_.insert(book.isbn, id);
  const std::string isbn = book.isbn.str();
  if (!book.name.empty()) nameIndex_.insert(TextIsbnKey(book.name.str(), isbn), id);
  if (!book.author.empty()) authorIndex_.insert(TextIsbnKey(book.author.str(), isbn), id);
  if (!book.keyword.empty()) {
    for (const std::string &part : splitKeywords(book.keyword.str(), false))
      keywordIndex_.insert(TextIsbnKey(part, isbn), id);
  }
}

void BookSystem::unindexBook(const BookRecord &book) {
  isbnIndex_.erase(book.isbn);
  const std::string isbn = book.isbn.str();
  if (!book.name.empty()) nameIndex_.erase(TextIsbnKey(book.name.str(), isbn));
  if (!book.author.empty()) authorIndex_.erase(TextIsbnKey(book.author.str(), isbn));
  if (!book.keyword.empty()) {
    for (const std::string &part : splitKeywords(book.keyword.str(), false))
      keywordIndex_.erase(TextIsbnKey(part, isbn));
  }
}

void BookSystem::printBooks(const std::vector<int> &ids) {
  if (ids.empty()) {
    std::cout << '\n';
    return;
  }
  BookRecord book;
  for (int id : ids) {
    books_.read(id, book);
    std::cout << book.isbn.str() << '\t' << book.name.str() << '\t' << book.author.str() << '\t'
              << book.keyword.str() << '\t' << formatMoney(book.price) << '\t' << book.stock
              << '\n';
  }
}

void BookSystem::show(const std::vector<std::string> &tokens) {
  failIf(tokens.size() > 2);

  std::vector<int> ids;
  if (tokens.size() == 1) {
    // Scanning the ISBN index yields every book already in the required order.
    isbnIndex_.range(IsbnStr(), IsbnStr::maxSentinel(), ids);
  } else {
    const std::string &arg = tokens[1];
    std::string value;
    if (matchOption(arg, "-ISBN=", value)) {
      failIf(!isValidIsbn(value));
      int id;
      if (isbnIndex_.find(IsbnStr(value), id)) ids.push_back(id);
    } else if (matchOption(arg, "-name=", value)) {
      value = unquote(value);
      failIf(!isValidBookText(value));
      nameIndex_.range(textLowerBound(value), textUpperBound(value), ids);
    } else if (matchOption(arg, "-author=", value)) {
      value = unquote(value);
      failIf(!isValidBookText(value));
      authorIndex_.range(textLowerBound(value), textUpperBound(value), ids);
    } else if (matchOption(arg, "-keyword=", value)) {
      value = unquote(value);
      failIf(!isValidBookText(value));
      failIf(value.find('|') != std::string::npos);  // `show` accepts a single keyword only.
      keywordIndex_.range(textLowerBound(value), textUpperBound(value), ids);
    } else {
      fail();
    }
  }
  printBooks(ids);
}

void BookSystem::buy(const std::vector<std::string> &tokens) {
  failIf(tokens.size() != 3);
  failIf(!isValidIsbn(tokens[1]));

  long long quantity;
  failIf(!parseCount(tokens[2], quantity));
  failIf(quantity <= 0);

  int id;
  failIf(!isbnIndex_.find(IsbnStr(tokens[1]), id));

  BookRecord book;
  books_.read(id, book);
  failIf(book.stock < quantity);

  const long long total = book.price * quantity;
  book.stock -= quantity;
  books_.write(id, book);

  std::cout << formatMoney(total) << '\n';
  logs_.recordIncome(accounts_.currentUserId(), accounts_.currentPrivilege(), total,
                     "buy " + tokens[1] + " " + tokens[2]);
}

void BookSystem::select(const std::vector<std::string> &tokens) {
  failIf(tokens.size() != 2);
  failIf(!isValidIsbn(tokens[1]));

  int id;
  if (!isbnIndex_.find(IsbnStr(tokens[1]), id)) {
    BookRecord book{};
    book.isbn = IsbnStr(tokens[1]);
    id = books_.allocate();
    books_.write(id, book);
    indexBook(book, id);
  }
  accounts_.setSelectedBook(id);
}

void BookSystem::modify(const std::vector<std::string> &tokens) {
  failIf(tokens.size() < 2);
  const int id = accounts_.selectedBook();
  failIf(id == -1);

  // Parse and validate everything before touching the record, so a command that
  // fails half way through leaves no partial change behind.
  BookUpdate update;
  for (size_t i = 1; i < tokens.size(); ++i) {
    const std::string &arg = tokens[i];
    std::string value;
    if (matchOption(arg, "-ISBN=", value)) {
      failIf(update.hasIsbn);
      failIf(!isValidIsbn(value));
      update.hasIsbn = true;
      update.isbn = value;
    } else if (matchOption(arg, "-name=", value)) {
      failIf(update.hasName);
      value = unquote(value);
      failIf(!isValidBookText(value));
      update.hasName = true;
      update.name = value;
    } else if (matchOption(arg, "-author=", value)) {
      failIf(update.hasAuthor);
      value = unquote(value);
      failIf(!isValidBookText(value));
      update.hasAuthor = true;
      update.author = value;
    } else if (matchOption(arg, "-keyword=", value)) {
      failIf(update.hasKeyword);
      value = unquote(value);
      splitKeywords(value, /*requireUnique=*/true);  // Validates charset and segments too.
      update.hasKeyword = true;
      update.keyword = value;
    } else if (matchOption(arg, "-price=", value)) {
      failIf(update.hasPrice);
      failIf(!parseMoney(value, update.price));
      update.hasPrice = true;
    } else {
      fail();
    }
  }

  BookRecord before;
  books_.read(id, before);

  if (update.hasIsbn) {
    failIf(update.isbn == before.isbn.str());
    failIf(isbnIndex_.contains(IsbnStr(update.isbn)));
  }

  BookRecord after = before;
  if (update.hasIsbn) after.isbn = IsbnStr(update.isbn);
  if (update.hasName) after.name = TextStr(update.name);
  if (update.hasAuthor) after.author = TextStr(update.author);
  if (update.hasKeyword) after.keyword = TextStr(update.keyword);
  if (update.hasPrice) after.price = update.price;

  // Search keys embed the ISBN, so a changed ISBN invalidates every index entry
  // of this book. Rebuilding all of them keeps that case from needing a
  // special path.
  unindexBook(before);
  indexBook(after, id);
  books_.write(id, after);
}

void BookSystem::importBooks(const std::vector<std::string> &tokens) {
  failIf(tokens.size() != 3);
  const int id = accounts_.selectedBook();
  failIf(id == -1);

  long long quantity;
  failIf(!parseCount(tokens[1], quantity));
  failIf(quantity <= 0);

  long long cost;
  failIf(!parseMoney(tokens[2], cost));
  failIf(cost <= 0);

  BookRecord book;
  books_.read(id, book);
  book.stock += quantity;
  books_.write(id, book);

  logs_.recordExpense(accounts_.currentUserId(), accounts_.currentPrivilege(), cost,
                      "import " + tokens[1] + " " + tokens[2]);
}

}  // namespace bookstore
