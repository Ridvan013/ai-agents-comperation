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
    class Compare = std::less<Key>
> class map {
   public:
    /**
     * the internal type of data.
     * it should have a default constructor, a copy constructor.
     * You can use sjtu::map as value_type by typedef.
     */
    typedef pair<const Key, T> value_type;

   private:
    /**
     * Node of the AVL tree. Stores the value in place (never default
     * constructs T, and never assigns keys/values) so that key/value
     * types without default or assignment operators still work.
     */
    struct Node {
        value_type data;
        Node *left;
        Node *right;
        Node *parent;
        int height;
        Node(const value_type &d, Node *p)
            : data(d), left(nullptr), right(nullptr), parent(p), height(1) {}
    };

    Node *root;
    size_t sz;
    Compare comp;

    static int nodeHeight(Node *n) { return n ? n->height : 0; }

    static int maxInt(int a, int b) { return a > b ? a : b; }

    static void updateHeight(Node *n) {
        n->height = 1 + maxInt(nodeHeight(n->left), nodeHeight(n->right));
    }

    static int balanceFactor(Node *n) {
        return nodeHeight(n->left) - nodeHeight(n->right);
    }

    // Right rotation around y. Returns the new subtree root and fixes all
    // parent pointers relative to y's original parent.
    static Node *rotateRight(Node *y) {
        Node *x = y->left;
        Node *t = x->right;
        x->right = y;
        y->left = t;
        x->parent = y->parent;
        y->parent = x;
        if (t) t->parent = y;
        updateHeight(y);
        updateHeight(x);
        return x;
    }

    // Left rotation around x. Returns the new subtree root.
    static Node *rotateLeft(Node *x) {
        Node *y = x->right;
        Node *t = y->left;
        y->left = x;
        x->right = t;
        y->parent = x->parent;
        x->parent = y;
        if (t) t->parent = x;
        updateHeight(x);
        updateHeight(y);
        return y;
    }

    // Rebalances a single node and returns the (possibly new) subtree root.
    // The returned node's parent pointer already points to n's old parent,
    // but the parent's child link must be reattached by the caller.
    static Node *balanceNode(Node *n) {
        updateHeight(n);
        int bf = balanceFactor(n);
        if (bf > 1) {
            if (balanceFactor(n->left) < 0) {
                n->left = rotateLeft(n->left);
            }
            return rotateRight(n);
        }
        if (bf < -1) {
            if (balanceFactor(n->right) > 0) {
                n->right = rotateRight(n->right);
            }
            return rotateLeft(n);
        }
        return n;
    }

    // Rebalance the path from n up to the root, reattaching child links.
    void rebalanceUp(Node *n) {
        while (n) {
            Node *p = n->parent;
            Node *nb = balanceNode(n);
            if (p == nullptr) {
                root = nb;
            } else if (p->left == n) {
                p->left = nb;
            } else {
                p->right = nb;
            }
            n = p;
        }
    }

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
        Node *p = n->parent;
        while (p && p->right == n) {
            n = p;
            p = p->parent;
        }
        return p;
    }

    static Node *predecessor(Node *n) {
        if (n->left) return maxNode(n->left);
        Node *p = n->parent;
        while (p && p->left == n) {
            n = p;
            p = p->parent;
        }
        return p;
    }

    Node *findNode(const Key &key) const {
        Node *cur = root;
        while (cur) {
            if (comp(key, cur->data.first)) {
                cur = cur->left;
            } else if (comp(cur->data.first, key)) {
                cur = cur->right;
            } else {
                return cur;
            }
        }
        return nullptr;
    }

    Node *cloneTree(Node *n, Node *parent) {
        if (!n) return nullptr;
        Node *nn = new Node(n->data, parent);
        nn->height = n->height;
        nn->left = cloneTree(n->left, nn);
        nn->right = cloneTree(n->right, nn);
        return nn;
    }

    void destroyTree(Node *n) {
        if (!n) return;
        destroyTree(n->left);
        destroyTree(n->right);
        delete n;
    }

    // Replaces the subtree rooted at u with the subtree v in u's parent link.
    void transplant(Node *u, Node *v) {
        if (u->parent == nullptr) {
            root = v;
        } else if (u->parent->left == u) {
            u->parent->left = v;
        } else {
            u->parent->right = v;
        }
        if (v) v->parent = u->parent;
    }

    // Removes node z from the tree (relinking nodes, never copying data) and
    // rebalances. Assumes z belongs to this tree.
    void eraseNode(Node *z) {
        Node *rebalanceFrom;
        if (z->left && z->right) {
            Node *y = minNode(z->right);  // successor, has no left child
            if (y->parent == z) {
                rebalanceFrom = y;
            } else {
                rebalanceFrom = y->parent;
                transplant(y, y->right);
                y->right = z->right;
                if (y->right) y->right->parent = y;
            }
            transplant(z, y);
            y->left = z->left;
            if (y->left) y->left->parent = y;
            y->height = z->height;
        } else {
            Node *child = z->left ? z->left : z->right;
            rebalanceFrom = z->parent;
            transplant(z, child);
        }
        delete z;
        --sz;
        rebalanceUp(rebalanceFrom);
    }

   public:
    /**
     * see BidirectionalIterator at CppReference for help.
     *
     * if there is anything wrong throw invalid_iterator.
     *     like it = map.begin(); --it;
     *       or it = map.end(); ++end();
     */
    class const_iterator;
    class iterator {
        friend class map;
        friend class const_iterator;

       private:
        const map *mp;
        Node *nd;

        iterator(const map *m, Node *n) : mp(m), nd(n) {}

       public:
        iterator() : mp(nullptr), nd(nullptr) {}

        iterator(const iterator &other) : mp(other.mp), nd(other.nd) {}

        /**
         * iter++
         */
        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        /**
         * ++iter
         */
        iterator &operator++() {
            if (nd == nullptr) throw invalid_iterator();
            nd = map::successor(nd);
            return *this;
        }

        /**
         * iter--
         */
        iterator operator--(int) {
            iterator tmp = *this;
            --(*this);
            return tmp;
        }

        /**
         * --iter
         */
        iterator &operator--() {
            if (nd == nullptr) {
                if (mp == nullptr || mp->root == nullptr) throw invalid_iterator();
                nd = map::maxNode(mp->root);
                return *this;
            }
            Node *p = map::predecessor(nd);
            if (p == nullptr) throw invalid_iterator();
            nd = p;
            return *this;
        }

        /**
         * a operator to check whether two iterators are same (pointing to the same memory).
         */
        value_type &operator*() const { return nd->data; }

        bool operator==(const iterator &rhs) const {
            return mp == rhs.mp && nd == rhs.nd;
        }

        bool operator==(const const_iterator &rhs) const {
            return mp == rhs.mp && nd == rhs.nd;
        }

        /**
         * some other operator for iterator.
         */
        bool operator!=(const iterator &rhs) const {
            return !(*this == rhs);
        }

        bool operator!=(const const_iterator &rhs) const {
            return !(*this == rhs);
        }

        /**
         * for the support of it->first.
         */
        value_type *operator->() const noexcept { return &(nd->data); }
    };

    class const_iterator {
        friend class map;
        friend class iterator;

       private:
        const map *mp;
        Node *nd;

        const_iterator(const map *m, Node *n) : mp(m), nd(n) {}

       public:
        const_iterator() : mp(nullptr), nd(nullptr) {}

        const_iterator(const const_iterator &other)
            : mp(other.mp), nd(other.nd) {}

        const_iterator(const iterator &other) : mp(other.mp), nd(other.nd) {}

        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        const_iterator &operator++() {
            if (nd == nullptr) throw invalid_iterator();
            nd = map::successor(nd);
            return *this;
        }

        const_iterator operator--(int) {
            const_iterator tmp = *this;
            --(*this);
            return tmp;
        }

        const_iterator &operator--() {
            if (nd == nullptr) {
                if (mp == nullptr || mp->root == nullptr) throw invalid_iterator();
                nd = map::maxNode(mp->root);
                return *this;
            }
            Node *p = map::predecessor(nd);
            if (p == nullptr) throw invalid_iterator();
            nd = p;
            return *this;
        }

        const value_type &operator*() const { return nd->data; }

        bool operator==(const iterator &rhs) const {
            return mp == rhs.mp && nd == rhs.nd;
        }

        bool operator==(const const_iterator &rhs) const {
            return mp == rhs.mp && nd == rhs.nd;
        }

        bool operator!=(const iterator &rhs) const {
            return !(*this == rhs);
        }

        bool operator!=(const const_iterator &rhs) const {
            return !(*this == rhs);
        }

        const value_type *operator->() const noexcept { return &(nd->data); }
    };

    /**
     * two constructors
     */
    map() : root(nullptr), sz(0), comp() {}

    map(const map &other) : root(nullptr), sz(other.sz), comp(other.comp) {
        root = cloneTree(other.root, nullptr);
    }

    /**
     * assignment operator
     */
    map &operator=(const map &other) {
        if (this == &other) return *this;
        destroyTree(root);
        root = nullptr;
        sz = 0;
        comp = other.comp;
        root = cloneTree(other.root, nullptr);
        sz = other.sz;
        return *this;
    }

    /**
     * Destructors
     */
    ~map() { destroyTree(root); }

    /**
     * access specified element with bounds checking
     * Returns a reference to the mapped value of the element with key
     * equivalent to key. If no such element exists, an exception of type
     * `index_out_of_bound` is thrown.
     */
    T &at(const Key &key) {
        Node *n = findNode(key);
        if (!n) throw index_out_of_bound();
        return n->data.second;
    }

    const T &at(const Key &key) const {
        Node *n = findNode(key);
        if (!n) throw index_out_of_bound();
        return n->data.second;
    }

    /**
     * access specified element
     * Returns a reference to the value that is mapped to a key equivalent to
     * key, performing an insertion if such key does not already exist.
     */
    T &operator[](const Key &key) {
        Node *n = findNode(key);
        if (n) return n->data.second;
        pair<iterator, bool> res = insert(value_type(key, T()));
        return res.first.nd->data.second;
    }

    /**
     * behave like at() throw index_out_of_bound if such key does not exist.
     */
    const T &operator[](const Key &key) const {
        Node *n = findNode(key);
        if (!n) throw index_out_of_bound();
        return n->data.second;
    }

    /**
     * return a iterator to the beginning
     */
    iterator begin() { return iterator(this, minNode(root)); }

    const_iterator cbegin() const { return const_iterator(this, minNode(root)); }

    /**
     * return a iterator to the end
     * in fact, it returns past-the-end.
     */
    iterator end() { return iterator(this, nullptr); }

    const_iterator cend() const { return const_iterator(this, nullptr); }

    /**
     * checks whether the container is empty
     * return true if empty, otherwise false.
     */
    bool empty() const { return sz == 0; }

    /**
     * returns the number of elements.
     */
    size_t size() const { return sz; }

    /**
     * clears the contents
     */
    void clear() {
        destroyTree(root);
        root = nullptr;
        sz = 0;
    }

    /**
     * insert an element.
     * return a pair, the first of the pair is
     *   the iterator to the new element (or the element that prevented the
     *   insertion), the second one is true if insert successfully, or false.
     */
    pair<iterator, bool> insert(const value_type &value) {
        if (root == nullptr) {
            root = new Node(value, nullptr);
            sz = 1;
            return pair<iterator, bool>(iterator(this, root), true);
        }
        Node *cur = root;
        Node *parent = nullptr;
        bool goLeft = false;
        while (cur) {
            parent = cur;
            if (comp(value.first, cur->data.first)) {
                cur = cur->left;
                goLeft = true;
            } else if (comp(cur->data.first, value.first)) {
                cur = cur->right;
                goLeft = false;
            } else {
                return pair<iterator, bool>(iterator(this, cur), false);
            }
        }
        Node *nn = new Node(value, parent);
        if (goLeft) {
            parent->left = nn;
        } else {
            parent->right = nn;
        }
        ++sz;
        rebalanceUp(parent);
        return pair<iterator, bool>(iterator(this, nn), true);
    }

    /**
     * erase the element at pos.
     *
     * throw if pos pointed to a bad element
     * (pos == this->end() || pos points an element out of this)
     */
    void erase(iterator pos) {
        if (pos.mp != this || pos.nd == nullptr) throw invalid_iterator();
        eraseNode(pos.nd);
    }

    /**
     * Returns the number of elements with key that compares equivalent to the
     * specified argument, which is either 1 or 0 since this container does not
     * allow duplicates.
     */
    size_t count(const Key &key) const {
        return findNode(key) ? 1 : 0;
    }

    /**
     * Finds an element with key equivalent to key.
     */
    iterator find(const Key &key) {
        return iterator(this, findNode(key));
    }

    const_iterator find(const Key &key) const {
        return const_iterator(this, findNode(key));
    }
};

}

#endif
