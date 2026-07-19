#ifndef BPT_HPP
#define BPT_HPP

#include <iostream>
#include <fstream>
#include <cstring>
#include <string>

#pragma pack(push, 1)

struct Key {
    char k[65];
    int v;

    Key() {
        memset(k, 0, sizeof(k));
        v = 0;
    }

    Key(const char* str, int val) {
        memset(k, 0, sizeof(k));
        strncpy(k, str, 64);
        v = val;
    }

    bool operator<(const Key& o) const {
        int cmp = strcmp(k, o.k);
        if (cmp != 0) return cmp < 0;
        return v < o.v;
    }

    bool operator==(const Key& o) const {
        return strcmp(k, o.k) == 0 && v == o.v;
    }
};

const int M = 50; 

struct Node {
    int id;
    int parent;
    int next; 
    bool is_leaf;
    int count;
    Key keys[M];
    int children[M]; 

    Node() {
        id = -1;
        parent = -1;
        next = -1;
        is_leaf = true;
        count = 0;
        memset(children, -1, sizeof(children));
    }
};

#pragma pack(pop)

class BPlusTree {
private:
    std::fstream fs;
    std::string file_name;
    int root_id;
    int next_node_id;

    void read_node(int id, Node& node) {
        fs.seekg(8 + id * sizeof(Node));
        fs.read((char*)&node, sizeof(Node));
    }

    void write_node(int id, const Node& node) {
        fs.seekp(8 + id * sizeof(Node));
        fs.write((const char*)&node, sizeof(Node));
    }

    void read_meta() {
        fs.seekg(0);
        fs.read((char*)&root_id, sizeof(int));
        fs.read((char*)&next_node_id, sizeof(int));
    }

    void write_meta() {
        fs.seekp(0);
        fs.write((char*)&root_id, sizeof(int));
        fs.write((char*)&next_node_id, sizeof(int));
    }

    int alloc_node() {
        int id = next_node_id++;
        write_meta();
        return id;
    }

    void split_node(Node& parent, int child_idx, Node& child) {
        Node sibling;
        sibling.id = alloc_node();
        sibling.is_leaf = child.is_leaf;
        sibling.parent = child.parent;

        int mid = child.count / 2;
        sibling.count = child.count - mid;
        for (int i = 0; i < sibling.count; ++i) {
            sibling.keys[i] = child.keys[mid + i];
            sibling.children[i] = child.children[mid + i];
        }
        child.count = mid;

        if (child.is_leaf) {
            sibling.next = child.next;
            child.next = sibling.id;
        } else {
            for (int i = 0; i < sibling.count; ++i) {
                if (sibling.children[i] != -1) {
                    Node c;
                    read_node(sibling.children[i], c);
                    c.parent = sibling.id;
                    write_node(c.id, c);
                }
            }
        }

        for (int i = parent.count; i > child_idx; --i) {
            parent.keys[i] = parent.keys[i - 1];
            parent.children[i] = parent.children[i - 1];
        }
        
        parent.keys[child_idx + 1] = sibling.keys[0];
        parent.children[child_idx + 1] = sibling.id;
        parent.count++;

        write_node(child.id, child);
        write_node(sibling.id, sibling);
        write_node(parent.id, parent);
    }

    void insert_non_full(Node& node, const Key& key, int val) {
        if (node.is_leaf) {
            int i = node.count - 1;
            while (i >= 0 && key < node.keys[i]) {
                node.keys[i + 1] = node.keys[i];
                node.children[i + 1] = node.children[i];
                i--;
            }
            node.keys[i + 1] = key;
            node.children[i + 1] = val;
            node.count++;
            write_node(node.id, node);
        } else {
            int i = 0;
            while (i < node.count - 1 && !(key < node.keys[i + 1])) {
                i++;
            }
            Node child;
            read_node(node.children[i], child);
            if (child.count == M) {
                split_node(node, i, child);
                if (!(key < node.keys[i + 1])) {
                    i++;
                }
                read_node(node.children[i], child);
            }
            insert_non_full(child, key, val);
            
            // update keys in inner node
            node.keys[i] = child.keys[0];
            write_node(node.id, node);
        }
    }

public:
    BPlusTree(const std::string& name) : file_name(name) {
        fs.open(file_name, std::ios::in | std::ios::out | std::ios::binary);
        if (!fs) {
            fs.clear();
            fs.open(file_name, std::ios::out | std::ios::binary);
            root_id = 0;
            next_node_id = 1;
            write_meta();
            
            Node root;
            root.id = 0;
            write_node(root.id, root);
            fs.close();
            fs.open(file_name, std::ios::in | std::ios::out | std::ios::binary);
        } else {
            read_meta();
        }
    }

    ~BPlusTree() {
        if (fs.is_open()) fs.close();
    }

    void insert(const char* k, int v) {
        Key key(k, v);
        Node root;
        read_node(root_id, root);
        if (root.count == M) {
            Node new_root;
            new_root.id = alloc_node();
            new_root.is_leaf = false;
            new_root.children[0] = root.id;
            new_root.keys[0] = root.keys[0];
            new_root.count = 1;
            
            root.parent = new_root.id;
            write_node(root.id, root);
            
            split_node(new_root, 0, root);
            root_id = new_root.id;
            write_meta();
            insert_non_full(new_root, key, v);
        } else {
            insert_non_full(root, key, v);
        }
    }

    void remove(const char* k, int v) {
        Key key(k, v);
        Node curr;
        read_node(root_id, curr);
        
        while (!curr.is_leaf) {
            int i = 0;
            while (i < curr.count - 1 && !(key < curr.keys[i + 1])) i++;
            int nxt = curr.children[i];
            read_node(nxt, curr);
        }
        
        int i = 0;
        while (i < curr.count && curr.keys[i] < key) i++;
        if (i < curr.count && curr.keys[i] == key) {
            for (int j = i; j < curr.count - 1; ++j) {
                curr.keys[j] = curr.keys[j + 1];
                curr.children[j] = curr.children[j + 1];
            }
            curr.count--;
            write_node(curr.id, curr);
            
            // update parent keys recursively
            if (i == 0 && curr.count > 0 && curr.parent != -1) {
                Key new_first = curr.keys[0];
                int p_id = curr.parent;
                int child_id = curr.id;
                while (p_id != -1) {
                    Node p;
                    read_node(p_id, p);
                    int idx = 0;
                    while (idx < p.count && p.children[idx] != child_id) idx++;
                    if (idx < p.count) {
                        p.keys[idx] = new_first;
                        write_node(p.id, p);
                        if (idx == 0) {
                            child_id = p.id;
                            p_id = p.parent;
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }
            }
        }
    }

    void find(const char* k) {
        Key key(k, -1);
        Node curr;
        read_node(root_id, curr);
        
        while (!curr.is_leaf) {
            int i = 0;
            while (i < curr.count - 1 && !(key < curr.keys[i + 1])) i++;
            read_node(curr.children[i], curr);
        }
        
        bool found = false;
        while (true) {
            for (int i = 0; i < curr.count; ++i) {
                if (strcmp(curr.keys[i].k, k) == 0) {
                    if (found) std::cout << " ";
                    std::cout << curr.keys[i].v;
                    found = true;
                } else if (strcmp(curr.keys[i].k, k) > 0) {
                    if (found) std::cout << "\n";
                    else std::cout << "null\n";
                    return;
                }
            }
            if (curr.next != -1) {
                read_node(curr.next, curr);
            } else {
                break;
            }
        }
        if (found) std::cout << "\n";
        else std::cout << "null\n";
    }
};

#endif
