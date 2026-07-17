#ifndef BOOKSTORE_BPLUS_TREE_HPP
#define BOOKSTORE_BPLUS_TREE_HPP

#include <string>
#include <vector>

#include "block_file.hpp"

namespace bookstore {

/// A disk-resident B+ tree mapping unique `Key`s to `Value`s.
///
/// Every node is one block of a BlockFile, so the tree lives entirely in its
/// file and only a bounded LRU window is ever resident. Lookups cost
/// O(log n) block reads, of which the upper levels are almost always cached.
///
/// Nodes are read into local copies and written back rather than mutated
/// through references into the cache: an eviction triggered deeper in a
/// recursive insert/erase would otherwise invalidate a reference still held by
/// an outer frame. Copying a node is a few kilobytes of memcpy and buys
/// immunity from that whole class of bug.
///
/// Invariants:
///   - An internal node with `size` keys has `size + 1` children.
///   - `key[i]` is the smallest key in the subtree under `child[i + 1]`, so
///     `child[i]` covers `[key[i - 1], key[i])`.
///   - Every non-root node holds at least `kMinKeys` keys.
template <class Key, class Value, int Order = 40>
class BPlusTree {
  static_assert(Order >= 4 && Order % 2 == 0, "Order must be even and at least 4");

 public:
  bool open(const std::string &path) { return file_.open(path, TreeMeta{-1}); }
  void close() { file_.close(); }
  void flush() { file_.flush(); }

  /// Inserts `key`. Returns false (changing nothing) if the key already exists.
  bool insert(const Key &key, const Value &value) {
    if (root() == -1) {
      Node leaf;
      leaf.leaf = true;
      leaf.size = 1;
      leaf.next = -1;
      leaf.key[0] = key;
      leaf.val[0] = value;
      int id = file_.allocate();
      file_.write(id, leaf);
      root() = id;
      return true;
    }

    Split split;
    if (!insertInto(root(), key, value, split)) return false;
    if (split.happened) {
      Node newRoot;
      newRoot.leaf = false;
      newRoot.size = 1;
      newRoot.next = -1;
      newRoot.key[0] = split.key;
      newRoot.child[0] = root();
      newRoot.child[1] = split.node;
      int id = file_.allocate();
      file_.write(id, newRoot);
      root() = id;
    }
    return true;
  }

  /// Removes `key`. Returns false if it was not present.
  bool erase(const Key &key) {
    if (root() == -1) return false;
    if (!eraseFrom(root(), key)) return false;

    Node node;
    file_.read(root(), node);
    if (node.leaf) {
      if (node.size == 0) {  // Tree is now empty.
        file_.deallocate(root());
        root() = -1;
      }
    } else if (node.size == 0) {  // Root lost its last separator: drop a level.
      int old = root();
      root() = node.child[0];
      file_.deallocate(old);
    }
    return true;
  }

  bool find(const Key &key, Value &out) {
    if (root() == -1) return false;
    Node node;
    int id = descendToLeaf(key, node);
    (void)id;
    int i = lowerBound(node, key);
    if (i >= node.size || !(node.key[i] == key)) return false;
    out = node.val[i];
    return true;
  }

  bool contains(const Key &key) {
    Value ignored;
    return find(key, ignored);
  }

  /// Appends the values of every key in the inclusive range [lo, hi], in key
  /// order, to `out`.
  void range(const Key &lo, const Key &hi, std::vector<Value> &out) {
    if (root() == -1) return;
    Node node;
    descendToLeaf(lo, node);
    int i = lowerBound(node, lo);
    while (true) {
      while (i < node.size) {
        if (hi < node.key[i]) return;
        out.push_back(node.val[i]);
        ++i;
      }
      int next = node.next;
      if (next == -1) return;
      file_.read(next, node);
      i = 0;
    }
  }

 private:
  static constexpr int kMaxKeys = Order;
  static constexpr int kMinKeys = Order / 2;

  /// One slot of slack lets an insert overflow the node and split afterwards.
  struct Node {
    bool leaf;
    int size;
    int next;  // Leaves only: id of the next leaf, for range scans.
    Key key[kMaxKeys + 1];
    int child[kMaxKeys + 2];  // Internal nodes only.
    Value val[kMaxKeys + 1];  // Leaves only.
  };

  struct TreeMeta {
    int root;
  };

  /// A node split propagating towards the root: `key` is the smallest key of
  /// the new right-hand node `node`.
  struct Split {
    bool happened = false;
    Key key;
    int node = -1;
  };

  int &root() { return file_.meta().root; }

  // -------------------------------------------------------------------------
  // Search helpers
  // -------------------------------------------------------------------------

