#ifndef TICKET_STORAGE_HPP
#define TICKET_STORAGE_HPP

#include <cstdio>
#include <string>

#include "utils.hpp"

// ---------------------------------------------------------------------------
// MemoryRiver: a flat file of fixed-size records with a free list, used for
// variable-lifetime records that are too large to embed directly in B+ tree
// nodes (train metadata, order records).
//
// File layout:
//   [ int info[INFO] ][ int freeHead ][ record 0 ][ record 1 ] ...
// A freed slot stores, in its first sizeof(int) bytes, the previous freeHead.
// ---------------------------------------------------------------------------
template <class T, int INFO = 2>
class MemoryRiver {
  std::FILE *file_ = nullptr;
  std::string name_;
  static const long BASE = (long)(INFO + 1) * (long)sizeof(int);

  long offsetOf(int index) const { return BASE + (long)index * (long)sizeof(T); }

 public:
  void open(const std::string &fname) {
    name_ = fname;
    file_ = std::fopen(fname.c_str(), "rb+");
    if (!file_) {
      file_ = std::fopen(fname.c_str(), "wb+");
      int zero = 0;
      for (int i = 0; i < INFO; ++i) std::fwrite(&zero, sizeof(int), 1, file_);
      int freeHead = -1;
      std::fwrite(&freeHead, sizeof(int), 1, file_);
      std::fflush(file_);
    }
  }
  void close() {
    if (file_) {
      std::fflush(file_);
      std::fclose(file_);
      file_ = nullptr;
    }
  }
  void reset() {
    // truncate to just the header, free list empty
    close();
    file_ = std::fopen(name_.c_str(), "wb+");
    int zero = 0;
    for (int i = 0; i < INFO; ++i) std::fwrite(&zero, sizeof(int), 1, file_);
    int freeHead = -1;
    std::fwrite(&freeHead, sizeof(int), 1, file_);
    std::fflush(file_);
  }

  void writeInfo(int idx, int val) {
    std::fseek(file_, (long)idx * sizeof(int), SEEK_SET);
    std::fwrite(&val, sizeof(int), 1, file_);
  }
  int readInfo(int idx) {
    std::fseek(file_, (long)idx * sizeof(int), SEEK_SET);
    int v = 0;
    std::fread(&v, sizeof(int), 1, file_);
    return v;
  }

  int freeHead() {
    std::fseek(file_, (long)INFO * sizeof(int), SEEK_SET);
    int v = -1;
    std::fread(&v, sizeof(int), 1, file_);
    return v;
  }
  void setFreeHead(int v) {
    std::fseek(file_, (long)INFO * sizeof(int), SEEK_SET);
    std::fwrite(&v, sizeof(int), 1, file_);
  }
  int recordCount() {
    std::fseek(file_, 0, SEEK_END);
    long sz = std::ftell(file_);
    return (int)((sz - BASE) / (long)sizeof(T));
  }

  // Allocate a slot and store `t`; returns its index.
  int allocate(const T &t) {
    int fh = freeHead();
    int index;
    if (fh != -1) {
      int next = -1;
      std::fseek(file_, offsetOf(fh), SEEK_SET);
      std::fread(&next, sizeof(int), 1, file_);
      setFreeHead(next);
      index = fh;
    } else {
      index = recordCount();
    }
    std::fseek(file_, offsetOf(index), SEEK_SET);
    std::fwrite(&t, sizeof(T), 1, file_);
    return index;
  }
  void read(int index, T &t) {
    std::fseek(file_, offsetOf(index), SEEK_SET);
    std::fread(&t, sizeof(T), 1, file_);
  }
  void update(int index, const T &t) {
    std::fseek(file_, offsetOf(index), SEEK_SET);
    std::fwrite(&t, sizeof(T), 1, file_);
  }
  void erase(int index) {
    int fh = freeHead();
    std::fseek(file_, offsetOf(index), SEEK_SET);
    std::fwrite(&fh, sizeof(int), 1, file_);
    setFreeHead(index);
  }
};

// ---------------------------------------------------------------------------
// RawIntFile: an append-only file of ints with random read/write access,
// used for per-(train,day) seat availability.
// ---------------------------------------------------------------------------
class RawIntFile {
  std::FILE *file_ = nullptr;
  std::string name_;

 public:
  void open(const std::string &fname) {
    name_ = fname;
    file_ = std::fopen(fname.c_str(), "rb+");
    if (!file_) file_ = std::fopen(fname.c_str(), "wb+");
  }
  void close() {
    if (file_) {
      std::fflush(file_);
      std::fclose(file_);
      file_ = nullptr;
    }
  }
  void reset() {
    close();
    file_ = std::fopen(name_.c_str(), "wb+");
    std::fflush(file_);
  }
  // number of ints currently in file
  long count() {
    std::fseek(file_, 0, SEEK_END);
    return std::ftell(file_) / (long)sizeof(int);
  }
  // append `n` ints all equal to `val`; returns starting index.
  long appendFill(int n, int val) {
    long start = count();
    std::fseek(file_, 0, SEEK_END);
    for (int i = 0; i < n; ++i) std::fwrite(&val, sizeof(int), 1, file_);
    return start;
  }
  void readRange(long start, int n, int *out) {
    std::fseek(file_, start * (long)sizeof(int), SEEK_SET);
    std::fread(out, sizeof(int), n, file_);
  }
  void writeRange(long start, int n, const int *in) {
    std::fseek(file_, start * (long)sizeof(int), SEEK_SET);
    std::fwrite(in, sizeof(int), n, file_);
  }
};

#endif
