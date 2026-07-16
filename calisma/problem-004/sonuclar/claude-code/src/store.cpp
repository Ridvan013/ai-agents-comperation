#include "store.hpp"

#include <algorithm>
#include <iostream>

#include "utils.hpp"

using std::string;
using std::vector;

// ---------------------------------------------------------------------------
// construction / persistence
// ---------------------------------------------------------------------------

Store::Store() {
    bool freshAccounts = accountIdx.open("bs_account_idx");
    accountStore.open("bs_account_rec");
    bookIdx.open("bs_book_idx");
    bookStore.open("bs_book_rec");
    nameIdx.open("bs_name");
    authorIdx.open("bs_author");
    keywordIdx.open("bs_keyword");
    finance.open("bs_finance");
    opLog.open("bs_oplog.txt", std::ios::app);

    // First run: create the super-administrator account.
    if (freshAccounts) {
        Account root;
        setStr(root.userID, "root", sizeof(root.userID));
        setStr(root.password, "sjtu", sizeof(root.password));
        setStr(root.username, "root", sizeof(root.username));
        root.privilege = 7;
        int id = accountStore.append(root);
        AccountRef ref;
        setStr(ref.userID, "root", sizeof(ref.userID));
        ref.recId = id;
        accountIdx.insert(ref);
    }
}

Store::~Store() {
    if (opLog.is_open()) opLog.flush();
}

// ---------------------------------------------------------------------------
// small helpers
// ---------------------------------------------------------------------------

void Store::invalid() { std::cout << "Invalid\n"; }

bool Store::isLoggedIn(const string &uid) const {
    for (const auto &f : loginStack)
        if (f.user == uid) return true;
    return false;
}

void Store::logOp(const string &command) {
    if (!opLog.is_open()) return;
    string user = loginStack.empty() ? "guest" : loginStack.back().user;
    opLog << user << '\t' << curPriv() << '\t' << command << '\n';
}

vector<string> Store::splitKeywords(const char *raw) {
    vector<string> segs;
    string cur;
    for (const char *p = raw; *p; ++p) {
        if (*p == '|') {
            segs.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(*p);
        }
    }
    segs.push_back(cur);
    return segs;
}

bool Store::findAccount(const string &uid, Account &acc, int &recId) {
    AccountRef probe, ref;
    setStr(probe.userID, uid.c_str(), sizeof(probe.userID));
    if (!accountIdx.find(probe, ref)) return false;
    recId = ref.recId;
    accountStore.read(recId, acc);
    return true;
}

bool Store::findBook(const string &isbn, Book &book, int &recId) {
    BookRef probe, ref;
    setStr(probe.isbn, isbn.c_str(), sizeof(probe.isbn));
    if (!bookIdx.find(probe, ref)) return false;
    recId = ref.recId;
    bookStore.read(recId, book);
    return true;
}

// ---------------------------------------------------------------------------
// dispatch
// ---------------------------------------------------------------------------

bool Store::executeLine(const string &raw) {
    if (util::lineHasIllegalChar(raw)) {
        invalid();
        return true;
    }
    vector<string> t = util::tokenize(raw);
    if (t.empty()) return true;  // blank line: legal, no output

    const string &cmd = t[0];

    if (cmd == "quit" || cmd == "exit") {
        if (t.size() != 1) { invalid(); return true; }
        return false;  // terminate
    }

    bool ok;
    if (cmd == "su")            ok = cmdSu(t);
    else if (cmd == "logout")   ok = cmdLogout(t);
    else if (cmd == "register") ok = cmdRegister(t);
    else if (cmd == "passwd")   ok = cmdPasswd(t);
    else if (cmd == "useradd")  ok = cmdUseradd(t);
    else if (cmd == "delete")   ok = cmdDelete(t);
    else if (cmd == "show")     ok = (t.size() >= 2 && t[1] == "finance")
                                         ? cmdShowFinance(t) : cmdShow(t);
    else if (cmd == "buy")      ok = cmdBuy(t);
    else if (cmd == "select")   ok = cmdSelect(t);
    else if (cmd == "modify")   ok = cmdModify(t);
    else if (cmd == "import")   ok = cmdImport(t);
    else if (cmd == "report")   ok = cmdReport(t);
    else if (cmd == "log")      ok = cmdLog(t);
    else                        ok = false;

    if (!ok) invalid();
    return true;
}

