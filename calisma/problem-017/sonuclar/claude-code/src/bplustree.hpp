#ifndef TICKET_BPLUSTREE_HPP
#define TICKET_BPLUSTREE_HPP

#include <cstdio>
#include <string>

#include "utils.hpp"

// ---------------------------------------------------------------------------
// Disk-resident B+ tree mapping Key -> Value (both fixed size, small).
//
// Keys are unique (callers build composite keys for multimaps). Insertion
// splits nodes; deletion is lazy (a key is removed from its leaf, nodes are
// never merged). Lazy deletion keeps search/scan/insert correct while
// avoiding the complexity of rebalancing; freed space is tolerable within the
// generous disk budget.
//
// Nodes are cached in memory through a fixed-capacity LRU cache so that the
// working set stays well under the memory limit.
// ---------------------------------------------------------------------------
template <class Key, class Value, int M = 64, int CACHE = 512>
class BPlusTree {
  struct Node {
    int isLeaf;      // 1 leaf, 0 internal
    int cnt;         // number of keys
    int next;        // leaf: next leaf page id (-1 none)
    Key key[M + 1];  // room for one overflow key before split
    int child[M + 2];
    Value val[M + 1];
  };

  struct Header {
    int root;      // root page id, -1 if empty tree
    int pageCnt;   // next fresh page id to allocate (page 0 is header)
    int freeHead;  // free page list head, -1 none
    int extra[5];  // caller-usable persistent counters
  };

  std::FILE *file_ = nullptr;
  std::string name_;
  Header header_;
  static const long PAGE = (long)sizeof(Node);

  // --- LRU cache -----------------------------------------------------------
  static const int HB = 2 * CACHE + 7;  // hash buckets
  Node cnode_[CACHE];
  int cpid_[CACHE];
  bool cdirty_[CACHE];
  int lprev_[CACHE], lnext_[CACHE];
  int hnext_[CACHE];
  int bucket_[HB];
  int lruHead_, lruTail_;  // head = most recent
  int cacheUsed_;

  int hashPid(int pid) const { return ((unsigned)pid * 2654435761u) % HB; }

  void lruRemove(int i) {
    if (lprev_[i] != -1) lnext_[lprev_[i]] = lnext_[i]; else lruHead_ = lnext_[i];
    if (lnext_[i] != -1) lprev_[lnext_[i]] = lprev_[i]; else lruTail_ = lprev_[i];
  }
  void lruPushFront(int i) {
    lprev_[i] = -1;
    lnext_[i] = lruHead_;
    if (lruHead_ != -1) lprev_[lruHead_] = i;
    lruHead_ = i;
    if (lruTail_ == -1) lruTail_ = i;
  }
  void bucketInsert(int i) {
    int b = hashPid(cpid_[i]);
    hnext_[i] = bucket_[b];
    bucket_[b] = i;
  }
  void bucketRemove(int i) {
    int b = hashPid(cpid_[i]);
    int cur = bucket_[b], prev = -1;
    while (cur != -1) {
      if (cur == i) {
        if (prev == -1) bucket_[b] = hnext_[cur];
        else hnext_[prev] = hnext_[cur];
        return;
      }
      prev = cur;
      cur = hnext_[cur];
    }
  }
  int findSlot(int pid) {
    int b = hashPid(pid);
    int cur = bucket_[b];
    while (cur != -1) {
      if (cpid_[cur] == pid) return cur;
      cur = hnext_[cur];
    }
    return -1;
  }
  void diskRead(int pid, Node &n) {
    std::fseek(file_, (long)pid * PAGE, SEEK_SET);
    std::fread(&n, sizeof(Node), 1, file_);
  }
  void diskWrite(int pid, const Node &n) {
    std::fseek(file_, (long)pid * PAGE, SEEK_SET);
    std::fwrite(&n, sizeof(Node), 1, file_);
  }
  // Ensure page is cached; return its slot index.
  int fetch(int pid) {
    int s = findSlot(pid);
    if (s != -1) {
      lruRemove(s);
      lruPushFront(s);
      return s;
    }
    int slot;
    if (cacheUsed_ < CACHE) {
      slot = cacheUsed_++;
    } else {
      slot = lruTail_;
      lruRemove(slot);
      if (cdirty_[slot]) diskWrite(cpid_[slot], cnode_[slot]);
      bucketRemove(slot);
    }
    diskRead(pid, cnode_[slot]);
    cpid_[slot] = pid;
    cdirty_[slot] = false;
    bucketInsert(slot);
    lruPushFront(slot);
    return slot;
  }

