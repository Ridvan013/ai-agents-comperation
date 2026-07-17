#ifndef HASHMAP_HPP
#define HASHMAP_HPP

#include <cstring>
#include <string>

#include "vector.hpp"

inline unsigned str_hash(const char *s) {
  unsigned h = 2166136261u;
  while (*s) {
    h ^= (unsigned char)(*s++);
    h *= 16777619u;
  }
  return h;
}

inline unsigned str_hash(const std::string &s) { return str_hash(s.c_str()); }

// Open-addressing hash map: string key -> Value. Tombstones for erase.
template <typename V>
class HashMap {
  struct Slot {
    char *key;
    V val;
    char state;  // 0 empty, 1 occupied, 2 tombstone
    Slot() : key(nullptr), state(0) {}
  };

  Slot *tab_;
  int cap_;
  int sz_;
  int tombs_;

  static char *dup(const char *s) {
    int n = (int)std::strlen(s);
    char *p = new char[n + 1];
    std::memcpy(p, s, n + 1);
    return p;
  }

  void rehash(int nc) {
    Slot *old = tab_;
    int oc = cap_;
    tab_ = new Slot[nc];
    cap_ = nc;
    sz_ = 0;
    tombs_ = 0;
    for (int i = 0; i < oc; ++i) {
      if (old[i].state == 1) {
        insert_move(old[i].key, old[i].val);
        old[i].key = nullptr;
      } else if (old[i].key) {
        delete[] old[i].key;
      }
    }
    delete[] old;
  }

  void insert_move(char *key, const V &val) {
    unsigned h = str_hash(key) % (unsigned)cap_;
    while (tab_[h].state == 1) {
      h = (h + 1) % (unsigned)cap_;
    }
    tab_[h].key = key;
    tab_[h].val = val;
    tab_[h].state = 1;
    ++sz_;
  }

  void maybe_rehash() {
    if (sz_ * 2 >= cap_ || (sz_ + tombs_) * 2 >= cap_) {
      int nc = cap_ * 2;
      if (nc < 16) nc = 16;
      rehash(nc);
    }
  }

 public:
  HashMap() : tab_(nullptr), cap_(0), sz_(0), tombs_(0) {
    cap_ = 16;
    tab_ = new Slot[cap_];
  }
  ~HashMap() {
    for (int i = 0; i < cap_; ++i) {
      if (tab_[i].key) delete[] tab_[i].key;
    }
    delete[] tab_;
  }

  HashMap(const HashMap &) = delete;
  HashMap &operator=(const HashMap &) = delete;

  void clear() {
    for (int i = 0; i < cap_; ++i) {
      if (tab_[i].key) {
        delete[] tab_[i].key;
        tab_[i].key = nullptr;
      }
      tab_[i].state = 0;
    }
    sz_ = 0;
    tombs_ = 0;
  }

  bool put(const char *key, const V &val) {
    maybe_rehash();
    unsigned h0 = str_hash(key) % (unsigned)cap_;
    unsigned h = h0;
    int tomb = -1;
    do {
      if (tab_[h].state == 0) {
        int slot = (tomb >= 0) ? tomb : (int)h;
        if (tab_[slot].key) delete[] tab_[slot].key;
        tab_[slot].key = dup(key);
        tab_[slot].val = val;
        if (tab_[slot].state == 2) --tombs_;
        tab_[slot].state = 1;
        ++sz_;
        return true;
      }
      if (tab_[h].state == 2) {
        if (tomb < 0) tomb = (int)h;
      } else if (std::strcmp(tab_[h].key, key) == 0) {
        tab_[h].val = val;
        return false;  // updated existing
      }
      h = (h + 1) % (unsigned)cap_;
    } while (h != h0);
    return true;
  }

  bool put(const std::string &key, const V &val) { return put(key.c_str(), val); }

  bool get(const char *key, V &out) const {
    if (cap_ == 0) return false;
    unsigned h0 = str_hash(key) % (unsigned)cap_;
    unsigned h = h0;
    do {
      if (tab_[h].state == 0) return false;
      if (tab_[h].state == 1 && std::strcmp(tab_[h].key, key) == 0) {
        out = tab_[h].val;
        return true;
      }
      h = (h + 1) % (unsigned)cap_;
    } while (h != h0);
    return false;
  }

  bool get(const std::string &key, V &out) const { return get(key.c_str(), out); }

  V *find(const char *key) {
    if (cap_ == 0) return nullptr;
    unsigned h0 = str_hash(key) % (unsigned)cap_;
    unsigned h = h0;
    do {
      if (tab_[h].state == 0) return nullptr;
      if (tab_[h].state == 1 && std::strcmp(tab_[h].key, key) == 0) {
        return &tab_[h].val;
      }
      h = (h + 1) % (unsigned)cap_;
    } while (h != h0);
    return nullptr;
  }

  V *find(const std::string &key) { return find(key.c_str()); }

  bool erase(const char *key) {
    if (cap_ == 0) return false;
    unsigned h0 = str_hash(key) % (unsigned)cap_;
    unsigned h = h0;
    do {
      if (tab_[h].state == 0) return false;
      if (tab_[h].state == 1 && std::strcmp(tab_[h].key, key) == 0) {
        tab_[h].state = 2;
        ++tombs_;
        --sz_;
        return true;
      }
      h = (h + 1) % (unsigned)cap_;
    } while (h != h0);
    return false;
  }

  bool erase(const std::string &key) { return erase(key.c_str()); }

  bool contains(const char *key) const {
    V tmp;
    return get(key, tmp);
  }

  int size() const { return sz_; }
};

// Set of strings (login set)
class StringSet {
  HashMap<char> map_;

 public:
  void insert(const std::string &s) { map_.put(s, 1); }
  bool erase(const std::string &s) { return map_.erase(s); }
  bool contains(const std::string &s) const {
    char v;
    return map_.get(s, v);
  }
  void clear() { map_.clear(); }
  int size() const { return map_.size(); }
};

#endif
