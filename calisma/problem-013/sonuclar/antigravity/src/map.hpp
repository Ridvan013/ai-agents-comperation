#ifndef SJTU_MAP_HPP
#define SJTU_MAP_HPP

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
    typedef pair<const Key, T> value_type;

private:
    struct Node {
        value_type data;
        Node *left;
        Node *right;
        Node *parent;
        int height;

        Node(const value_type &v, Node *p = nullptr) 
            : data(v), left(nullptr), right(nullptr), parent(p), height(1) {}
    };

    Node *root;
    size_t tree_size;

    bool is_less(const Key &a, const Key &b) const {
        Compare cmp;
        return cmp(a, b);
    }

    int height(Node *n) const { return n ? n->height : 0; }

    void update_height(Node *n) {
        if (n) {
            int hl = height(n->left);
            int hr = height(n->right);
            n->height = (hl > hr ? hl : hr) + 1;
        }
    }

    int balance_factor(Node *n) const {
        return n ? height(n->left) - height(n->right) : 0;
    }

    Node* rotate_right(Node *y) {
        Node *x = y->left;
        y->left = x->right;
        if (x->right) x->right->parent = y;
        x->parent = y->parent;
        x->right = y;
        y->parent = x;
        update_height(y);
        update_height(x);
        return x;
    }

    Node* rotate_left(Node *x) {
        Node *y = x->right;
        x->right = y->left;
        if (y->left) y->left->parent = x;
        y->parent = x->parent;
        y->left = x;
        x->parent = y;
        update_height(x);
        update_height(y);
        return y;
    }

    Node* balance(Node *n) {
        update_height(n);
        int bf = balance_factor(n);
        if (bf > 1) {
            if (balance_factor(n->left) < 0) {
                n->left = rotate_left(n->left);
                if (n->left) n->left->parent = n;
            }
            return rotate_right(n);
        }
        if (bf < -1) {
            if (balance_factor(n->right) > 0) {
                n->right = rotate_right(n->right);
                if (n->right) n->right->parent = n;
            }
            return rotate_left(n);
        }
        return n;
    }

    Node* insert_node(Node *node, const value_type &val, Node *parent, bool &inserted, Node *&inserted_node) {
        if (!node) {
            inserted = true;
            inserted_node = new Node(val, parent);
            return inserted_node;
        }
        if (is_less(val.first, node->data.first)) {
            node->left = insert_node(node->left, val, node, inserted, inserted_node);
            if (node->left) node->left->parent = node;
        } else if (is_less(node->data.first, val.first)) {
            node->right = insert_node(node->right, val, node, inserted, inserted_node);
            if (node->right) node->right->parent = node;
        } else {
            inserted = false;
            inserted_node = node;
            return node;
        }
        return balance(node);
    }

    Node* extract_min(Node *node, Node *&min_node) {
        if (!node->left) {
            min_node = node;
            return node->right;
        }
        node->left = extract_min(node->left, min_node);
        if (node->left) node->left->parent = node;
        return balance(node);
    }

    Node* erase_node(Node *node, const Key &key, bool &erased) {
        if (!node) {
            erased = false;
            return nullptr;
        }
        if (is_less(key, node->data.first)) {
            node->left = erase_node(node->left, key, erased);
            if (node->left) node->left->parent = node;
        } else if (is_less(node->data.first, key)) {
            node->right = erase_node(node->right, key, erased);
            if (node->right) node->right->parent = node;
        } else {
            erased = true;
            if (!node->left || !node->right) {
                Node *temp = node->left ? node->left : node->right;
                if (temp) temp->parent = node->parent;
                delete node;
                return temp;
            } else {
                Node *temp = nullptr;
                node->right = extract_min(node->right, temp);
                
                temp->left = node->left;
                if (temp->left) temp->left->parent = temp;
                
                temp->right = node->right;
                if (temp->right) temp->right->parent = temp;
                
                temp->parent = node->parent;
                
                delete node;
                return balance(temp);
            }
        }
        return balance(node);
    }

    Node* find_node(Node *node, const Key &key) const {
        while (node) {
            if (is_less(key, node->data.first)) {
                node = node->left;
            } else if (is_less(node->data.first, key)) {
                node = node->right;
            } else {
                return node;
            }
        }
        return nullptr;
    }

    void clear_tree(Node *node) {
        if (node) {
            clear_tree(node->left);
            clear_tree(node->right);
            delete node;
        }
    }

    Node* copy_tree(Node *node, Node *parent) {
        if (!node) return nullptr;
        Node *new_node = new Node(node->data, parent);
        new_node->height = node->height;
        new_node->left = copy_tree(node->left, new_node);
        new_node->right = copy_tree(node->right, new_node);
        return new_node;
    }

