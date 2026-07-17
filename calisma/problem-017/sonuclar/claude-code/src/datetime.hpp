#ifndef TICKET_DATETIME_HPP
#define TICKET_DATETIME_HPP

#include <cstdio>
#include <string>

// Calendar helpers for the (non-leap) year 2021. A "day index" counts days
// from Jan 1 = 0. Absolute time is measured in minutes: day*1440 + minute.
namespace dt {
static const int MLEN[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

inline int monthOffset(int m /*1..12*/) {
  int off = 0;
  for (int i = 0; i < m - 1; ++i) off += MLEN[i];
  return off;
}

// parse "mm-dd" -> day index
inline int parseDate(const std::string &s) {
  int mm = (s[0] - '0') * 10 + (s[1] - '0');
  int dd = (s[3] - '0') * 10 + (s[4] - '0');
  return monthOffset(mm) + (dd - 1);
}

// parse "hh:mi" -> minutes of day
inline int parseTime(const std::string &s) {
  int hh = (s[0] - '0') * 10 + (s[1] - '0');
  int mi = (s[3] - '0') * 10 + (s[4] - '0');
  return hh * 60 + mi;
}

inline void dayToMonthDay(int day, int &mm, int &dd) {
  int m = 0;
  while (m < 12 && day >= MLEN[m]) {
    day -= MLEN[m];
    ++m;
  }
  mm = m + 1;
  dd = day + 1;
}

// format an absolute-minute value into "mm-dd hh:mi"
inline std::string formatAbs(long absMin) {
  long day = absMin / 1440;
  int minute = (int)(absMin - day * 1440);
  int mm, dd;
  dayToMonthDay((int)day, mm, dd);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%02d-%02d %02d:%02d", mm, dd, minute / 60,
                minute % 60);
  return std::string(buf);
}
}  // namespace dt

#endif
