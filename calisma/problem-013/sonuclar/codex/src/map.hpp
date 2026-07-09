/**
 * implement a container like std::map
 */
#ifndef SJTU_MAP_HPP
#define SJTU_MAP_HPP

#include <functional>
#include <cstddef>
#include "utility.hpp"
#include "exceptions.hpp"

namespace sjtu {

template<class Key, class T, class Compare = std::less<Key> >
class map {
 public:
  typedef pair<const Key, T> value_type;

 private:
  struct node {
    value_type *data;
    node *left;
    node *right;
    node *parent;
    int height;

    node() : data(NULL), left(NULL), right(NULL), parent(NULL), height(0) {}

    node(const value_type &value, node *parent_)
        : data(new value_type(value)),
          left(NULL),
          right(NULL),
          parent(parent_),
          height(1) {}

    ~node() { delete data; }
  };

  node *header;
  size_t node_count;
  Compare comp;

  static int height(node *cur) { return cur == NULL ? 0 : cur->height; }

  static int max_int(int lhs, int rhs) { return lhs > rhs ? lhs : rhs; }

  static node *minimum(node *cur) {
    while (cur != NULL && cur->left != NULL) cur = cur->left;
    return cur;
  }

  static node *maximum(node *cur) {
    while (cur != NULL && cur->right != NULL) cur = cur->right;
    return cur;
  }

  static bool is_equal_key(const Key &lhs, const Key &rhs, const Compare &cmp) {
    return !cmp(lhs, rhs) && !cmp(rhs, lhs);
  }

  void update(node *cur) {
    if (cur != NULL) {
      cur->height = max_int(height(cur->left), height(cur->right)) + 1;
    }
  }

  void maintain_header() {
    node *root = header->parent;
    if (root == NULL) {
      header->left = header;
      header->right = header;
    } else {
      header->left = minimum(root);
      header->right = maximum(root);
      root->parent = header;
    }
  }

  void rotate_left(node *cur) {
    node *pivot = cur->right;
    node *parent = cur->parent;
    cur->right = pivot->left;
    if (pivot->left != NULL) pivot->left->parent = cur;
    pivot->left = cur;
    cur->parent = pivot;
    pivot->parent = parent;
    if (parent == header) {
      header->parent = pivot;
    } else if (parent->left == cur) {
      parent->left = pivot;
    } else {
      parent->right = pivot;
    }
    update(cur);
    update(pivot);
  }

  void rotate_right(node *cur) {
    node *pivot = cur->left;
    node *parent = cur->parent;
    cur->left = pivot->right;
    if (pivot->right != NULL) pivot->right->parent = cur;
    pivot->right = cur;
    cur->parent = pivot;
    pivot->parent = parent;
    if (parent == header) {
      header->parent = pivot;
    } else if (parent->left == cur) {
      parent->left = pivot;
    } else {
      parent->right = pivot;
    }
    update(cur);
    update(pivot);
  }

  void rebalance(node *cur) {
    while (cur != NULL && cur != header) {
      update(cur);
      int balance = height(cur->left) - height(cur->right);
      if (balance > 1) {
        if (height(cur->left->left) < height(cur->left->right)) {
          rotate_left(cur->left);
        }
        node *next = cur->parent;
        rotate_right(cur);
        cur = next;
      } else if (balance < -1) {
        if (height(cur->right->right) < height(cur->right->left)) {
          rotate_right(cur->right);
        }
        node *next = cur->parent;
        rotate_left(cur);
        cur = next;
      } else {
        cur = cur->parent;
      }
    }
    maintain_header();
  }

  void destroy(node *cur) {
    if (cur == NULL) return;
    destroy(cur->left);
    destroy(cur->right);
    delete cur;
  }

  node *clone(node *other, node *parent_) {
    if (other == NULL) return NULL;
    node *cur = new node(*(other->data), parent_);
    cur->height = other->height;
    cur->left = clone(other->left, cur);
    cur->right = clone(other->right, cur);
    return cur;
  }

  node *find_node(const Key &key) const {
    node *cur = header->parent;
    while (cur != NULL) {
      if (comp(key, cur->data->first)) {
        cur = cur->left;
      } else if (comp(cur->data->first, key)) {
        cur = cur->right;
      } else {
        return cur;
      }
    }
    return NULL;
  }