// ---------------------------------------------------------------------------
// account system
// ---------------------------------------------------------------------------

bool Store::cmdSu(const vector<string> &t) {
    if (t.size() != 2 && t.size() != 3) return false;
    const string &uid = t[1];
    if (!util::validId(uid)) return false;
    if (t.size() == 3 && !util::validId(t[2])) return false;

    Account acc;
    int recId;
    if (!findAccount(uid, acc, recId)) return false;

    if (t.size() == 3) {
        if (acc.password != string(t[2])) return false;
    } else {
        if (curPriv() <= acc.privilege) return false;  // need password
    }

    logOp("su " + uid);
    loginStack.push_back({uid, acc.privilege, ""});
    return true;
}

bool Store::cmdLogout(const vector<string> &t) {
    if (t.size() != 1) return false;
    if (loginStack.empty()) return false;
    logOp("logout");
    loginStack.pop_back();
    return true;
}

bool Store::cmdRegister(const vector<string> &t) {
    if (t.size() != 4) return false;
    if (!util::validId(t[1]) || !util::validId(t[2]) || !util::validUsername(t[3]))
        return false;

    Account tmp;
    int recId;
    if (findAccount(t[1], tmp, recId)) return false;  // already registered

    Account acc;
    setStr(acc.userID, t[1].c_str(), sizeof(acc.userID));
    setStr(acc.password, t[2].c_str(), sizeof(acc.password));
    setStr(acc.username, t[3].c_str(), sizeof(acc.username));
    acc.privilege = 1;
    int id = accountStore.append(acc);
    AccountRef ref;
    setStr(ref.userID, t[1].c_str(), sizeof(ref.userID));
    ref.recId = id;
    accountIdx.insert(ref);
    logOp("register " + t[1]);
    return true;
}

bool Store::cmdPasswd(const vector<string> &t) {
    if (curPriv() < 1) return false;
    if (t.size() != 3 && t.size() != 4) return false;
    if (!util::validId(t[1])) return false;

    Account acc;
    int recId;
    if (!findAccount(t[1], acc, recId)) return false;

    string newPass;
    if (t.size() == 4) {
        if (!util::validId(t[2]) || !util::validId(t[3])) return false;
        if (acc.password != string(t[2])) return false;
        newPass = t[3];
    } else {
        if (!util::validId(t[2])) return false;
        if (curPriv() != 7) return false;  // current password required otherwise
        newPass = t[2];
    }

    setStr(acc.password, newPass.c_str(), sizeof(acc.password));
    accountStore.write(recId, acc);
    logOp("passwd " + t[1]);
    return true;
}

bool Store::cmdUseradd(const vector<string> &t) {
    if (curPriv() < 3) return false;
    if (t.size() != 5) return false;
    int priv;
    if (!util::validId(t[1]) || !util::validId(t[2]) ||
        !util::validPrivilege(t[3], priv) || !util::validUsername(t[4]))
        return false;
    if (priv >= curPriv()) return false;

    Account tmp;
    int recId;
    if (findAccount(t[1], tmp, recId)) return false;

    Account acc;
    setStr(acc.userID, t[1].c_str(), sizeof(acc.userID));
    setStr(acc.password, t[2].c_str(), sizeof(acc.password));
    setStr(acc.username, t[4].c_str(), sizeof(acc.username));
    acc.privilege = priv;
    int id = accountStore.append(acc);
    AccountRef ref;
    setStr(ref.userID, t[1].c_str(), sizeof(ref.userID));
    ref.recId = id;
    accountIdx.insert(ref);
    logOp("useradd " + t[1]);
    return true;
}

bool Store::cmdDelete(const vector<string> &t) {
    if (curPriv() < 7) return false;
    if (t.size() != 2) return false;
    if (!util::validId(t[1])) return false;

    Account acc;
    int recId;
    if (!findAccount(t[1], acc, recId)) return false;
    if (isLoggedIn(t[1])) return false;  // cannot delete a logged-in account

    AccountRef ref;
    setStr(ref.userID, t[1].c_str(), sizeof(ref.userID));
    accountIdx.erase(ref);
    accountStore.remove(recId);
    logOp("delete " + t[1]);
    return true;
}

// ---------------------------------------------------------------------------
// book system
// ---------------------------------------------------------------------------

