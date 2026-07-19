#ifndef FILE_ARRAY_HPP
#define FILE_ARRAY_HPP

#include <fstream>
#include <string>

// Disk-backed array. CacheSize is reserved for future tuning.
template <typename T, int CacheSize>
class CachedFileArray {
  std::fstream file_;
  std::string path_;
  int size_;

 public:
  CachedFileArray() : size_(0) { (void)CacheSize; }

  void open(const std::string &path) {
    close();
    path_ = path;
    file_.open(path.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    if (!file_) {
      file_.clear();
      file_.open(path.c_str(), std::ios::out | std::ios::binary);
      file_.close();
      file_.open(path.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    }
    file_.seekg(0, std::ios::end);
    std::streamoff bytes = file_.tellg();
    if (bytes < 0) bytes = 0;
    size_ = static_cast<int>(bytes / static_cast<std::streamoff>(sizeof(T)));
    file_.seekg(0, std::ios::beg);
    file_.seekp(0, std::ios::beg);
  }

  void close() {
    if (file_.is_open()) {
      file_.flush();
      file_.close();
    }
    size_ = 0;
  }

  int size() const { return size_; }

  int push_back(const T &value) {
    int idx = size_;
    file_.clear();
    file_.seekp(static_cast<std::streamoff>(idx) * sizeof(T), std::ios::beg);
    file_.write(reinterpret_cast<const char *>(&value), sizeof(T));
    file_.flush();
    ++size_;
    return idx;
  }

  T read(int idx) {
    T value;
    file_.clear();
    file_.seekg(static_cast<std::streamoff>(idx) * sizeof(T), std::ios::beg);
    file_.read(reinterpret_cast<char *>(&value), sizeof(T));
    return value;
  }

  void update(int idx, const T &value) {
    file_.clear();
    file_.seekp(static_cast<std::streamoff>(idx) * sizeof(T), std::ios::beg);
    file_.write(reinterpret_cast<const char *>(&value), sizeof(T));
    file_.flush();
  }
};

#endif
