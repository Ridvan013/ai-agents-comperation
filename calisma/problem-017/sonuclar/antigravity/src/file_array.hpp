#ifndef FILE_ARRAY_HPP
#define FILE_ARRAY_HPP

#include <fstream>
#include <string>

template <typename T, int CacheSize = 2000>
class CachedFileArray {
  std::fstream fs;
  int count_;
  int item_size_;

  struct Node {
    int id;
    T val;
    int prev, next;
    bool dirty;
    bool used;
  };
  Node cache_[CacheSize];
  int head_, tail_, cache_cnt_;
  int *map_; // id -> cache_index
  int map_cap_;

  void push_front(int cidx) {
    if (head_ == -1) {
      head_ = tail_ = cidx;
      cache_[cidx].prev = cache_[cidx].next = -1;
    } else {
      cache_[cidx].next = head_;
      cache_[cidx].prev = -1;
      cache_[head_].prev = cidx;
      head_ = cidx;
    }
  }

  void remove_node(int cidx) {
    if (cache_[cidx].prev != -1) cache_[cache_[cidx].prev].next = cache_[cidx].next;
    else head_ = cache_[cidx].next;
    if (cache_[cidx].next != -1) cache_[cache_[cidx].next].prev = cache_[cidx].prev;
    else tail_ = cache_[cidx].prev;
  }

  void evict() {
    int cidx = tail_;
    if (cidx == -1) return;
    remove_node(cidx);
    if (cache_[cidx].dirty) {
      fs.seekp(sizeof(int) + cache_[cidx].id * item_size_);
      fs.write((const char *)&cache_[cidx].val, item_size_);
    }
    map_[cache_[cidx].id] = -1;
    cache_[cidx].used = false;
    cache_cnt_--;
  }

  void ensure_map(int id) {
    if (id >= map_cap_) {
      int new_cap = map_cap_ == 0 ? 1024 : map_cap_;
      while (id >= new_cap) new_cap *= 2;
      int *new_map = new int[new_cap];
      for (int i = 0; i < map_cap_; ++i) new_map[i] = map_[i];
      for (int i = map_cap_; i < new_cap; ++i) new_map[i] = -1;
      if (map_) delete[] map_;
      map_ = new_map;
      map_cap_ = new_cap;
    }
  }

  int get_free_slot() {
    for (int i = 0; i < CacheSize; ++i) {
      if (!cache_[i].used) return i;
    }
    return -1;
  }

 public:
  CachedFileArray() : count_(0), item_size_(sizeof(T)), head_(-1), tail_(-1), cache_cnt_(0), map_(nullptr), map_cap_(0) {
    for (int i = 0; i < CacheSize; ++i) {
      cache_[i].used = false;
      cache_[i].dirty = false;
    }
  }

  ~CachedFileArray() {
    close();
    if (map_) delete[] map_;
  }

  void open(const std::string &path) {
    fs.open(path.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    if (!fs) {
      fs.clear();
      fs.open(path.c_str(), std::ios::out | std::ios::binary);
      count_ = 0;
      fs.write((char *)&count_, sizeof(int));
      fs.close();
      fs.open(path.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    } else {
      fs.seekg(0);
      fs.read((char *)&count_, sizeof(int));
    }
    ensure_map(count_ + 1000);
  }

  void close() {
    if (fs.is_open()) {
      for (int cidx = head_; cidx != -1; cidx = cache_[cidx].next) {
        if (cache_[cidx].dirty) {
          fs.seekp(sizeof(int) + cache_[cidx].id * item_size_);
          fs.write((const char *)&cache_[cidx].val, item_size_);
          cache_[cidx].dirty = false;
        }
      }
      fs.seekp(0);
      fs.write((char *)&count_, sizeof(int));
      fs.close();
    }
  }

  int push_back(const T &val) {
    int id = count_++;
    ensure_map(id);
    if (cache_cnt_ == CacheSize) evict();
    int cidx = get_free_slot();
    cache_[cidx].id = id;
    cache_[cidx].val = val;
    cache_[cidx].dirty = true;
    cache_[cidx].used = true;
    map_[id] = cidx;
    push_front(cidx);
    cache_cnt_++;
    return id;
  }

  void update(int id, const T &val) {
    ensure_map(id);
    int cidx = map_[id];
    if (cidx != -1) {
      cache_[cidx].val = val;
      cache_[cidx].dirty = true;
      remove_node(cidx);
      push_front(cidx);
    } else {
      if (cache_cnt_ == CacheSize) evict();
      cidx = get_free_slot();
      cache_[cidx].id = id;
      cache_[cidx].val = val;
      cache_[cidx].dirty = true;
      cache_[cidx].used = true;
      map_[id] = cidx;
      push_front(cidx);
      cache_cnt_++;
    }
  }

  T read(int id) {
    ensure_map(id);
    int cidx = map_[id];
    if (cidx != -1) {
      remove_node(cidx);
      push_front(cidx);
      return cache_[cidx].val;
    }
    if (cache_cnt_ == CacheSize) evict();
    cidx = get_free_slot();
    T val;
    fs.seekg(sizeof(int) + id * item_size_);
    fs.read((char *)&val, item_size_);
    cache_[cidx].id = id;
    cache_[cidx].val = val;
    cache_[cidx].dirty = false;
    cache_[cidx].used = true;
    map_[id] = cidx;
    push_front(cidx);
    cache_cnt_++;
    return val;
  }
  
  T& get_ref(int id) {
    ensure_map(id);
    int cidx = map_[id];
    if (cidx != -1) {
      remove_node(cidx);
      push_front(cidx);
      cache_[cidx].dirty = true;
      return cache_[cidx].val;
    }
    if (cache_cnt_ == CacheSize) evict();
    cidx = get_free_slot();
    T val;
    fs.seekg(sizeof(int) + id * item_size_);
    fs.read((char *)&val, item_size_);
    cache_[cidx].id = id;
    cache_[cidx].val = val;
    cache_[cidx].dirty = true;
    cache_[cidx].used = true;
    map_[id] = cidx;
    push_front(cidx);
    cache_cnt_++;
    return cache_[cidx].val;
  }

  int size() const { return count_; }

  void clear(const std::string &path) {
    close();
    fs.open(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    count_ = 0;
    fs.write((char *)&count_, sizeof(int));
    fs.close();
    if (map_) {
      for (int i = 0; i < map_cap_; ++i) map_[i] = -1;
    }
    head_ = tail_ = -1;
    cache_cnt_ = 0;
    for (int i = 0; i < CacheSize; ++i) {
      cache_[i].used = false;
      cache_[i].dirty = false;
    }
    open(path);
  }
};

#endif