void Store::outputBooks(const vector<Book> &list) {
    if (list.empty()) {
        std::cout << '\n';
        return;
    }
    for (const Book &b : list) {
        std::cout << b.isbn << '\t' << b.name << '\t' << b.author << '\t'
                  << b.keyword << '\t' << util::formatMoney(b.priceCents) << '\t'
                  << b.stock << '\n';
    }
}

bool Store::cmdShow(const vector<string> &t) {
    if (curPriv() < 1) return false;
    if (t.size() > 2) return false;

    vector<Book> result;
    if (t.size() == 1) {
        BookRef low;
        setStr(low.isbn, "", sizeof(low.isbn));
        low.recId = 0;
        bookIdx.forEachFrom(low, [&](const BookRef &r) {
            Book b;
            bookStore.read(r.recId, b);
            result.push_back(b);
            return true;
        });
        outputBooks(result);
        return true;
    }

    const string &f = t[1];
    // -ISBN=...
    if (f.compare(0, 6, "-ISBN=") == 0) {
        string val = f.substr(6);
        if (!util::validIsbn(val)) return false;
        Book b;
        int recId;
        if (findBook(val, b, recId)) result.push_back(b);
        outputBooks(result);
        return true;
    }

    // quoted attribute filters
    struct Q { const char *prefix; size_t plen; SecIndex *idx; bool kw; };
    Q qs[3] = {
        {"-name=\"", 7, &nameIdx, false},
        {"-author=\"", 9, &authorIdx, false},
        {"-keyword=\"", 10, &keywordIdx, true},
    };
    for (const Q &q : qs) {
        if (f.size() >= q.plen && f.compare(0, q.plen, q.prefix) == 0) {
            if (f.back() != '"' || f.size() < q.plen + 1) return false;
            string inner = f.substr(q.plen, f.size() - q.plen - 1);
            if (!util::validText(inner)) return false;
            if (q.kw && inner.find('|') != string::npos) return false;  // single keyword only

            IndexEntry low;
            setStr(low.key, inner.c_str(), sizeof(low.key));
            setStr(low.isbn, "", sizeof(low.isbn));
            q.idx->forEachFrom(low, [&](const IndexEntry &e) {
                if (string(e.key) != inner) return false;
                Book b;
                int recId;
                if (findBook(e.isbn, b, recId)) result.push_back(b);
                return true;
            });
            outputBooks(result);
            return true;
        }
    }
    return false;
}

bool Store::cmdBuy(const vector<string> &t) {
    if (curPriv() < 1) return false;
    if (t.size() != 3) return false;
    if (!util::validIsbn(t[1])) return false;
    long long qty;
    if (!util::parseCount(t[2], qty) || qty <= 0) return false;

    Book book;
    int recId;
    if (!findBook(t[1], book, recId)) return false;
    if (book.stock < qty) return false;

    book.stock -= qty;
    bookStore.write(recId, book);  // in-place: the ordered index is untouched
    Int128 total = (Int128)book.priceCents * (Int128)qty;
    finance.record(total, 0);
    logOp("buy " + t[1] + " " + t[2]);
    std::cout << util::formatMoney(total) << '\n';
    return true;
}

bool Store::cmdSelect(const vector<string> &t) {
    if (curPriv() < 3) return false;
    if (t.size() != 2) return false;
    if (!util::validIsbn(t[1])) return false;

    Book book;
    int recId;
    if (!findBook(t[1], book, recId)) {
        Book nb;
        setStr(nb.isbn, t[1].c_str(), sizeof(nb.isbn));
        setStr(nb.name, "", sizeof(nb.name));
        setStr(nb.author, "", sizeof(nb.author));
        setStr(nb.keyword, "", sizeof(nb.keyword));
        nb.priceCents = 0;
        nb.stock      = 0;
        int id = bookStore.append(nb);
        BookRef ref;
        setStr(ref.isbn, t[1].c_str(), sizeof(ref.isbn));
        ref.recId = id;
        bookIdx.insert(ref);
    }
    loginStack.back().selected = t[1];
    logOp("select " + t[1]);
    return true;
}

