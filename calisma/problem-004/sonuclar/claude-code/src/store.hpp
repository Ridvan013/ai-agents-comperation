#ifndef BOOKSTORE_STORE_HPP
#define BOOKSTORE_STORE_HPP

#include <fstream>
#include <string>
#include <vector>

#include "blocklist.hpp"
#include "finance.hpp"
#include "recordstore.hpp"
#include "types.hpp"

// One frame of the login stack.
struct LoginFrame {
    std::string user;
    int         priv;
    std::string selected;  // selected book's ISBN ("" if none)
};

class Store {
  public:
    Store();
    ~Store();

    // Process one raw input line. Returns false when the system must terminate
    // (quit / exit).
    bool executeLine(const std::string &raw);

  private:
    // Block-list orders are tuned so a block spans a few KB: the small primary
    // key->id refs pack more per block; the fat 88-byte secondary-index entries
    // use a smaller order to keep per-operation copying cheap.
    using AcctIndex = BlockList<AccountRef, 96>;
    using BookIndex = BlockList<BookRef, 96>;
    using SecIndex  = BlockList<IndexEntry, 40>;

    // Primary stores: a small ordered key->id index plus a flat record file.
    AcctIndex            accountIdx;
    RecordStore<Account> accountStore;
    BookIndex            bookIdx;
    RecordStore<Book>    bookStore;
    // Secondary indexes for name / author / keyword lookups.
    SecIndex nameIdx;
    SecIndex authorIdx;
    SecIndex keywordIdx;
    Finance               finance;
    std::ofstream         opLog;

    std::vector<LoginFrame> loginStack;

    // helpers -----------------------------------------------------------------
    int  curPriv() const { return loginStack.empty() ? 0 : loginStack.back().priv; }
    bool isLoggedIn(const std::string &uid) const;
    void logOp(const std::string &command);

    bool findAccount(const std::string &uid, Account &acc, int &recId);
    bool findBook(const std::string &isbn, Book &book, int &recId);

    static void invalid();
    static std::vector<std::string> splitKeywords(const char *raw);

    // command handlers (return true on success) -------------------------------
    bool cmdSu(const std::vector<std::string> &t);
    bool cmdLogout(const std::vector<std::string> &t);
    bool cmdRegister(const std::vector<std::string> &t);
    bool cmdPasswd(const std::vector<std::string> &t);
    bool cmdUseradd(const std::vector<std::string> &t);
    bool cmdDelete(const std::vector<std::string> &t);

    bool cmdShow(const std::vector<std::string> &t);
    bool cmdBuy(const std::vector<std::string> &t);
    bool cmdSelect(const std::vector<std::string> &t);
    bool cmdModify(const std::vector<std::string> &t);
    bool cmdImport(const std::vector<std::string> &t);

    bool cmdShowFinance(const std::vector<std::string> &t);
    bool cmdReport(const std::vector<std::string> &t);
    bool cmdLog(const std::vector<std::string> &t);

    void outputBooks(const std::vector<Book> &list);
};

#endif  // BOOKSTORE_STORE_HPP
