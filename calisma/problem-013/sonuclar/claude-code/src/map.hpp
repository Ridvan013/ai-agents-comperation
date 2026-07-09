/**
* implement a container like std::map
*/
#ifndef SJTU_MAP_HPP
#define SJTU_MAP_HPP

// only for std::less<T>
#include <functional>
#include <cstddef>
#include "utility.hpp"
#include "exceptions.hpp"

namespace sjtu {

template<
   class Key,
   class T,
   class Compare = std::less <Key>
   > class map {
  public:
   typedef pair<const Key, T> value_type;

  private:
   /**
    * Node of the AVL tree.
    * Stores the value directly (so we never require a default constructor
    * for Key or T). Parent pointers make iterator ++/-- traversal simple and
    * keep every node's address stable across rotations, which is required for
    * the "erase does not invalidate other iterators" guarantee.
    */
   struct Node {
       value_type value;
       Node *left, *right, *parent;
       int height;
       Node(const value_type &v, Node *p = nullptr)
           : value(v), left(nullptr), right(nullptr), parent(p), height(1) {}
   };

   Node *root;
   size_t sz;
   Compare comp;

   /* -------------------- small helpers -------------------- */
   static int nodeHeight(Node *n) { return n ? n->height : 0; }

   static void updateHeight(Node *n) {
       int hl = nodeHeight(n->left), hr = nodeHeight(n->right);
       n->height = (hl > hr ? hl : hr) + 1;
   }

   static int balanceFactor(Node *n) {
       return nodeHeight(n->left) - nodeHeight(n->right);
   }

   /* rotations return the new subtree root; caller fixes the root's parent */
   static Node *rotateRight(Node *y) {
       Node *x = y->left;
       y->left = x->right;
       if (x->right) x->right->parent = y;
       x->right = y;
       y->parent = x;
       updateHeight(y);
       updateHeight(x);
       return x;
   }

   static Node *rotateLeft(Node *x) {
       Node *y = x->right;
       x->right = y->left;
       if (y->left) y->left->parent = x;
       y->left = x;
       x->parent = y;
       updateHeight(x);
       updateHeight(y);
       return y;
   }

   /* rebalance node n, return new subtree root (parent fixed by caller) */
   static Node *balance(Node *n) {
       updateHeight(n);
       int bf = balanceFactor(n);
       if (bf > 1) {
           if (balanceFactor(n->left) < 0) {
               n->left = rotateLeft(n->left);
               n->left->parent = n;
           }
           return rotateRight(n);
       }
       if (bf < -1) {
           if (balanceFactor(n->right) > 0) {
               n->right = rotateRight(n->right);
               n->right->parent = n;
           }
           return rotateLeft(n);
       }
       return n;
   }

   /* -------------------- tree navigation -------------------- */
   static Node *minNode(Node *n) {
       if (!n) return nullptr;
       while (n->left) n = n->left;
       return n;
   }
   static Node *maxNode(Node *n) {
       if (!n) return nullptr;
       while (n->right) n = n->right;
       return n;
   }
   static Node *successor(Node *n) {
       if (n->right) return minNode(n->right);
       while (n->parent && n->parent->right == n) n = n->parent;
       return n->parent;
   }
   static Node *predecessor(Node *n) {
       if (n->left) return maxNode(n->left);
       while (n->parent && n->parent->left == n) n = n->parent;
       return n->parent;
   }

   Node *findNode(const Key &key) const {
       Node *cur = root;
       while (cur) {
           if (comp(key, cur->value.first))
               cur = cur->left;
           else if (comp(cur->value.first, key))
               cur = cur->right;
           else
               return cur;
       }
       return nullptr;
   }

   /* -------------------- insert -------------------- */
   Node *insertRec(Node *cur, Node *parent, const value_type &v,
                   Node *&resultNode, bool &inserted) {
       if (!cur) {
           Node *n = new Node(v, parent);
           resultNode = n;
           inserted = true;
           return n;
       }
       if (comp(v.first, cur->value.first)) {
           cur->left = insertRec(cur->left, cur, v, resultNode, inserted);
           cur->left->parent = cur;
       } else if (comp(cur->value.first, v.first)) {
           cur->right = insertRec(cur->right, cur, v, resultNode, inserted);
           cur->right->parent = cur;
       } else {
           resultNode = cur;
           inserted = false;
           return cur;
       }
       return balance(cur);
   }

   pair<Node *, bool> insertNode(const value_type &v) {
       Node *resultNode = nullptr;
       bool inserted = false;
       root = insertRec(root, nullptr, v, resultNode, inserted);
       root->parent = nullptr;
       if (inserted) ++sz;
       return pair<Node *, bool>(resultNode, inserted);
   }

   /* -------------------- erase -------------------- */
   /* remove the minimum node of the subtree WITHOUT deleting it,
      hand the extracted node back through `out`. */
   Node *extractMin(Node *cur, Node *parent, Node *&out) {
       if (!cur->left) {
           out = cur;
           Node *r = cur->right;
           if (r) r->parent = parent;
           return r;
       }
       cur->left = extractMin(cur->left, cur, out);
       if (cur->left) cur->left->parent = cur;
       return balance(cur);
   }

   Node *eraseRec(Node *cur, Node *parent, const Key &key) {
       if (comp(key, cur->value.first)) {
           cur->left = eraseRec(cur->left, cur, key);
           if (cur->left) cur->left->parent = cur;
       } else if (comp(cur->value.first, key)) {
           cur->right = eraseRec(cur->right, cur, key);
           if (cur->right) cur->right->parent = cur;
       } else {
           if (!cur->left || !cur->right) {
               Node *child = cur->left ? cur->left : cur->right;
               if (child) child->parent = parent;
               delete cur;
               return child;
           }
           // two children: splice the successor node into cur's place
           Node *succ = nullptr;
           Node *newRight = extractMin(cur->right, cur, succ);
           succ->left = cur->left;
           if (cur->left) cur->left->parent = succ;
           succ->right = newRight;
           if (newRight) newRight->parent = succ;
           succ->parent = parent;
           delete cur;
           return balance(succ);
       }
       return balance(cur);
   }

   void eraseNode(Node *n) {
       root = eraseRec(root, nullptr, n->value.first);
       if (root) root->parent = nullptr;
       --sz;
   }

   /* -------------------- copy / clear -------------------- */
   static Node *clone(Node *src, Node *parent) {
       if (!src) return nullptr;
       Node *n = new Node(src->value, parent);
       n->height = src->height;
       n->left = clone(src->left, n);
       n->right = clone(src->right, n);
       return n;
   }

   static void destroy(Node *n) {
       if (!n) return;
       destroy(n->left);
       destroy(n->right);
       delete n;
   }

  public:
   class const_iterator;
   class iterator {
       friend class map;
       friend class const_iterator;
      private:
       const map *mp;
       Node *node;   // nullptr represents end()
       iterator(const map *m, Node *n) : mp(m), node(n) {}
      public:
       iterator() : mp(nullptr), node(nullptr) {}
       iterator(const iterator &other) : mp(other.mp), node(other.node) {}

       iterator operator++(int) {
           iterator tmp = *this;
           ++(*this);
           return tmp;
       }

       iterator &operator++() {
           if (node == nullptr) throw invalid_iterator();
           node = successor(node);
           return *this;
       }

       iterator operator--(int) {
           iterator tmp = *this;
           --(*this);
           return tmp;
       }

       iterator &operator--() {
           if (node == nullptr) {
               Node *m = mp ? maxNode(mp->root) : nullptr;
               if (!m) throw invalid_iterator();
               node = m;
               return *this;
           }
           Node *p = predecessor(node);
           if (!p) throw invalid_iterator();
           node = p;
           return *this;
       }

       value_type &operator*() const { return node->value; }

       bool operator==(const iterator &rhs) const {
           return mp == rhs.mp && node == rhs.node;
       }
       bool operator==(const const_iterator &rhs) const {
           return mp == rhs.mp && node == rhs.node;
       }
       bool operator!=(const iterator &rhs) const { return !(*this == rhs); }
       bool operator!=(const const_iterator &rhs) const { return !(*this == rhs); }

       value_type *operator->() const noexcept { return &(node->value); }
   };

   class const_iterator {
       friend class map;
       friend class iterator;
      private:
       const map *mp;
       Node *node;
       const_iterator(const map *m, Node *n) : mp(m), node(n) {}
      public:
       const_iterator() : mp(nullptr), node(nullptr) {}
       const_iterator(const const_iterator &other)
           : mp(other.mp), node(other.node) {}
       const_iterator(const iterator &other)
           : mp(other.mp), node(other.node) {}

       const_iterator operator++(int) {
           const_iterator tmp = *this;
           ++(*this);
           return tmp;
       }
       const_iterator &operator++() {
           if (node == nullptr) throw invalid_iterator();
           node = successor(node);
           return *this;
       }
       const_iterator operator--(int) {
           const_iterator tmp = *this;
           --(*this);
           return tmp;
       }
       const_iterator &operator--() {
           if (node == nullptr) {
               Node *m = mp ? maxNode(mp->root) : nullptr;
               if (!m) throw invalid_iterator();
               node = m;
               return *this;
           }
           Node *p = predecessor(node);
           if (!p) throw invalid_iterator();
           node = p;
           return *this;
       }

       const value_type &operator*() const { return node->value; }

       bool operator==(const iterator &rhs) const {
           return mp == rhs.mp && node == rhs.node;
       }
       bool operator==(const const_iterator &rhs) const {
           return mp == rhs.mp && node == rhs.node;
       }
       bool operator!=(const iterator &rhs) const { return !(*this == rhs); }
       bool operator!=(const const_iterator &rhs) const { return !(*this == rhs); }

       const value_type *operator->() const noexcept { return &(node->value); }
   };

   /* -------------------- constructors / assignment / dtor -------------------- */
   map() : root(nullptr), sz(0) {}

   map(const map &other) : root(nullptr), sz(other.sz), comp(other.comp) {
       root = clone(other.root, nullptr);
   }

   map &operator=(const map &other) {
       if (this == &other) return *this;
       destroy(root);
       comp = other.comp;
       root = clone(other.root, nullptr);
       sz = other.sz;
       return *this;
   }

   ~map() { destroy(root); }

   /* -------------------- element access -------------------- */
   T &at(const Key &key) {
       Node *n = findNode(key);
       if (!n) throw index_out_of_bound();
       return n->value.second;
   }

   const T &at(const Key &key) const {
       Node *n = findNode(key);
       if (!n) throw index_out_of_bound();
       return n->value.second;
   }

   T &operator[](const Key &key) {
       Node *n = findNode(key);
       if (n) return n->value.second;
       pair<Node *, bool> res = insertNode(value_type(key, T()));
       return res.first->value.second;
   }

   const T &operator[](const Key &key) const {
       Node *n = findNode(key);
       if (!n) throw index_out_of_bound();
       return n->value.second;
   }

   /* -------------------- iterators -------------------- */
   iterator begin() { return iterator(this, minNode(root)); }
   const_iterator cbegin() const { return const_iterator(this, minNode(root)); }
   iterator end() { return iterator(this, nullptr); }
   const_iterator cend() const { return const_iterator(this, nullptr); }

   /* -------------------- capacity -------------------- */
   bool empty() const { return sz == 0; }
   size_t size() const { return sz; }

   void clear() {
       destroy(root);
       root = nullptr;
       sz = 0;
   }

   /* -------------------- modifiers -------------------- */
   pair<iterator, bool> insert(const value_type &value) {
       pair<Node *, bool> res = insertNode(value);
       return pair<iterator, bool>(iterator(this, res.first), res.second);
   }

   void erase(iterator pos) {
       if (pos.mp != this || pos.node == nullptr) throw invalid_iterator();
       eraseNode(pos.node);
   }

   /* -------------------- lookup -------------------- */
   size_t count(const Key &key) const { return findNode(key) ? 1 : 0; }

   iterator find(const Key &key) { return iterator(this, findNode(key)); }
   const_iterator find(const Key &key) const {
       return const_iterator(this, findNode(key));
   }
};

}

#endif
