#ifndef VECTOR_HPP
#define VECTOR_HPP

#include <cstddef>
#include <cstring>
#include <new>

template <typename T>
class Vector {
  T *data_;
  int size_;
  int cap_;

  void ensure(int need) {
    if (need <= cap_) return;
    int nc = cap_ == 0 ? 4 : cap_;
    while (nc < need) nc *= 2;
    T *nd = static_cast<T *>(::operator new(sizeof(T) * nc));
    for (int i = 0; i < size_; ++i) {
      new (nd + i) T(data_[i]);
      data_[i].~T();
    }
    if (data_) ::operator delete(data_);
    data_ = nd;
    cap_ = nc;
  }

 public:
  Vector() : data_(nullptr), size_(0), cap_(0) {}
  Vector(const Vector &o) : data_(nullptr), size_(0), cap_(0) {
    ensure(o.size_);
    for (int i = 0; i < o.size_; ++i) {
      new (data_ + i) T(o.data_[i]);
    }
    size_ = o.size_;
  }
  Vector &operator=(const Vector &o) {
    if (this == &o) return *this;
    clear();
    ensure(o.size_);
    for (int i = 0; i < o.size_; ++i) {
      new (data_ + i) T(o.data_[i]);
    }
    size_ = o.size_;
    return *this;
  }
  ~Vector() { clear(); if (data_) ::operator delete(data_); }

  void push_back(const T &v) {
    ensure(size_ + 1);
    new (data_ + size_) T(v);
    ++size_;
  }
  void pop_back() {
    if (size_ > 0) {
      --size_;
      data_[size_].~T();
    }
  }
  T &operator[](int i) { return data_[i]; }
  const T &operator[](int i) const { return data_[i]; }
  T &back() { return data_[size_ - 1]; }
  const T &back() const { return data_[size_ - 1]; }
  int size() const { return size_; }
  bool empty() const { return size_ == 0; }
  void clear() {
    for (int i = 0; i < size_; ++i) data_[i].~T();
    size_ = 0;
  }
  void resize(int n) {
    if (n < size_) {
      for (int i = n; i < size_; ++i) data_[i].~T();
      size_ = n;
    } else {
      ensure(n);
      for (int i = size_; i < n; ++i) new (data_ + i) T();
      size_ = n;
    }
  }
  void resize(int n, const T &val) {
    if (n < size_) {
      for (int i = n; i < size_; ++i) data_[i].~T();
      size_ = n;
    } else {
      ensure(n);
      for (int i = size_; i < n; ++i) new (data_ + i) T(val);
      size_ = n;
    }
  }
  T *begin() { return data_; }
  T *end() { return data_ + size_; }
  const T *begin() const { return data_; }
  const T *end() const { return data_ + size_; }

  void erase_at(int idx) {
    data_[idx].~T();
    for (int i = idx; i + 1 < size_; ++i) {
      new (data_ + i) T(data_[i + 1]);
      data_[i + 1].~T();
    }
    --size_;
  }
};

#endif