  void readNode(int pid, Node &out) { out = cnode_[fetch(pid)]; }
  void writeNode(int pid, const Node &n) {
    int s = fetch(pid);
    cnode_[s] = n;
    cdirty_[s] = true;
  }

  int allocPage() {
    if (header_.freeHead != -1) {
      int pid = header_.freeHead;
      Node n;
      diskRead(pid, n);       // freed page stores next-free in child[0]
      header_.freeHead = n.child[0];
      return pid;
    }
    return header_.pageCnt++;
  }

  void writeHeader() {
    std::fseek(file_, 0, SEEK_SET);
    std::fwrite(&header_, sizeof(Header), 1, file_);
  }

  // routing inside an internal node: index of child to descend into.
  int route(const Node &n, const Key &k) const {
    int i = 0;
    while (i < n.cnt && !(k < n.key[i])) ++i;
    return i;
  }
  // leaf lower_bound: first index whose key >= k
  int leafLB(const Node &n, const Key &k) const {
    int lo = 0, hi = n.cnt;
    while (lo < hi) {
      int mid = (lo + hi) >> 1;
      if (n.key[mid] < k) lo = mid + 1; else hi = mid;
    }
    return lo;
  }

 public:
  void open(const std::string &fname) {
    name_ = fname;
    lruHead_ = lruTail_ = -1;
    cacheUsed_ = 0;
    for (int i = 0; i < HB; ++i) bucket_[i] = -1;
    file_ = std::fopen(fname.c_str(), "rb+");
    if (!file_) {
      file_ = std::fopen(fname.c_str(), "wb+");
      header_.root = -1;
      header_.pageCnt = 1;  // page 0 is header
      header_.freeHead = -1;
      for (int i = 0; i < 5; ++i) header_.extra[i] = 0;
      writeHeader();
      std::fflush(file_);
    } else {
      std::fseek(file_, 0, SEEK_SET);
      std::fread(&header_, sizeof(Header), 1, file_);
    }
  }
  void flush() {
    if (!file_) return;
    for (int i = 0; i < cacheUsed_; ++i) {
      if (cdirty_[i]) {
        diskWrite(cpid_[i], cnode_[i]);
        cdirty_[i] = false;
      }
    }
    writeHeader();
    std::fflush(file_);
  }
  void close() {
    if (!file_) return;
    flush();
    std::fclose(file_);
    file_ = nullptr;
  }
  void reset() {
    // drop everything
    if (file_) std::fclose(file_);
    lruHead_ = lruTail_ = -1;
    cacheUsed_ = 0;
    for (int i = 0; i < HB; ++i) bucket_[i] = -1;
    file_ = std::fopen(name_.c_str(), "wb+");
    header_.root = -1;
    header_.pageCnt = 1;
    header_.freeHead = -1;
    for (int i = 0; i < 5; ++i) header_.extra[i] = 0;
    writeHeader();
    std::fflush(file_);
  }

  int getExtra(int i) { return header_.extra[i]; }
  void setExtra(int i, int v) { header_.extra[i] = v; }

  bool find(const Key &k, Value &out) {
    if (header_.root == -1) return false;
    Node n;
    int pid = header_.root;
    readNode(pid, n);
    while (!n.isLeaf) {
      pid = n.child[route(n, k)];
      readNode(pid, n);
    }
    int i = leafLB(n, k);
    if (i < n.cnt && n.key[i] == k) {
      out = n.val[i];
      return true;
    }
    return false;
  }