  /// First index whose key is >= `key`.
  static int lowerBound(const Node &node, const Key &key) {
    int lo = 0, hi = node.size;
    while (lo < hi) {
      int mid = (lo + hi) / 2;
      if (node.key[mid] < key)
        lo = mid + 1;
      else
        hi = mid;
    }
    return lo;
  }

  /// First index whose key is > `key`; equivalently the index of the child that
  /// must contain `key`.
  static int upperBound(const Node &node, const Key &key) {
    int lo = 0, hi = node.size;
    while (lo < hi) {
      int mid = (lo + hi) / 2;
      if (key < node.key[mid])
        hi = mid;
      else
        lo = mid + 1;
    }
    return lo;
  }

  /// Loads the leaf that would contain `key` into `node`; returns its id.
  int descendToLeaf(const Key &key, Node &node) {
    int id = root();
    file_.read(id, node);
    while (!node.leaf) {
      id = node.child[upperBound(node, key)];
      file_.read(id, node);
    }
    return id;
  }

  // -------------------------------------------------------------------------
  // Insertion
  // -------------------------------------------------------------------------

  bool insertInto(int id, const Key &key, const Value &value, Split &split) {
    Node node;
    file_.read(id, node);

    if (node.leaf) {
      int i = lowerBound(node, key);
      if (i < node.size && node.key[i] == key) return false;
      for (int j = node.size; j > i; --j) {
        node.key[j] = node.key[j - 1];
        node.val[j] = node.val[j - 1];
      }
      node.key[i] = key;
      node.val[i] = value;
      ++node.size;
      if (node.size > kMaxKeys)
        splitLeaf(id, node, split);
      else
        file_.write(id, node);
      return true;
    }

    int i = upperBound(node, key);
    Split childSplit;
    if (!insertInto(node.child[i], key, value, childSplit)) return false;
    if (!childSplit.happened) return true;

    for (int j = node.size; j > i; --j) {
      node.key[j] = node.key[j - 1];
      node.child[j + 1] = node.child[j];
    }
    node.key[i] = childSplit.key;
    node.child[i + 1] = childSplit.node;
    ++node.size;
    if (node.size > kMaxKeys)
      splitInternal(id, node, split);
    else
      file_.write(id, node);
    return true;
  }

  void splitLeaf(int id, Node &node, Split &split) {
    const int total = node.size;
    const int leftCount = total / 2;

    Node right;
    right.leaf = true;
    right.size = total - leftCount;
    for (int j = 0; j < right.size; ++j) {
      right.key[j] = node.key[leftCount + j];
      right.val[j] = node.val[leftCount + j];
    }
    right.next = node.next;

    int rightId = file_.allocate();
    node.size = leftCount;
    node.next = rightId;
    file_.write(id, node);
    file_.write(rightId, right);

    split.happened = true;
    split.key = right.key[0];
    split.node = rightId;
  }

  void splitInternal(int id, Node &node, Split &split) {
    const int total = node.size;
    const int mid = total / 2;  // node.key[mid] is promoted to the parent.

    Node right;
    right.leaf = false;
    right.size = total - mid - 1;
    right.next = -1;
    for (int j = 0; j < right.size; ++j) right.key[j] = node.key[mid + 1 + j];
    for (int j = 0; j <= right.size; ++j) right.child[j] = node.child[mid + 1 + j];

    int rightId = file_.allocate();
    Key promoted = node.key[mid];
    node.size = mid;
    file_.write(id, node);
    file_.write(rightId, right);

    split.happened = true;
    split.key = promoted;
    split.node = rightId;
  }

  // -------------------------------------------------------------------------
  // Deletion
  // -------------------------------------------------------------------------

  bool eraseFrom(int id, const Key &key) {
    Node node;
    file_.read(id, node);

    if (node.leaf) {
      int i = lowerBound(node, key);
      if (i >= node.size || !(node.key[i] == key)) return false;
      for (int j = i; j + 1 < node.size; ++j) {
        node.key[j] = node.key[j + 1];
        node.val[j] = node.val[j + 1];
      }
      --node.size;
      file_.write(id, node);
      return true;
    }

    int i = upperBound(node, key);
    if (!eraseFrom(node.child[i], key)) return false;

    Node child;
    file_.read(node.child[i], child);
    if (child.size < kMinKeys) repairUnderflow(id, node, i);
    return true;
  }

