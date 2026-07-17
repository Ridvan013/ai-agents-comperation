#ifndef BOOKSTORE_COMMON_HPP
#define BOOKSTORE_COMMON_HPP

#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

namespace bookstore {

/// Thrown whenever a command is syntactically illegal or its operation fails.
/// Both cases produce exactly the same observable behaviour ("Invalid\n"), so a
/// single exception type keeps the command handlers free of error plumbing.
struct InvalidCommand : std::exception {
  const char *what() const noexcept override { return "Invalid"; }
};

/// Aborts the current command with `Invalid` output.
[[noreturn]] inline void fail() { throw InvalidCommand(); }

inline void failIf(bool condition) {
  if (condition) fail();
}

// ---------------------------------------------------------------------------
// Character classes
// ---------------------------------------------------------------------------

/// The spec's "ASCII characters except invisible characters": printable ASCII
/// with the space excluded, since space is the command separator.
inline bool isVisible(char c) { return c >= 33 && c <= 126; }

inline bool isDigit(char c) { return c >= '0' && c <= '9'; }

inline bool isIdentifierChar(char c) {
  return isDigit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

// ---------------------------------------------------------------------------
// Field validation
// ---------------------------------------------------------------------------

/// [UserID], [Password]: digits, letters and underscores, at most 30 chars.
inline bool isValidIdentifier(const std::string &s) {
  if (s.empty() || s.size() > 30) return false;
  for (char c : s)
    if (!isIdentifierChar(c)) return false;
  return true;
}

/// [Username]: visible ASCII, at most 30 chars.
inline bool isValidUsername(const std::string &s) {
  if (s.empty() || s.size() > 30) return false;
  for (char c : s)
    if (!isVisible(c)) return false;
  return true;
}

/// [ISBN]: visible ASCII, at most 20 chars.
inline bool isValidIsbn(const std::string &s) {
  if (s.empty() || s.size() > 20) return false;
  for (char c : s)
    if (!isVisible(c)) return false;
  return true;
}

/// [BookName], [Author], [Keyword]: visible ASCII except double quotes, at most
/// 60 chars. Structure inside a keyword list is checked by splitKeywords().
inline bool isValidBookText(const std::string &s) {
  if (s.empty() || s.size() > 60) return false;
  for (char c : s)
    if (!isVisible(c) || c == '"') return false;
  return true;
}

/// Splits a `a|b|c` keyword field into its segments. Every segment must be
/// non-empty; `requireUnique` additionally rejects repeated segments, which is
/// what `modify -keyword=` demands.
inline std::vector<std::string> splitKeywords(const std::string &field, bool requireUnique) {
  failIf(!isValidBookText(field));
  std::vector<std::string> parts;
  size_t begin = 0;
  while (true) {
    size_t sep = field.find('|', begin);
    std::string part = field.substr(begin, sep == std::string::npos ? std::string::npos : sep - begin);
    failIf(part.empty());
    parts.push_back(part);
    if (sep == std::string::npos) break;
    begin = sep + 1;
  }
  if (requireUnique) {
    for (size_t i = 0; i < parts.size(); ++i)
      for (size_t j = i + 1; j < parts.size(); ++j) failIf(parts[i] == parts[j]);
  }
  return parts;
}

// ---------------------------------------------------------------------------
// Numbers
// ---------------------------------------------------------------------------

/// [Quantity], [Count]: digits only, at most 10 chars, at most 2^31 - 1.
inline bool parseCount(const std::string &s, long long &out) {
  if (s.empty() || s.size() > 10) return false;
  long long value = 0;
  for (char c : s) {
    if (!isDigit(c)) return false;
    value = value * 10 + (c - '0');
  }
  if (value > 2147483647LL) return false;
  out = value;
  return true;
}

/// [Price], [TotalCost]: digits and at most one '.', at most 13 chars.
///
/// Money is carried as an integer number of cents throughout the system. Binary
/// floating point cannot represent two-decimal values exactly, and `show
/// finance` sums up to hundreds of thousands of them, so a double accumulator
/// would drift into visible one-cent errors.
inline bool parseMoney(const std::string &s, long long &cents) {
  if (s.empty() || s.size() > 13) return false;
  int dots = 0, digits = 0;
  for (char c : s) {
    if (c == '.') {
      if (++dots > 1) return false;
    } else if (isDigit(c)) {
      ++digits;
    } else {
      return false;
    }
  }
  if (digits == 0) return false;

  size_t dot = s.find('.');
  const std::string whole = (dot == std::string::npos) ? s : s.substr(0, dot);
  const std::string frac = (dot == std::string::npos) ? std::string() : s.substr(dot + 1);

  long long value = 0;
  for (char c : whole) value = value * 10 + (c - '0');

  // The system's precision is two decimals; round half-up on anything finer.
  auto digitAt = [&frac](size_t i) -> int { return i < frac.size() ? frac[i] - '0' : 0; };
  long long hundredths = digitAt(0) * 10 + digitAt(1);
  if (digitAt(2) >= 5) ++hundredths;

  cents = value * 100 + hundredths;
  return true;
}

inline std::string formatMoney(long long cents) {
  const long long hundredths = cents % 100;
  std::string text = std::to_string(cents / 100);
  text += '.';
  if (hundredths < 10) text += '0';
  text += std::to_string(hundredths);
  return text;
}

// ---------------------------------------------------------------------------
// FixedStr
// ---------------------------------------------------------------------------

/// A zero-padded, trivially copyable string of at most `N - 1` characters, so
/// records and index keys have a fixed on-disk footprint.
///
/// Ordering is a plain memcmp over the whole buffer. Because the payload is
/// zero-padded and never contains a NUL, that reproduces lexicographic order
/// exactly, and it lets maxSentinel() act as an upper bound for range scans.
template <int N>
struct FixedStr {
  char data[N];

  FixedStr() { std::memset(data, 0, N); }

  explicit FixedStr(const std::string &s) {
    std::memset(data, 0, N);
    std::memcpy(data, s.data(), s.size() < static_cast<size_t>(N - 1) ? s.size() : N - 1);
  }

  std::string str() const {
    int length = 0;
    while (length < N && data[length] != '\0') ++length;
    return std::string(data, length);
  }

  bool empty() const { return data[0] == '\0'; }

  /// Compares greater than any real value; used as the open end of range scans.
  static FixedStr maxSentinel() {
    FixedStr s;
    std::memset(s.data, 0xFF, N);
    return s;
  }

  bool operator<(const FixedStr &other) const { return std::memcmp(data, other.data, N) < 0; }
  bool operator==(const FixedStr &other) const { return std::memcmp(data, other.data, N) == 0; }
};

using IsbnStr = FixedStr<21>;
using TextStr = FixedStr<61>;
using AccountStr = FixedStr<31>;

/// Index key for the name/author/keyword lookups.
///
/// Pairing the searched text with the book's ISBN makes every key unique (so
/// the B+ tree needs no duplicate handling) and makes a range scan emit results
/// already sorted by ISBN, which is exactly the output order `show` requires.
struct TextIsbnKey {
  TextStr text;
  IsbnStr isbn;

  TextIsbnKey() = default;
  TextIsbnKey(const std::string &t, const std::string &i) : text(t), isbn(i) {}

  bool operator<(const TextIsbnKey &other) const {
    int cmp = std::memcmp(text.data, other.text.data, sizeof(text.data));
    if (cmp != 0) return cmp < 0;
    return std::memcmp(isbn.data, other.isbn.data, sizeof(isbn.data)) < 0;
  }
  bool operator==(const TextIsbnKey &other) const {
    return text == other.text && isbn == other.isbn;
  }
};

/// Bounds selecting every entry whose text equals `text`, for a prefix scan.
inline TextIsbnKey textLowerBound(const std::string &text) { return TextIsbnKey(text, std::string()); }

inline TextIsbnKey textUpperBound(const std::string &text) {
  TextIsbnKey key;
  key.text = TextStr(text);
  key.isbn = IsbnStr::maxSentinel();
  return key;
}

}  // namespace bookstore

#endif  // BOOKSTORE_COMMON_HPP