  // Insert or update key -> val.
 private:
  // Insert into subtree rooted at pid. If a split happened, sets *upKey and
  // *upChild (new right sibling page id) and returns true.
  bool insertRec(int pid, const Key &k, const Value &v, Key *upKey, int *upChild) {
    Node n;
    readNode(pid, n);
    if (n.isLeaf) {
      int i = leafLB(n, k);
      if (i < n.cnt && n.key[i] == k) {  // update existing
        n.val[i] = v;
        writeNode(pid, n);
        return false;
      }
      for (int j = n.cnt; j > i; --j) {
        n.key[j] = n.key[j - 1];
        n.val[j] = n.val[j - 1];
      }
      n.key[i] = k;
      n.val[i] = v;
      ++n.cnt;
      if (n.cnt <= M) {
        writeNode(pid, n);
        return false;
      }
      // split leaf
      int mid = n.cnt / 2;
      Node r;
      r.isLeaf = 1;
      r.cnt = n.cnt - mid;
      for (int j = 0; j < r.cnt; ++j) {
        r.key[j] = n.key[mid + j];
        r.val[j] = n.val[mid + j];
      }
      n.cnt = mid;
      int rpid = allocPage();
      r.next = n.next;
      n.next = rpid;
      writeNode(pid, n);
      writeNode(rpid, r);
      *upKey = r.key[0];
      *upChild = rpid;
      return true;
    } else {
      int ci = route(n, k);
      Key ck;
      int cc;
      if (!insertRec(n.child[ci], k, v, &ck, &cc)) return false;
      // re-read: nested insert may have modified this node's cache copy? No,
      // this node wasn't touched, but cache eviction could have flushed it.
      readNode(pid, n);
      // insert separator ck with right child cc at position ci
      for (int j = n.cnt; j > ci; --j) {
        n.key[j] = n.key[j - 1];
        n.child[j + 1] = n.child[j];
      }
      n.key[ci] = ck;
      n.child[ci + 1] = cc;
      ++n.cnt;
      if (n.cnt <= M) {
        writeNode(pid, n);
        return false;
      }
      // split internal: middle key goes up
      int mid = n.cnt / 2;
      Key midKey = n.key[mid];
      Node r;
      r.isLeaf = 0;
      r.cnt = n.cnt - mid - 1;
      for (int j = 0; j < r.cnt; ++j) r.key[j] = n.key[mid + 1 + j];
      for (int j = 0; j <= r.cnt; ++j) r.child[j] = n.child[mid + 1 + j];
      n.cnt = mid;
      int rpid = allocPage();
      writeNode(pid, n);
      writeNode(rpid, r);
      *upKey = midKey;
      *upChild = rpid;
      return true;
    }
  }

 public:
  void insert(const Key &k, const Value &v) {
    if (header_.root == -1) {
      Node n;
      n.isLeaf = 1;
      n.cnt = 1;
      n.next = -1;
      n.key[0] = k;
      n.val[0] = v;
      int pid = allocPage();
      header_.root = pid;
      writeNode(pid, n);
      return;
    }
    Key upKey;
    int upChild;
    if (insertRec(header_.root, k, v, &upKey, &upChild)) {
      Node root;
      root.isLeaf = 0;
      root.cnt = 1;
      root.key[0] = upKey;
      root.child[0] = header_.root;
      root.child[1] = upChild;
      int pid = allocPage();
      header_.root = pid;
      writeNode(pid, root);
    }
  }

  // Lazy delete: remove key from its leaf; no rebalancing.
  void erase(const Key &k) {
    if (header_.root == -1) return;
    Node n;
    int pid = header_.root;
    readNode(pid, n);
    while (!n.isLeaf) {
      pid = n.child[route(n, k)];
      readNode(pid, n);
    }
    int i = leafLB(n, k);
    if (i < n.cnt && n.key[i] == k) {
      for (int j = i; j + 1 < n.cnt; ++j) {
        n.key[j] = n.key[j + 1];
        n.val[j] = n.val[j + 1];
      }
      --n.cnt;
      writeNode(pid, n);
    }
  }

  // Visit every entry with key in [lo, hi] in ascending order.
  // Fn(const Key&, const Value&) returns true to continue, false to stop.
  template <class Fn>
  void scan(const Key &lo, const Key &hi, Fn fn) {
    if (header_.root == -1) return;
    Node n;
    int pid = header_.root;
    readNode(pid, n);
    while (!n.isLeaf) {
      pid = n.child[route(n, lo)];
      readNode(pid, n);
    }
    int i = leafLB(n, lo);
    while (true) {
      while (i < n.cnt) {
        if (hi < n.key[i]) return;
        if (!fn(n.key[i], n.val[i])) return;
        ++i;
      }
      if (n.next == -1) return;
      pid = n.next;
      readNode(pid, n);
      i = 0;
    }
  }
};

#endif
