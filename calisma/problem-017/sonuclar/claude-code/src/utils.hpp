#ifndef TICKET_UTILS_HPP
#define TICKET_UTILS_HPP

#include <cstring>
#include <cstdint>
#include <string>

// ---------------------------------------------------------------------------
// Small self-contained utilities. The assignment forbids STL containers
// (except std::string) and the <algorithm> header, so we roll our own.
// ---------------------------------------------------------------------------

template <class T>
inline T my_min(const T &a, const T &b) { return a < b ? a : b; }
template <class T>
inline T my_max(const T &a, const T &b) { return a < b ? b : a; }
template <class T>
inline void my_swap(T &a, T &b) { T t = a; a = b; b = t; }

// Fixed-capacity, zero-padded string usable as a B+ tree key.
// Comparison is byte-lexicographic on the zero-padded buffer, which matches
// ordinary lexicographic ordering for our (ASCII / UTF-8) identifiers.
template <int N>
struct FixedStr {
  char s[N];
  FixedStr() { memset(s, 0, N); }
  FixedStr(const std::string &str) {
    memset(s, 0, N);
    int n = (int)str.size();
    if (n > N) n = N; // identifiers are guaranteed to fit
    memcpy(s, str.data(), n);
  }
  FixedStr(const char *str) {
    memset(s, 0, N);
    int n = (int)strlen(str);
    if (n > N) n = N;
    memcpy(s, str, n);
  }
  void assign(const std::string &str) {
    memset(s, 0, N);
    int n = (int)str.size();
    if (n > N) n = N;
    memcpy(s, str.data(), n);
  }
  std::string str() const {
    // buffer is zero padded; length is up to first NUL or N.
    int n = 0;
    while (n < N && s[n] != '\0') ++n;
    return std::string(s, s + n);
  }
  bool operator<(const FixedStr &o) const { return memcmp(s, o.s, N) < 0; }
  bool operator>(const FixedStr &o) const { return memcmp(s, o.s, N) > 0; }
  bool operator==(const FixedStr &o) const { return memcmp(s, o.s, N) == 0; }
  bool operator!=(const FixedStr &o) const { return memcmp(s, o.s, N) != 0; }
  bool operator<=(const FixedStr &o) const { return memcmp(s, o.s, N) <= 0; }
  bool operator>=(const FixedStr &o) const { return memcmp(s, o.s, N) >= 0; }
};

// A dynamic array (used only for transient, bounded per-command work sets).
template <class T>
class Vector {
  T *data_;
  int size_;
  int cap_;
  void grow(int need) {
    int nc = cap_ ? cap_ * 2 : 4;
    while (nc < need) nc *= 2;
    T *nd = new T[nc];
    for (int i = 0; i < size_; ++i) nd[i] = data_[i];
    delete[] data_;
    data_ = nd;
    cap_ = nc;
  }

 public:
  Vector() : data_(nullptr), size_(0), cap_(0) {}
  Vector(const Vector &o) : data_(nullptr), size_(0), cap_(0) {
    if (o.size_) {
      data_ = new T[o.size_];
      cap_ = o.size_;
      size_ = o.size_;
      for (int i = 0; i < size_; ++i) data_[i] = o.data_[i];
    }
  }
  Vector &operator=(const Vector &o) {
    if (this == &o) return *this;
    delete[] data_;
    data_ = nullptr;
    size_ = cap_ = 0;
    if (o.size_) {
      data_ = new T[o.size_];
      cap_ = o.size_;
      size_ = o.size_;
      for (int i = 0; i < size_; ++i) data_[i] = o.data_[i];
    }
    return *this;
  }
  ~Vector() { delete[] data_; }
  int size() const { return size_; }
  bool empty() const { return size_ == 0; }
  void clear() { size_ = 0; }
  void push_back(const T &v) {
    if (size_ == cap_) grow(size_ + 1);
    data_[size_++] = v;
  }
  T &operator[](int i) { return data_[i]; }
  const T &operator[](int i) const { return data_[i]; }
  T &back() { return data_[size_ - 1]; }
};

// Simple insertion/quick sort so we avoid <algorithm>. Cmp(a,b) => a before b.
template <class T, class Cmp>
void insertion_sort(T *a, int n, Cmp cmp) {
  for (int i = 1; i < n; ++i) {
    T key = a[i];
    int j = i - 1;
    while (j >= 0 && cmp(key, a[j])) {
      a[j + 1] = a[j];
      --j;
    }
    a[j + 1] = key;
  }
}

template <class T, class Cmp>
void quick_sort(T *a, int lo, int hi, Cmp cmp) {
  while (lo < hi) {
    if (hi - lo < 16) {
      insertion_sort(a + lo, hi - lo + 1, cmp);
      return;
    }
    int mid = lo + (hi - lo) / 2;
    // median-of-three pivot
    if (cmp(a[mid], a[lo])) my_swap(a[lo], a[mid]);
    if (cmp(a[hi], a[lo])) my_swap(a[lo], a[hi]);
    if (cmp(a[hi], a[mid])) my_swap(a[mid], a[hi]);
    T pivot = a[mid];
    int i = lo, j = hi;
    while (i <= j) {
      while (cmp(a[i], pivot)) ++i;
      while (cmp(pivot, a[j])) --j;
      if (i <= j) {
        my_swap(a[i], a[j]);
        ++i;
        --j;
      }
    }
    if (j - lo < hi - i) {
      quick_sort(a, lo, j, cmp);
      lo = i;
    } else {
      quick_sort(a, i, hi, cmp);
      hi = j;
    }
  }
}

template <class T, class Cmp>
void sort_vec(Vector<T> &v, Cmp cmp) {
  if (v.size() > 1) quick_sort(&v[0], 0, v.size() - 1, cmp);
}

#endif
