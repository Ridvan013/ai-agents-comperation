#ifndef BOOKSTORE_BOOK_SYSTEM_HPP
#define BOOKSTORE_BOOK_SYSTEM_HPP

#include <string>
#include <vector>

#include "account_system.hpp"
#include "block_file.hpp"
#include "bplus_tree.hpp"
#include "log_system.hpp"
#include "records.hpp"

namespace bookstore {

/// Books, their search indices and the trading commands.
class BookSystem {
 public:
  BookSystem(AccountSystem &accounts, LogSystem &logs) : accounts_(accounts), logs_(logs) {}

  void open();
  void close();

  void show(const std::vector<std::string> &tokens);
  void buy(const std::vector<std::string> &tokens);
  void select(const std::vector<std::string> &tokens);
  void modify(const std::vector<std::string> &tokens);
  void importBooks(const std::vector<std::string> &tokens);

 private:
  /// The fields a `modify` command asks to change.
  struct BookUpdate {
    bool hasIsbn = false, hasName = false, hasAuthor = false, hasKeyword = false, hasPrice = false;
    std::string isbn, name, author, keyword;
    long long price = 0;
  };

  void printBooks(const std::vector<int> &ids);
  void indexBook(const BookRecord &book, int id);
  void unindexBook(const BookRecord &book);

  BlockFile<EmptyMeta, BookRecord> books_;
  BPlusTree<IsbnStr, int> isbnIndex_;
  BPlusTree<TextIsbnKey, int> nameIndex_;
  BPlusTree<TextIsbnKey, int> authorIndex_;
  BPlusTree<TextIsbnKey, int> keywordIndex_;

  AccountSystem &accounts_;
  LogSystem &logs_;
};

}  // namespace bookstore

#endif  // BOOKSTORE_BOOK_SYSTEM_HPP