bool Store::cmdModify(const vector<string> &t) {
    if (curPriv() < 3) return false;
    if (loginStack.empty() || loginStack.back().selected.empty()) return false;
    if (t.size() < 2) return false;

    bool hasIsbn = false, hasName = false, hasAuthor = false, hasKw = false, hasPrice = false;
    string newIsbn, newName, newAuthor, newKeyword;
    long long newPrice = 0;

    for (size_t i = 1; i < t.size(); ++i) {
        const string &f = t[i];
        if (f.compare(0, 6, "-ISBN=") == 0) {
            if (hasIsbn) return false;
            hasIsbn = true;
            newIsbn = f.substr(6);
            if (!util::validIsbn(newIsbn)) return false;
        } else if (f.compare(0, 7, "-price=") == 0) {
            if (hasPrice) return false;
            hasPrice = true;
            string val = f.substr(7);
            if (!util::parseMoney(val, newPrice)) return false;
        } else if (f.size() >= 7 && f.compare(0, 7, "-name=\"") == 0) {
            if (hasName) return false;
            hasName = true;
            if (f.back() != '"' || f.size() < 8) return false;
            newName = f.substr(7, f.size() - 8);
            if (!util::validText(newName)) return false;
        } else if (f.size() >= 9 && f.compare(0, 9, "-author=\"") == 0) {
            if (hasAuthor) return false;
            hasAuthor = true;
            if (f.back() != '"' || f.size() < 10) return false;
            newAuthor = f.substr(9, f.size() - 10);
            if (!util::validText(newAuthor)) return false;
        } else if (f.size() >= 10 && f.compare(0, 10, "-keyword=\"") == 0) {
            if (hasKw) return false;
            hasKw = true;
            if (f.back() != '"' || f.size() < 11) return false;
            newKeyword = f.substr(10, f.size() - 11);
            if (!util::validText(newKeyword)) return false;
        } else {
            return false;
        }
    }

    // validate keyword segments (non-empty, no duplicates)
    if (hasKw) {
        vector<string> segs = splitKeywords(newKeyword.c_str());
        for (const string &s : segs)
            if (s.empty()) return false;
        for (size_t a = 0; a < segs.size(); ++a)
            for (size_t b = a + 1; b < segs.size(); ++b)
                if (segs[a] == segs[b]) return false;
    }

    Book book;
    int recId;
    if (!findBook(loginStack.back().selected, book, recId)) return false;

    if (hasIsbn) {
        if (newIsbn == string(book.isbn)) return false;  // cannot keep original ISBN
        Book other;
        int otherRec;
        if (findBook(newIsbn, other, otherRec)) return false;  // ISBN already used
    }

    // remove existing secondary-index entries (keyed by old ISBN)
    if (book.name[0]) {
        IndexEntry e;
        setStr(e.key, book.name, sizeof(e.key));
        setStr(e.isbn, book.isbn, sizeof(e.isbn));
        nameIdx.erase(e);
    }
    if (book.author[0]) {
        IndexEntry e;
        setStr(e.key, book.author, sizeof(e.key));
        setStr(e.isbn, book.isbn, sizeof(e.isbn));
        authorIdx.erase(e);
    }
    if (book.keyword[0]) {
        for (const string &s : splitKeywords(book.keyword)) {
            IndexEntry e;
            setStr(e.key, s.c_str(), sizeof(e.key));
            setStr(e.isbn, book.isbn, sizeof(e.isbn));
            keywordIdx.erase(e);
        }
    }

    string oldIsbn = book.isbn;
    if (hasName)   setStr(book.name, newName.c_str(), sizeof(book.name));
    if (hasAuthor) setStr(book.author, newAuthor.c_str(), sizeof(book.author));
    if (hasKw)     setStr(book.keyword, newKeyword.c_str(), sizeof(book.keyword));
    if (hasPrice)  book.priceCents = newPrice;

    if (hasIsbn) {
        BookRef oldRef;
        setStr(oldRef.isbn, oldIsbn.c_str(), sizeof(oldRef.isbn));
        bookIdx.erase(oldRef);
        setStr(book.isbn, newIsbn.c_str(), sizeof(book.isbn));
        BookRef newRef;
        setStr(newRef.isbn, newIsbn.c_str(), sizeof(newRef.isbn));
        newRef.recId = recId;  // record stays in place, only the key changes
        bookIdx.insert(newRef);
        loginStack.back().selected = newIsbn;
    }
    bookStore.write(recId, book);

    // (re)insert secondary-index entries keyed by the new ISBN
    if (book.name[0]) {
        IndexEntry e;
        setStr(e.key, book.name, sizeof(e.key));
        setStr(e.isbn, book.isbn, sizeof(e.isbn));
        nameIdx.insert(e);
    }
    if (book.author[0]) {
        IndexEntry e;
        setStr(e.key, book.author, sizeof(e.key));
        setStr(e.isbn, book.isbn, sizeof(e.isbn));
        authorIdx.insert(e);
    }
    if (book.keyword[0]) {
        for (const string &s : splitKeywords(book.keyword)) {
            IndexEntry e;
            setStr(e.key, s.c_str(), sizeof(e.key));
            setStr(e.isbn, book.isbn, sizeof(e.isbn));
            keywordIdx.insert(e);
        }
    }

    logOp("modify " + string(book.isbn));
    return true;
}

