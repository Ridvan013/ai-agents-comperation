#ifndef BOOKSTORE_BLOCK_FILE_HPP
#define BOOKSTORE_BLOCK_FILE_HPP

#include <cstring>
#include <fstream>
#include <list>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace bookstore {

/// Fixed-size block storage backed by a single file.
///
/// On-disk layout: [FileHeader][Meta][Block 0][Block 1] ...
///
/// Blocks are addressed by a dense integer id. Freed blocks go on an intrusive
/// free list whose "next" link is stored in the first bytes of the block
/// payload, so reuse costs no extra file and no extra bookkeeping.
///
/// A write-back LRU cache absorbs repeated hits on hot blocks (the upper levels
/// of a B+ tree are touched by every single lookup) while keeping the resident
/// set bounded. Records themselves always live in the file, never in memory.
template <class Meta, class Block, int CacheCapacity = 256>
class BlockFile {
  static_assert(std::is_trivially_copyable<Meta>::value, "Meta must be trivially copyable");
  static_assert(std::is_trivially_copyable<Block>::value, "Block must be trivially copyable");
  static_assert(sizeof(Block) >= sizeof(int), "Block must have room for a free-list link");

 public:
  BlockFile() = default;
  BlockFile(const BlockFile &) = delete;
  BlockFile &operator=(const BlockFile &) = delete;
  ~BlockFile() { close(); }

  /// Opens `path`, creating and initialising it when absent. Returns true if
  /// the file was freshly created, which callers use to detect a first run.
  bool open(const std::string &path, const Meta &initialMeta) {
    file_.open(path, std::ios::in | std::ios::out | std::ios::binary);
    if (file_.is_open()) {
      file_.seekg(0);
      file_.read(reinterpret_cast<char *>(&header_), sizeof(FileHeader));
      file_.read(reinterpret_cast<char *>(&meta_), sizeof(Meta));
      if (!file_) {  // Truncated or empty file: start over from a clean header.
        file_.clear();
        header_ = FileHeader{0, -1};
        meta_ = initialMeta;
        flushHeader();
        return true;
      }
      return false;
    }

    // std::fstream will not create a file in in|out mode; make it first.
    file_.clear();
    file_.open(path, std::ios::out | std::ios::binary);
    file_.close();
    file_.clear();
    file_.open(path, std::ios::in | std::ios::out | std::ios::binary);
    header_ = FileHeader{0, -1};
    meta_ = initialMeta;
    flushHeader();
    return true;
  }

  void close() {
    if (!file_.is_open()) return;
    flush();
    file_.close();
  }

  /// Writes every dirty cached block plus the header back to the file.
  void flush() {
    for (auto &entry : cache_) {
      if (entry.second.dirty) {
        storeToDisk(entry.first, entry.second.block);
        entry.second.dirty = false;
      }
    }
    flushHeader();
    file_.flush();
  }

  Meta &meta() { return meta_; }
  const Meta &meta() const { return meta_; }

  /// Number of block slots ever handed out (including freed ones).
  int blockCount() const { return header_.blockCount; }

  int allocate() {
    if (header_.freeHead != -1) {
      int id = header_.freeHead;
      Block block;
      read(id, block);
      int next;
      std::memcpy(&next, &block, sizeof(int));
      header_.freeHead = next;
      return id;
    }
    return header_.blockCount++;
  }

  void deallocate(int id) {
    Block block;
    std::memset(&block, 0, sizeof(Block));
    std::memcpy(&block, &header_.freeHead, sizeof(int));
    write(id, block);
    header_.freeHead = id;
  }

  void read(int id, Block &out) {
    auto it = cache_.find(id);
    if (it != cache_.end()) {
      touch(it->second);
      out = it->second.block;
      return;
    }
    CacheSlot slot;
    slot.dirty = false;
    loadFromDisk(id, slot.block);
    out = slot.block;
    admit(id, std::move(slot));
  }

  void write(int id, const Block &in) {
    auto it = cache_.find(id);
    if (it != cache_.end()) {
      it->second.block = in;
      it->second.dirty = true;
      touch(it->second);
      return;
    }
    CacheSlot slot;
    slot.block = in;
    slot.dirty = true;
    admit(id, std::move(slot));
  }

 private:
  struct FileHeader {
    int blockCount;
    int freeHead;
  };

  struct CacheSlot {
    Block block;
    bool dirty;
    std::list<int>::iterator lru;
  };

  static constexpr long long kDataOffset = sizeof(FileHeader) + sizeof(Meta);

  static long long offsetOf(int id) {
    return kDataOffset + static_cast<long long>(id) * sizeof(Block);
  }

  void flushHeader() {
    file_.clear();
    file_.seekp(0);
    file_.write(reinterpret_cast<const char *>(&header_), sizeof(FileHeader));
    file_.write(reinterpret_cast<const char *>(&meta_), sizeof(Meta));
  }

  void loadFromDisk(int id, Block &out) {
    file_.clear();
    file_.seekg(offsetOf(id));
    file_.read(reinterpret_cast<char *>(&out), sizeof(Block));
    if (!file_) {  // Block was allocated but never written yet.
      file_.clear();
      std::memset(&out, 0, sizeof(Block));
    }
  }

  void storeToDisk(int id, const Block &in) {
    file_.clear();
    file_.seekp(offsetOf(id));
    file_.write(reinterpret_cast<const char *>(&in), sizeof(Block));
  }

  void touch(CacheSlot &slot) { lru_.splice(lru_.begin(), lru_, slot.lru); }

  void admit(int id, CacheSlot &&slot) {
    evictIfFull();
    lru_.push_front(id);
    slot.lru = lru_.begin();
    cache_.emplace(id, std::move(slot));
  }

  void evictIfFull() {
    while (static_cast<int>(cache_.size()) >= CacheCapacity) {
      int victim = lru_.back();
      auto it = cache_.find(victim);
      if (it->second.dirty) storeToDisk(victim, it->second.block);
      lru_.pop_back();
      cache_.erase(it);
    }
  }

  std::fstream file_;
  FileHeader header_{0, -1};
  Meta meta_{};
  std::unordered_map<int, CacheSlot> cache_;
  std::list<int> lru_;  // Front is most recently used.
};

}  // namespace bookstore

#endif  // BOOKSTORE_BLOCK_FILE_HPP