public:
    class const_iterator;
    class iterator {
        friend class map;
        friend class const_iterator;
    private:
        map *m;
        Node *ptr;
    public:
        iterator(map *m_ = nullptr, Node *ptr_ = nullptr) : m(m_), ptr(ptr_) {}
        iterator(const iterator &other) : m(other.m), ptr(other.ptr) {}

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        iterator &operator++() {
            if (!ptr) throw invalid_iterator();
            if (ptr->right) {
                ptr = ptr->right;
                while (ptr->left) ptr = ptr->left;
            } else {
                Node *p = ptr->parent;
                while (p && p->right == ptr) {
                    ptr = p;
                    p = p->parent;
                }
                ptr = p;
            }
            return *this;
        }

        iterator operator--(int) {
            iterator tmp = *this;
            --(*this);
            return tmp;
        }

        iterator &operator--() {
            if (!ptr) {
                if (!m || !m->root) throw invalid_iterator();
                ptr = m->root;
                while (ptr->right) ptr = ptr->right;
            } else {
                if (ptr->left) {
                    ptr = ptr->left;
                    while (ptr->right) ptr = ptr->right;
                } else {
                    Node *p = ptr->parent;
                    while (p && p->left == ptr) {
                        ptr = p;
                        p = p->parent;
                    }
                    if (!p) throw invalid_iterator();
                    ptr = p;
                }
            }
            return *this;
        }

        value_type &operator*() const {
            if (!ptr) throw invalid_iterator();
            return ptr->data;
        }

        bool operator==(const iterator &rhs) const {
            return ptr == rhs.ptr && m == rhs.m;
        }
        bool operator==(const const_iterator &rhs) const {
            return ptr == rhs.ptr && m == rhs.m;
        }
        bool operator!=(const iterator &rhs) const {
            return !(*this == rhs);
        }
        bool operator!=(const const_iterator &rhs) const {
            return !(*this == rhs);
        }

        value_type *operator->() const noexcept {
            if (!ptr) return nullptr;
            return &(ptr->data);
        }
    };

    class const_iterator {
        friend class map;
        friend class iterator;
    private:
        const map *m;
        Node *ptr;
    public:
        const_iterator(const map *m_ = nullptr, Node *ptr_ = nullptr) : m(m_), ptr(ptr_) {}
        const_iterator(const const_iterator &other) : m(other.m), ptr(other.ptr) {}
        const_iterator(const iterator &other) : m(other.m), ptr(other.ptr) {}

        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        const_iterator &operator++() {
            if (!ptr) throw invalid_iterator();
            if (ptr->right) {
                ptr = ptr->right;
                while (ptr->left) ptr = ptr->left;
            } else {
                Node *p = ptr->parent;
                while (p && p->right == ptr) {
                    ptr = p;
                    p = p->parent;
                }
                ptr = p;
            }
            return *this;
        }

        const_iterator operator--(int) {
            const_iterator tmp = *this;
            --(*this);
            return tmp;
        }

        const_iterator &operator--() {
            if (!ptr) {
                if (!m || !m->root) throw invalid_iterator();
                ptr = m->root;
                while (ptr->right) ptr = ptr->right;
            } else {
                if (ptr->left) {
                    ptr = ptr->left;
                    while (ptr->right) ptr = ptr->right;
                } else {
                    Node *p = ptr->parent;
                    while (p && p->left == ptr) {
                        ptr = p;
                        p = p->parent;
                    }
                    if (!p) throw invalid_iterator();
                    ptr = p;
                }
            }
            return *this;
        }

        const value_type &operator*() const {
            if (!ptr) throw invalid_iterator();
            return ptr->data;
        }

        bool operator==(const iterator &rhs) const {
            return ptr == rhs.ptr && m == rhs.m;
        }
        bool operator==(const const_iterator &rhs) const {
            return ptr == rhs.ptr && m == rhs.m;
        }
        bool operator!=(const iterator &rhs) const {
            return !(*this == rhs);
        }
        bool operator!=(const const_iterator &rhs) const {
            return !(*this == rhs);
        }

        const value_type *operator->() const noexcept {
            if (!ptr) return nullptr;
            return &(ptr->data);
        }
    };

    map() : root(nullptr), tree_size(0) {}

    map(const map &other) : root(nullptr), tree_size(other.tree_size) {
        root = copy_tree(other.root, nullptr);
    }

    map &operator=(const map &other) {
        if (this == &other) return *this;
        clear();
        root = copy_tree(other.root, nullptr);
        tree_size = other.tree_size;
        return *this;
    }

    ~map() {
        clear();
    }

    T &at(const Key &key) {
        Node *n = find_node(root, key);
        if (!n) throw index_out_of_bound();
        return n->data.second;
    }

    const T &at(const Key &key) const {
        Node *n = find_node(root, key);
        if (!n) throw index_out_of_bound();
        return n->data.second;
    }

    T &operator[](const Key &key) {
        Node *n = find_node(root, key);
        if (n) return n->data.second;
        bool inserted = false;
        Node *inserted_node = nullptr;
        root = insert_node(root, value_type(key, T()), nullptr, inserted, inserted_node);
        if (root) root->parent = nullptr;
        tree_size++;
        return inserted_node->data.second;
    }

    const T &operator[](const Key &key) const {
        Node *n = find_node(root, key);
        if (!n) throw index_out_of_bound();
        return n->data.second;
    }

    iterator begin() {
        if (!root) return iterator(this, nullptr);
        Node *n = root;
        while (n->left) n = n->left;
        return iterator(this, n);
    }
    
    const_iterator begin() const {
        return cbegin();
    }

    const_iterator cbegin() const {
        if (!root) return const_iterator(this, nullptr);
        Node *n = root;
        while (n->left) n = n->left;
        return const_iterator(this, n);
    }

    iterator end() {
        return iterator(this, nullptr);
    }
    
    const_iterator end() const {
        return cend();
    }

    const_iterator cend() const {
        return const_iterator(this, nullptr);
    }

    bool empty() const {
        return tree_size == 0;
    }

    size_t size() const {
        return tree_size;
    }

    void clear() {
        clear_tree(root);
        root = nullptr;
        tree_size = 0;
    }

    pair<iterator, bool> insert(const value_type &value) {
        bool inserted = false;
        Node *inserted_node = nullptr;
        root = insert_node(root, value, nullptr, inserted, inserted_node);
        if (root) root->parent = nullptr;
        if (inserted) tree_size++;
        return pair<iterator, bool>(iterator(this, inserted_node), inserted);
    }

    void erase(iterator pos) {
        if (pos.m != this || pos.ptr == nullptr) throw invalid_iterator();
        bool erased = false;
        root = erase_node(root, pos.ptr->data.first, erased);
        if (root) root->parent = nullptr;
        if (erased) tree_size--;
    }

    size_t count(const Key &key) const {
        return find_node(root, key) ? 1 : 0;
    }

    iterator find(const Key &key) {
        Node *n = find_node(root, key);
        return iterator(this, n);
    }

    const_iterator find(const Key &key) const {
        Node *n = find_node(root, key);
        return const_iterator(this, n);
    }
};

}

#endif