  void transplant(node *target, node *replacement) {
    node *parent = target->parent;
    if (parent == header) {
      header->parent = replacement;
    } else if (parent->left == target) {
      parent->left = replacement;
    } else {
      parent->right = replacement;
    }
    if (replacement != NULL) replacement->parent = parent;
  }

  void erase_node(node *target) {
    node *fix_from = NULL;
    if (target->left == NULL) {
      fix_from = target->parent;
      transplant(target, target->right);
    } else if (target->right == NULL) {
      fix_from = target->parent;
      transplant(target, target->left);
    } else {
      node *successor = minimum(target->right);
      if (successor->parent != target) {
        fix_from = successor->parent;
        transplant(successor, successor->right);
        successor->right = target->right;
        successor->right->parent = successor;
      } else {
        fix_from = successor;
      }
      transplant(target, successor);
      successor->left = target->left;
      successor->left->parent = successor;
      update(successor);
    }
    delete target;
    --node_count;
    if (node_count == 0) {
      header->parent = NULL;
      maintain_header();
      return;
    }
    if (fix_from == header) fix_from = header->parent;
    rebalance(fix_from);
  }

  node *next_node(node *cur) const {
    if (cur == header) throw invalid_iterator();
    if (cur->right != NULL) return minimum(cur->right);
    node *parent = cur->parent;
    while (parent != header && cur == parent->right) {
      cur = parent;
      parent = parent->parent;
    }
    return parent;
  }

  node *prev_node(node *cur) const {
    if (cur == header) {
      if (node_count == 0) throw invalid_iterator();
      return header->right;
    }
    if (cur->left != NULL) return maximum(cur->left);
    node *parent = cur->parent;
    while (parent != header && cur == parent->left) {
      cur = parent;
      parent = parent->parent;
    }
    if (parent == header) throw invalid_iterator();
    return parent;
  }

 public:
  class const_iterator;

  class iterator {
    friend class map;
    friend class const_iterator;

   private:
    node *ptr;
    const map *owner;

    iterator(node *ptr_, const map *owner_) : ptr(ptr_), owner(owner_) {}

   public:
    iterator() : ptr(NULL), owner(NULL) {}

    iterator(const iterator &other) : ptr(other.ptr), owner(other.owner) {}

    iterator operator++(int) {
      iterator tmp(*this);
      ++(*this);
      return tmp;
    }

    iterator &operator++() {
      if (owner == NULL || ptr == NULL) throw invalid_iterator();
      ptr = owner->next_node(ptr);
      return *this;
    }

    iterator operator--(int) {
      iterator tmp(*this);
      --(*this);
      return tmp;
    }

    iterator &operator--() {
      if (owner == NULL || ptr == NULL) throw invalid_iterator();
      ptr = owner->prev_node(ptr);
      return *this;
    }

    value_type &operator*() const {
      if (owner == NULL || ptr == NULL || ptr == owner->header) throw invalid_iterator();
      return *(ptr->data);
    }

    value_type *operator->() const noexcept {
      return &(operator*());
    }

    bool operator==(const iterator &rhs) const { return ptr == rhs.ptr && owner == rhs.owner; }

    bool operator==(const const_iterator &rhs) const;

    bool operator!=(const iterator &rhs) const { return !(*this == rhs); }

    bool operator!=(const const_iterator &rhs) const;
  };

  class const_iterator {
    friend class map;
    friend class iterator;

   private:
    node *ptr;
    const map *owner;

    const_iterator(node *ptr_, const map *owner_) : ptr(ptr_), owner(owner_) {}

   public:
    const_iterator() : ptr(NULL), owner(NULL) {}

    const_iterator(const const_iterator &other) : ptr(other.ptr), owner(other.owner) {}

    const_iterator(const iterator &other) : ptr(other.ptr), owner(other.owner) {}

    const_iterator operator++(int) {
      const_iterator tmp(*this);
      ++(*this);
      return tmp;
    }

    const_iterator &operator++() {
      if (owner == NULL || ptr == NULL) throw invalid_iterator();
      ptr = owner->next_node(ptr);
      return *this;
    }

    const_iterator operator--(int) {
      const_iterator tmp(*this);
      --(*this);
      return tmp;
    }

    const_iterator &operator--() {
      if (owner == NULL || ptr == NULL) throw invalid_iterator();
      ptr = owner->prev_node(ptr);
      return *this;
    }

    const value_type &operator*() const {
      if (owner == NULL || ptr == NULL || ptr == owner->header) throw invalid_iterator();
      return *(ptr->data);
    }

