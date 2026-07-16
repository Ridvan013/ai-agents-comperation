#ifndef BOOKSTORE_UTILS_HPP
#define BOOKSTORE_UTILS_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "types.hpp"

namespace util {

// --- character classes -------------------------------------------------------

inline bool isVisible(char c) {
    // printable ASCII excluding space and control characters
    unsigned char u = (unsigned char)c;
    return u >= 0x21 && u <= 0x7E;
}

inline bool isIdChar(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') || c == '_';
}

inline bool isDigit(char c) { return c >= '0' && c <= '9'; }

// --- line handling -----------------------------------------------------------

// A legal command line contains only printable ASCII and spaces. Anything else
// (tabs, other control characters, bytes >= 0x80) makes the command illegal.
inline bool lineHasIllegalChar(const std::string &line) {
    for (char c : line) {
        unsigned char u = (unsigned char)c;
        if (u != ' ' && (u < 0x21 || u > 0x7E)) return true;
    }
    return false;
}

// Split on runs of spaces; leading/trailing spaces ignored.
inline std::vector<std::string> tokenize(const std::string &line) {
    std::vector<std::string> out;
    size_t i = 0, n = line.size();
    while (i < n) {
        while (i < n && line[i] == ' ') ++i;
        if (i >= n) break;
        size_t j = i;
        while (j < n && line[j] != ' ') ++j;
        out.push_back(line.substr(i, j - i));
        i = j;
    }
    return out;
}

// --- field validation --------------------------------------------------------

inline bool validId(const std::string &s) {  // UserID / Password
    if (s.empty() || s.size() > 30) return false;
    for (char c : s)
        if (!isIdChar(c)) return false;
    return true;
}

inline bool validUsername(const std::string &s) {
    if (s.empty() || s.size() > 30) return false;
    for (char c : s)
        if (!isVisible(c)) return false;
    return true;
}

inline bool validIsbn(const std::string &s) {
    if (s.empty() || s.size() > 20) return false;
    for (char c : s)
        if (!isVisible(c)) return false;
    return true;
}

// name / author / keyword field: visible ASCII, no double quotes
inline bool validText(const std::string &s) {
    if (s.empty() || s.size() > 60) return false;
    for (char c : s)
        if (!isVisible(c) || c == '"') return false;
    return true;
}

// Privilege must be a single digit and one of the legal account levels.
inline bool validPrivilege(const std::string &s, int &out) {
    if (s.size() != 1 || !isDigit(s[0])) return false;
    out = s[0] - '0';
    return out == 1 || out == 3 || out == 7;
}

// Non-negative integer, digits only, up to 10 digits, value <= 2147483647.
inline bool parseCount(const std::string &s, long long &out) {
    if (s.empty() || s.size() > 10) return false;
    for (char c : s)
        if (!isDigit(c)) return false;
    long long v = 0;
    for (char c : s) v = v * 10 + (c - '0');
    if (v > 2147483647LL) return false;
    out = v;
    return true;
}

// Price / total-cost: digits and at most one '.', up to 13 characters, at most
// two fractional digits. Result is returned in cents.
inline bool parseMoney(const std::string &s, long long &cents) {
    if (s.empty() || s.size() > 13) return false;
    int dot = -1, digits = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '.') {
            if (dot != -1) return false;  // more than one dot
            dot = (int)i;
        } else if (isDigit(c)) {
            ++digits;
        } else {
            return false;
        }
    }
    if (digits == 0) return false;  // must contain at least one digit
    std::string intPart, fracPart;
    if (dot == -1) {
        intPart = s;
    } else {
        intPart  = s.substr(0, dot);
        fracPart = s.substr(dot + 1);
    }
    if (fracPart.size() > 2) return false;  // fixed two-decimal precision
    long long value = 0;
    for (char c : intPart) value = value * 10 + (c - '0');
    value *= 100;
    if (fracPart.size() == 1) value += (fracPart[0] - '0') * 10;
    else if (fracPart.size() == 2) value += (fracPart[0] - '0') * 10 + (fracPart[1] - '0');
    cents = value;
    return true;
}

// --- money formatting --------------------------------------------------------

inline std::string i128ToString(Int128 v) {
    if (v == 0) return "0";
    bool neg = v < 0;
    UInt128 u = neg ? (UInt128)(-v) : (UInt128)v;
    std::string s;
    while (u > 0) {
        s.push_back(char('0' + (int)(u % 10)));
        u /= 10;
    }
    if (neg) s.push_back('-');
    for (size_t i = 0, j = s.size() - 1; i < j; ++i, --j) std::swap(s[i], s[j]);
    return s;
}

// Format a non-negative amount given in cents as "D.DD".
inline std::string formatMoney(Int128 cents) {
    if (cents < 0) cents = -cents;
    Int128 dollars = cents / 100;
    int      frac    = (int)(cents % 100);
    std::string s = i128ToString(dollars);
    s.push_back('.');
    s.push_back(char('0' + frac / 10));
    s.push_back(char('0' + frac % 10));
    return s;
}

}  // namespace util

#endif  // BOOKSTORE_UTILS_HPP
