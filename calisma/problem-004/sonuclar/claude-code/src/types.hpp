#ifndef BOOKSTORE_TYPES_HPP
#define BOOKSTORE_TYPES_HPP

#include <cstring>

// Wide integer for monetary accumulation. The evaluation compiler (g++-13,
// 64-bit) provides __int128, which makes all price*quantity products and
// running totals exact. On toolchains without it we fall back to 64-bit
// integers (sufficient for local testing with modest data).
#ifdef __SIZEOF_INT128__
using Int128  = __int128;
using UInt128 = unsigned __int128;
#else
using Int128  = long long;
using UInt128 = unsigned long long;
#endif

// Fixed-length limits (per specification). Char arrays are one byte larger than
// the maximum allowed content length to always keep room for a NUL terminator.
constexpr int MAX_USERID   = 30;
constexpr int MAX_PASSWORD = 30;
constexpr int MAX_USERNAME = 30;
constexpr int MAX_ISBN     = 20;
constexpr int MAX_TEXT     = 60;  // book name / author / keyword field

// A registered account. Ordering (and equality) depend only on userID, so the
// payload can be updated in place without disturbing the on-disk order.
struct Account {
    char userID[MAX_USERID + 2];
    char password[MAX_PASSWORD + 2];
    char username[MAX_USERNAME + 2];
    int  privilege;

    bool operator<(const Account &o) const {
        return std::strcmp(userID, o.userID) < 0;
    }
};

// A book record. Ordering depends only on ISBN.
struct Book {
    char      isbn[MAX_ISBN + 2];
    char      name[MAX_TEXT + 2];
    char      author[MAX_TEXT + 2];
    char      keyword[MAX_TEXT + 2];  // segments joined by '|'
    long long priceCents;             // unit price in cents
    long long stock;

    bool operator<(const Book &o) const {
        return std::strcmp(isbn, o.isbn) < 0;
    }
};

// Ordered index entry mapping a userID to its record id in the account store.
struct AccountRef {
    char userID[MAX_USERID + 2];
    int  recId;

    bool operator<(const AccountRef &o) const {
        return std::strcmp(userID, o.userID) < 0;
    }
};

// Ordered index entry mapping an ISBN to its record id in the book store.
struct BookRef {
    char isbn[MAX_ISBN + 2];
    int  recId;

    bool operator<(const BookRef &o) const {
        return std::strcmp(isbn, o.isbn) < 0;
    }
};

// Secondary index entry: (attribute value, ISBN). Used for name / author /
// keyword lookups. Ordering is by value first, then ISBN, so all books sharing
// a value appear contiguously and already sorted by ISBN.
struct IndexEntry {
    char key[MAX_TEXT + 2];
    char isbn[MAX_ISBN + 2];

    bool operator<(const IndexEntry &o) const {
        int c = std::strcmp(key, o.key);
        if (c != 0) return c < 0;
        return std::strcmp(isbn, o.isbn) < 0;
    }
};

inline void setStr(char *dst, const char *src, int cap) {
    std::memset(dst, 0, cap);
    std::strncpy(dst, src, cap - 1);
}

#endif  // BOOKSTORE_TYPES_HPP