bool Store::cmdImport(const vector<string> &t) {
    if (curPriv() < 3) return false;
    if (loginStack.empty() || loginStack.back().selected.empty()) return false;
    if (t.size() != 3) return false;
    long long qty, cost;
    if (!util::parseCount(t[1], qty) || qty <= 0) return false;
    if (!util::parseMoney(t[2], cost) || cost <= 0) return false;

    Book book;
    int recId;
    if (!findBook(loginStack.back().selected, book, recId)) return false;

    book.stock += qty;
    bookStore.write(recId, book);
    finance.record(0, (Int128)cost);
    logOp("import " + t[1] + " " + t[2]);
    return true;
}

// ---------------------------------------------------------------------------
// log system
// ---------------------------------------------------------------------------

bool Store::cmdShowFinance(const vector<string> &t) {
    if (curPriv() < 7) return false;
    // t[0] == "show", t[1] == "finance"
    if (t.size() > 3) return false;

    long long k = -1;  // all
    if (t.size() == 3) {
        if (!util::parseCount(t[2], k)) return false;
        if (k > finance.total()) return false;
        if (k == 0) {
            std::cout << '\n';
            return true;
        }
    }

    Int128 income, expend;
    finance.sumLast(k, income, expend);
    std::cout << "+ " << util::formatMoney(income) << " - "
              << util::formatMoney(expend) << '\n';
    return true;
}

bool Store::cmdReport(const vector<string> &t) {
    if (curPriv() < 7) return false;
    if (t.size() != 2) return false;

    if (t[1] == "finance") {
        Int128 income, expend;
        finance.sumLast(-1, income, expend);
        std::cout << "===== Finance Report =====\n";
        std::cout << "Transactions : " << finance.total() << '\n';
        std::cout << "Total income : " << util::formatMoney(income) << '\n';
        std::cout << "Total expend : " << util::formatMoney(expend) << '\n';
        std::cout << "Net          : "
                  << util::formatMoney(income >= expend ? income - expend
                                                        : expend - income)
                  << (income >= expend ? " (profit)\n" : " (loss)\n");
        std::cout << "==========================\n";
        return true;
    }

    if (t[1] == "employee") {
        opLog.flush();
        std::ifstream in("bs_oplog.txt");
        std::cout << "===== Employee Work Report =====\n";
        string line;
        while (std::getline(in, line)) {
            // fields: user \t priv \t command
            size_t p1 = line.find('\t');
            if (p1 == string::npos) continue;
            size_t p2 = line.find('\t', p1 + 1);
            if (p2 == string::npos) continue;
            int priv = std::atoi(line.substr(p1 + 1, p2 - p1 - 1).c_str());
            if (priv >= 3) std::cout << line.substr(0, p1) << " : "
                                     << line.substr(p2 + 1) << '\n';
        }
        std::cout << "================================\n";
        return true;
    }

    return false;
}

bool Store::cmdLog(const vector<string> &t) {
    if (curPriv() < 7) return false;
    if (t.size() != 1) return false;

    opLog.flush();
    std::ifstream in("bs_oplog.txt");
    std::cout << "===== System Log =====\n";
    string line;
    while (std::getline(in, line)) {
        size_t p1 = line.find('\t');
        size_t p2 = (p1 == string::npos) ? string::npos : line.find('\t', p1 + 1);
        if (p2 == string::npos) { std::cout << line << '\n'; continue; }
        std::cout << line.substr(0, p1) << " (priv "
                  << line.substr(p1 + 1, p2 - p1 - 1) << "): "
                  << line.substr(p2 + 1) << '\n';
    }
    Int128 income, expend;
    finance.sumLast(-1, income, expend);
    std::cout << "----- Finance -----\n";
    std::cout << "Transactions: " << finance.total()
              << "  income: " << util::formatMoney(income)
              << "  expend: " << util::formatMoney(expend) << '\n';
    std::cout << "======================\n";
    return true;
}