    const value_type *operator->() const noexcept {
      return &(operator*());
    }

    bool operator==(const iterator &rhs) const { return ptr == rhs.ptr && owner == rhs.owner; }

    bool operator==(const const_iterator &rhs) const { return ptr == rhs.ptr && owner == rhs.owner; }

    bool operator!=(const iterator &rhs) const { return !(*this == rhs); }

    bool operator!=(const const_iterator &rhs) const { return !(*this == rhs); }
  };

  map() : header(new node()), node_count(0), comp(Compare()) {
    header->left = header;
    header->right = header;
  }

  map(const map &other) : header(new node()), node_count(other.node_count), comp(other.comp) {
    header->left = header;
    header->right = header;
    header->parent = clone(other.header->parent, header);
    maintain_header();
  }

  map &operator=(const map &other) {
    if (this == &other) return *this;
    clear();
    comp = other.comp;
    header->parent = clone(other.header->parent, header);
    node_count = other.node_count;
    maintain_header();
    return *this;
  }

  ~map() {
    clear();
    delete header;
  }

  T &at(const Key &key) {
    node *target = find_node(key);
    if (target == NULL) throw index_out_of_bound();
    return target->data->second;
  }

  const T &at(const Key &key) const {
    node *target = find_node(key);
    if (target == NULL) throw index_out_of_bound();
    return target->data->second;
  }

  T &operator[](const Key &key) {
    node *cur = header->parent;
    node *parent = header;
    bool go_left = true;
    while (cur != NULL) {
      parent = cur;
      if (comp(key, cur->data->first)) {
        cur = cur->left;
        go_left = true;
      } else if (comp(cur->data->first, key)) {
        cur = cur->right;
        go_left = false;
      } else {
        return cur->data->second;
      }
    }
    value_type value(key, T());
    node *inserted = new node(value, parent);
    if (parent == header) {
      header->parent = inserted;
    } else if (go_left) {
      parent->left = inserted;
    } else {
      parent->right = inserted;
    }
    ++node_count;
    rebalance(inserted);
    return inserted->data->second;
  }

  const T &operator[](const Key &key) const { return at(key); }

  iterator begin() { return iterator(header->left, this); }

  const_iterator cbegin() const { return const_iterator(header->left, this); }

  iterator end() { return iterator(header, this); }

  const_iterator cend() const { return const_iterator(header, this); }

  bool empty() const { return node_count == 0; }

  size_t size() const { return node_count; }

  void clear() {
    destroy(header->parent);
    header->parent = NULL;
    node_count = 0;
    header->left = header;
    header->right = header;
  }

  pair<iterator, bool> insert(const value_type &value) {
    node *cur = header->parent;
    node *parent = header;
    bool go_left = true;
    while (cur != NULL) {
      parent = cur;
      if (comp(value.first, cur->data->first)) {
        cur = cur->left;
        go_left = true;
      } else if (comp(cur->data->first, value.first)) {
        cur = cur->right;
        go_left = false;
      } else {
        return pair<iterator, bool>(iterator(cur, this), false);
      }
    }
    node *inserted = new node(value, parent);
    if (parent == header) {
      header->parent = inserted;
    } else if (go_left) {
      parent->left = inserted;
    } else {
      parent->right = inserted;
    }
    ++node_count;
    rebalance(inserted);
    return pair<iterator, bool>(iterator(inserted, this), true);
  }

  void erase(iterator pos) {
    if (pos.owner != this || pos.ptr == NULL || pos.ptr == header) throw invalid_iterator();
    erase_node(pos.ptr);
  }

  size_t count(const Key &key) const { return find_node(key) == NULL ? 0 : 1; }

  iterator find(const Key &key) {
    node *target = find_node(key);
    if (target == NULL) return end();
    return iterator(target, this);
  }

  const_iterator find(const Key &key) const {
    node *target = find_node(key);
    if (target == NULL) return cend();
    return const_iterator(target, this);
  }
};

template<class Key, class T, class Compare>
bool map<Key, T, Compare>::iterator::operator==(const const_iterator &rhs) const {
  return ptr == rhs.ptr && owner == rhs.owner;
}

template<class Key, class T, class Compare>
bool map<Key, T, Compare>::iterator::operator!=(const const_iterator &rhs) const {
  return !(*this == rhs);
}

}

#endif
