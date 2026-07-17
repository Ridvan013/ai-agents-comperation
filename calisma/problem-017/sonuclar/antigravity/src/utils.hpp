#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstdio>
#include <cstring>
#include <string>

#include "vector.hpp"

// Day-of-year in 2021 (non-leap): 0 = Jan 1, ..., 364 = Dec 31
inline int days_in_month(int m) {
  static const int d[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return d[m];
}

inline int date_to_day(int mm, int dd) {
  int d = dd - 1;
  for (int m = 1; m < mm; ++m) d += days_in_month(m);
  return d;
}

inline int parse_date(const std::string &s) {
  // mm-dd
  int mm = (s[0] - '0') * 10 + (s[1] - '0');
  int dd = (s[3] - '0') * 10 + (s[4] - '0');
  return date_to_day(mm, dd);
}

inline void day_to_date(int day, int &mm, int &dd) {
  for (mm = 1; mm <= 12; ++mm) {
    if (day < days_in_month(mm)) {
      dd = day + 1;
      return;
    }
    day -= days_in_month(mm);
  }
  mm = 12;
  dd = 31;
}

inline int parse_time(const std::string &s) {
  // hr:mi
  int hr = (s[0] - '0') * 10 + (s[1] - '0');
  int mi = (s[3] - '0') * 10 + (s[4] - '0');
  return hr * 60 + mi;
}

inline void format_datetime(int abs_min, char *buf) {
  // abs_min = day * 1440 + time_of_day
  int day = abs_min / 1440;
  int tod = abs_min % 1440;
  int mm, dd;
  day_to_date(day, mm, dd);
  int hr = tod / 60;
  int mi = tod % 60;
  std::sprintf(buf, "%02d-%02d %02d:%02d", mm, dd, hr, mi);
}

inline Vector<std::string> split_pipe(const std::string &s) {
  Vector<std::string> res;
  if (s == "_") return res;
  std::string cur;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '|') {
      res.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(s[i]);
    }
  }
  res.push_back(cur);
  return res;
}

inline Vector<int> split_pipe_int(const std::string &s) {
  Vector<int> res;
  if (s == "_") return res;
  int cur = 0;
  bool neg = false;
  bool in = false;
  for (size_t i = 0; i <= s.size(); ++i) {
    char c = (i == s.size()) ? '|' : s[i];
    if (c == '|') {
      if (in) res.push_back(neg ? -cur : cur);
      cur = 0;
      neg = false;
      in = false;
    } else if (c == '-') {
      neg = true;
      in = true;
    } else {
      cur = cur * 10 + (c - '0');
      in = true;
    }
  }
  return res;
}

inline int strcmp_c(const char *a, const char *b) { return std::strcmp(a, b); }

template <typename T, typename Cmp>
void quick_sort(T *a, int l, int r, Cmp cmp) {
  if (l >= r) return;
  T pivot = a[(l + r) / 2];
  int i = l, j = r;
  while (i <= j) {
    while (cmp(a[i], pivot)) ++i;
    while (cmp(pivot, a[j])) --j;
    if (i <= j) {
      T tmp = a[i];
      a[i] = a[j];
      a[j] = tmp;
      ++i;
      --j;
    }
  }
  if (l < j) quick_sort(a, l, j, cmp);
  if (i < r) quick_sort(a, i, r, cmp);
}

template <typename T, typename Cmp>
void sort_vec(Vector<T> &v, Cmp cmp) {
  if (v.size() <= 1) return;
  quick_sort(v.begin(), 0, v.size() - 1, cmp);
}

#endif