  /// Restores the minimum-fill invariant for `parent.child[index]`, by
  /// borrowing from a sibling when one can spare a key and merging otherwise.
  /// A merge can only shrink `parent`; its own underflow is handled one level
  /// up (or, for the root, by erase()).
  void repairUnderflow(int parentId, Node &parent, int index) {
    if (index > 0) {
      Node left;
      file_.read(parent.child[index - 1], left);
      if (left.size > kMinKeys) {
        borrowFromLeft(parent, index, left);
        file_.write(parentId, parent);
        return;
      }
    }
    if (index < parent.size) {
      Node right;
      file_.read(parent.child[index + 1], right);
      if (right.size > kMinKeys) {
        borrowFromRight(parent, index, right);
        file_.write(parentId, parent);
        return;
      }
    }
    // No sibling can spare a key, so fuse the child with one of them. A parent
    // always has at least one separator, so at least one sibling exists.
    mergeChildren(parent, index > 0 ? index - 1 : 0);
    file_.write(parentId, parent);
  }

  void borrowFromLeft(Node &parent, int index, Node &left) {
    Node child;
    file_.read(parent.child[index], child);

    if (child.leaf) {
      for (int j = child.size; j > 0; --j) {
        child.key[j] = child.key[j - 1];
        child.val[j] = child.val[j - 1];
      }
      child.key[0] = left.key[left.size - 1];
      child.val[0] = left.val[left.size - 1];
      ++child.size;
      --left.size;
      parent.key[index - 1] = child.key[0];
    } else {
      for (int j = child.size; j > 0; --j) child.key[j] = child.key[j - 1];
      for (int j = child.size + 1; j > 0; --j) child.child[j] = child.child[j - 1];
      // The old separator is the minimum of the subtree now sitting at
      // child.child[1], and left's last key becomes the new separator.
      child.key[0] = parent.key[index - 1];
      child.child[0] = left.child[left.size];
      ++child.size;
      parent.key[index - 1] = left.key[left.size - 1];
      --left.size;
    }

    file_.write(parent.child[index - 1], left);
    file_.write(parent.child[index], child);
  }

  void borrowFromRight(Node &parent, int index, Node &right) {
    Node child;
    file_.read(parent.child[index], child);

    if (child.leaf) {
      child.key[child.size] = right.key[0];
      child.val[child.size] = right.val[0];
      ++child.size;
      for (int j = 0; j + 1 < right.size; ++j) {
        right.key[j] = right.key[j + 1];
        right.val[j] = right.val[j + 1];
      }
      --right.size;
      parent.key[index] = right.key[0];
    } else {
      child.key[child.size] = parent.key[index];
      child.child[child.size + 1] = right.child[0];
      ++child.size;
      parent.key[index] = right.key[0];
      for (int j = 0; j + 1 < right.size; ++j) right.key[j] = right.key[j + 1];
      for (int j = 0; j < right.size; ++j) right.child[j] = right.child[j + 1];
      --right.size;
    }

    file_.write(parent.child[index + 1], right);
    file_.write(parent.child[index], child);
  }

  /// Merges `parent.child[index + 1]` into `parent.child[index]` and drops the
  /// separator `parent.key[index]`.
  void mergeChildren(Node &parent, int index) {
    int leftId = parent.child[index];
    int rightId = parent.child[index + 1];
    Node left, right;
    file_.read(leftId, left);
    file_.read(rightId, right);

    if (left.leaf) {
      for (int j = 0; j < right.size; ++j) {
        left.key[left.size + j] = right.key[j];
        left.val[left.size + j] = right.val[j];
      }
      left.size += right.size;
      left.next = right.next;
    } else {
      const int base = left.size;
      left.key[base] = parent.key[index];  // Separator sinks into the merged node.
      for (int j = 0; j < right.size; ++j) left.key[base + 1 + j] = right.key[j];
      for (int j = 0; j <= right.size; ++j) left.child[base + 1 + j] = right.child[j];
      left.size = base + 1 + right.size;
    }

    file_.write(leftId, left);
    file_.deallocate(rightId);

    for (int j = index; j + 1 < parent.size; ++j) {
      parent.key[j] = parent.key[j + 1];
      parent.child[j + 1] = parent.child[j + 2];
    }
    --parent.size;
  }

  /// The cache is deliberately small. At order 40, tens of thousands of keys
  /// form only a few dozen internal nodes, so this window keeps every level
  /// above the leaves resident and a point lookup costs a single leaf read --
  /// the minimum a disk-resident index can achieve. Enlarging it past that only
  /// buys leaf hits, which random access rarely repeats; measurements at 1024
  /// were indistinguishable, and caching the whole index would defeat the point
  /// of keeping the data in files at all.
  BlockFile<TreeMeta, Node, 128> file_;
};

}  // namespace bookstore

#endif  // BOOKSTORE_BPLUS_TREE_HPP
