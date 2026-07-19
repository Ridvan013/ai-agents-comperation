#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <iomanip>

using namespace std;

struct Book {
    char ISBN[21];
    char BookName[61];
    char Author[61];
    char Keyword[61];
    double Price;
    long long StockQuantity;
};

struct Account {
    char UserID[31];
    char Password[31];
    char Username[31];
    int Privilege;
};

template <size_t L>
struct StringKey {
    char data[L];
    StringKey() { memset(data, 0, L); }
    StringKey(const char* s) {
        memset(data, 0, L);
        strncpy(data, s, L - 1);
    }
    bool operator<(const StringKey& o) const { return strcmp(data, o.data) < 0; }
    bool operator==(const StringKey& o) const { return strcmp(data, o.data) == 0; }
    bool operator<=(const StringKey& o) const { return strcmp(data, o.data) <= 0; }
};

struct IndexKey {
    char key[61];
    char ISBN[21];
    IndexKey() { memset(key, 0, sizeof(key)); memset(ISBN, 0, sizeof(ISBN)); }
    IndexKey(const char* k, const char* i) {
        memset(key, 0, sizeof(key));
        memset(ISBN, 0, sizeof(ISBN));
        if (k) strncpy(key, k, 60);
        if (i) strncpy(ISBN, i, 20);
    }
    bool operator<(const IndexKey& o) const {
        int c = strcmp(key, o.key);
        if (c != 0) return c < 0;
        return strcmp(ISBN, o.ISBN) < 0;
    }
    bool operator==(const IndexKey& o) const {
        return strcmp(key, o.key) == 0 && strcmp(ISBN, o.ISBN) == 0;
    }
    bool operator<=(const IndexKey& o) const {
        return *this < o || *this == o;
    }
};

template <typename T>
class DataFile {
    fstream file;
    string filename;
    int count;
public:
    DataFile(const string& fname) : filename(fname), count(0) {
        file.open(filename, ios::in | ios::out | ios::binary);
        if (!file) {
            file.open(filename, ios::out | ios::binary);
            file.close();
            file.open(filename, ios::in | ios::out | ios::binary);
            set_header();
        } else {
            get_header();
        }
    }
    ~DataFile() { if (file.is_open()) file.close(); }
    void get_header() {
        file.seekg(0);
        file.read((char*)&count, sizeof(int));
    }
    void set_header() {
        file.seekp(0);
        file.write((char*)&count, sizeof(int));
    }
    int insert(const T& data) {
        int idx = count++;
        file.seekp(sizeof(int) + idx * sizeof(T));
        file.write((char*)&data, sizeof(T));
        set_header();
        return idx;
    }
    void update(int idx, const T& data) {
        file.seekp(sizeof(int) + idx * sizeof(T));
        file.write((char*)&data, sizeof(T));
    }
    void read(int idx, T& data) {
        file.seekg(sizeof(int) + idx * sizeof(T));
        file.read((char*)&data, sizeof(T));
    }
};

template <typename Key, int M>
class BPlusTree {
public:
    struct Node {
        bool is_leaf;
        int size;
        int next;
        Key keys[M];
        int values[M + 1];
    };
private:
    fstream file;
    string filename;
    int root;
    int node_count;

    void read_node(int idx, Node& node) {
        file.seekg(sizeof(int)*2 + idx * sizeof(Node));
        file.read((char*)&node, sizeof(Node));
    }
    void write_node(int idx, const Node& node) {
        file.seekp(sizeof(int)*2 + idx * sizeof(Node));
        file.write((char*)&node, sizeof(Node));
    }
    int new_node() {
        Node node;
        node.is_leaf = true;
        node.size = 0;
        node.next = -1;
        int idx = node_count++;
        write_node(idx, node);
        set_header();
        return idx;
    }
    void get_header() {
        file.seekg(0);
        file.read((char*)&root, sizeof(int));
        file.read((char*)&node_count, sizeof(int));
    }
    void set_header() {
        file.seekp(0);
        file.write((char*)&root, sizeof(int));
        file.write((char*)&node_count, sizeof(int));
    }
    int insert_internal(int node_idx, Key key, int value, Key& up_key) {
        Node node; read_node(node_idx, node);
        if (node.is_leaf) {
            int pos = 0;
            while (pos < node.size && node.keys[pos] < key) pos++;
            if (pos < node.size && node.keys[pos] == key) {
                node.values[pos] = value;
                write_node(node_idx, node);
                return -1;
            }
            for (int i = node.size; i > pos; i--) {
                node.keys[i] = node.keys[i - 1];
                node.values[i] = node.values[i - 1];
            }
            node.keys[pos] = key;
            node.values[pos] = value;
            node.size++;
            if (node.size < M) {
                write_node(node_idx, node);
                return -1;
            } else {
                int new_idx = new_node();
                Node new_n; read_node(new_idx, new_n);
                new_n.is_leaf = true;
                int mid = M / 2;
                new_n.size = M - mid;
                for (int i = mid; i < M; i++) {
                    new_n.keys[i - mid] = node.keys[i];
                    new_n.values[i - mid] = node.values[i];
                }
                node.size = mid;
                new_n.next = node.next;
                node.next = new_idx;
                write_node(node_idx, node);
                write_node(new_idx, new_n);
                up_key = new_n.keys[0];
                return new_idx;
            }
        } else {
            int pos = 0;
            while (pos < node.size && node.keys[pos] <= key) pos++;
            Key child_up_key;
            int new_child = insert_internal(node.values[pos], key, value, child_up_key);
            if (new_child == -1) return -1;
            
            for (int i = node.size; i > pos; i--) {
                node.keys[i] = node.keys[i - 1];
                node.values[i + 1] = node.values[i];
            }
            node.keys[pos] = child_up_key;
            node.values[pos + 1] = new_child;
            node.size++;
            if (node.size < M) {
                write_node(node_idx, node);
                return -1;
            } else {
                int new_idx = new_node();
                Node new_n; read_node(new_idx, new_n);
                new_n.is_leaf = false;
                int mid = M / 2;
                up_key = node.keys[mid];
                new_n.size = M - mid - 1;
                for (int i = mid + 1; i < M; i++) {
                    new_n.keys[i - (mid + 1)] = node.keys[i];
                    new_n.values[i - (mid + 1)] = node.values[i];
                }
                new_n.values[M - (mid + 1)] = node.values[M];
                node.size = mid;
                write_node(node_idx, node);
                write_node(new_idx, new_n);
                return new_idx;
            }
        }
    }
    void erase_internal(int node_idx, Key key) {
        Node node; read_node(node_idx, node);
        if (node.is_leaf) {
            int pos = 0;
            while (pos < node.size && node.keys[pos] < key) pos++;
            if (pos < node.size && node.keys[pos] == key) {
                for (int i = pos; i < node.size - 1; i++) {
                    node.keys[i] = node.keys[i + 1];
                    node.values[i] = node.values[i + 1];
                }
                node.size--;
                write_node(node_idx, node);
            }
        } else {
            int pos = 0;
            while (pos < node.size && node.keys[pos] <= key) pos++;
            erase_internal(node.values[pos], key);
        }
    }
public:
    BPlusTree(const string& fname) : filename(fname), root(-1), node_count(0) {
        file.open(filename, ios::in | ios::out | ios::binary);
        if (!file) {
            file.open(filename, ios::out | ios::binary);
            file.close();
            file.open(filename, ios::in | ios::out | ios::binary);
            set_header();
        } else {
            get_header();
        }
    }
    ~BPlusTree() { if (file.is_open()) file.close(); }
    void insert(Key key, int value) {
        if (root == -1) {
            root = new_node();
            Node r; read_node(root, r);
            r.keys[0] = key;
            r.values[0] = value;
            r.size = 1;
            write_node(root, r);
            set_header();
            return;
        }
        Key up_key;
        int new_child = insert_internal(root, key, value, up_key);
        if (new_child != -1) {
            int new_root = new_node();
            Node nr; read_node(new_root, nr);
            nr.is_leaf = false;
            nr.size = 1;
            nr.keys[0] = up_key;
            nr.values[0] = root;
            nr.values[1] = new_child;
            write_node(new_root, nr);
            root = new_root;
            set_header();
        }
    }
    void erase(Key key) {
        if (root == -1) return;
        erase_internal(root, key);
    }
    bool find(Key key, int& value) {
        if (root == -1) return false;
        int curr = root;
        Node node; 
        while (true) {
            read_node(curr, node);
            if (node.is_leaf) {
                int pos = 0;
                while (pos < node.size && node.keys[pos] < key) pos++;
                if (pos < node.size && node.keys[pos] == key) {
                    value = node.values[pos];
                    return true;
                }
                return false;
            } else {
                int pos = 0;
                while (pos < node.size && node.keys[pos] <= key) pos++;
                curr = node.values[pos];
            }
        }
    }
    void find_prefix(const char* prefix, vector<int>& results) {
        if (root == -1) return;
        int curr = root;
        Node node;
        Key query(prefix, "");
        while (true) {
            read_node(curr, node);
            if (node.is_leaf) break;
            int pos = 0;
            while (pos < node.size && node.keys[pos] <= query) pos++;
            curr = node.values[pos];
        }
        while (curr != -1) {
            read_node(curr, node);
            for (int i = 0; i < node.size; i++) {
                if (strcmp(node.keys[i].key, prefix) == 0) {
                    results.push_back(node.values[i]);
                } else if (strcmp(node.keys[i].key, prefix) > 0) {
                    return; 
                }
            }
            curr = node.next;
        }
    }
    int get_first_leaf() {
        if (root == -1) return -1;
        int curr = root;
        Node node; read_node(curr, node);
        while (!node.is_leaf) {
            curr = node.values[0];
            read_node(curr, node);
        }
        return curr;
    }
    void get_all(vector<int>& results) {
        int curr = get_first_leaf();
        while (curr != -1) {
            Node node; read_node(curr, node);
            for (int i = 0; i < node.size; i++) {
                results.push_back(node.values[i]);
            }
            curr = node.next;
        }
    }
};

struct FinanceRecord {
    double income;
    double expenditure;
};
class FinanceFile {
    fstream file;
    string filename;
    int count;
public:
    FinanceFile(const string& fname) : filename(fname), count(0) {
        file.open(filename, ios::in | ios::out | ios::binary);
        if (!file) {
            file.open(filename, ios::out | ios::binary);
            file.close();
            file.open(filename, ios::in | ios::out | ios::binary);
            set_header();
        } else {
            get_header();
        }
    }
    void get_header() {
        file.seekg(0);
        file.read((char*)&count, sizeof(int));
    }
    void set_header() {
        file.seekp(0);
        file.write((char*)&count, sizeof(int));
    }
    void add(double inc, double exp) {
        FinanceRecord r;
        if (count > 0) {
            read(count - 1, r);
            r.income += inc;
            r.expenditure += exp;
        } else {
            r.income = inc;
            r.expenditure = exp;
        }
        file.seekp(sizeof(int) + count * sizeof(FinanceRecord));
        file.write((char*)&r, sizeof(FinanceRecord));
        count++;
        set_header();
    }
    void read(int idx, FinanceRecord& data) {
        file.seekg(sizeof(int) + idx * sizeof(FinanceRecord));
        file.read((char*)&data, sizeof(FinanceRecord));
    }
    int get_count() const { return count; }
};

bool is_valid_userid(const string& s) {
    if (s.empty() || s.length() > 30) return false;
    for (char c : s) if (!isalnum(c) && c != '_') return false;
    return true;
}
bool is_valid_password(const string& s) { return is_valid_userid(s); }
bool is_valid_username(const string& s) {
    if (s.empty() || s.length() > 30) return false;
    for (char c : s) if (c < 33 || c > 126) return false;
    return true;
}
bool is_valid_privilege(const string& s) {
    return s.length() == 1 && (s[0] == '1' || s[0] == '3' || s[0] == '7');
}
bool is_valid_isbn(const string& s) {
    if (s.empty() || s.length() > 20) return false;
    for (char c : s) if (c < 33 || c > 126) return false;
    return true;
}
bool is_valid_name(const string& s) {
    if (s.empty() || s.length() > 60) return false;
    for (char c : s) if (c < 33 || c > 126 || c == '"') return false;
    return true;
}
bool is_valid_keyword(const string& s) {
    if (s.empty() || s.length() > 60) return false;
    for (char c : s) if (c < 33 || c > 126 || c == '"') return false;
    return true;
}
bool is_valid_price(const string& s) {
    if (s.empty() || s.length() > 13) return false;
    int dot_count = 0;
    for (char c : s) {
        if (c == '.') dot_count++;
        else if (!isdigit(c)) return false;
    }
    return dot_count <= 1 && s.front() != '.' && s.back() != '.';
}
bool is_valid_quantity(const string& s) {
    if (s.empty() || s.length() > 10) return false;
    for (char c : s) if (!isdigit(c)) return false;
    long long val = stoll(s);
    return val <= 2147483647;
}

vector<string> parse_tokens(const string& line) {
    vector<string> tokens;
    string token;
    for (char c : line) {
        if (c == ' ' || c == '\r' || c == '\n' || c == '\t') {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token.push_back(c);
        }
    }
    if (!token.empty()) tokens.push_back(token);
    return tokens;
}

bool parse_arg(const string& token, string& key, string& val) {
    if (token.length() > 6 && token.substr(0, 6) == "-ISBN=") {
        key = "ISBN"; val = token.substr(6); return true;
    } else if (token.length() > 7 && token.substr(0, 7) == "-price=") {
        key = "price"; val = token.substr(7); return true;
    } else if (token.length() > 7 && token.substr(0, 7) == "-name=\"") {
        if (token.back() == '"') {
            key = "name"; val = token.substr(7, token.length() - 8); return true;
        }
    } else if (token.length() > 9 && token.substr(0, 9) == "-author=\"") {
        if (token.back() == '"') {
            key = "author"; val = token.substr(9, token.length() - 10); return true;
        }
    } else if (token.length() > 10 && token.substr(0, 10) == "-keyword=\"") {
        if (token.back() == '"') {
            key = "keyword"; val = token.substr(10, token.length() - 11); return true;
        }
    }
    return false;
}

vector<string> split_keywords(const string& kw) {
    vector<string> res;
    string cur;
    for (char c : kw) {
        if (c == '|') {
            if (cur.empty()) return {};
            res.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (cur.empty()) return {};
    res.push_back(cur);
    return res;
}

bool has_duplicate_segments(const vector<string>& kws) {
    for (size_t i = 0; i < kws.size(); ++i) {
        for (size_t j = i + 1; j < kws.size(); ++j) {
            if (kws[i] == kws[j]) return true;
        }
    }
    return false;
}

void print_book(const Book& b) {
    printf("%s\t%s\t%s\t%s\t%.2f\t%lld\n",
        b.ISBN, b.BookName, b.Author, b.Keyword, b.Price, b.StockQuantity);
}

struct Session {
    Account acc;
    string selected_isbn;
    bool has_selected;
};
vector<Session> login_stack;

int main() {
    BPlusTree<StringKey<31>, 60> user_index("user_index.dat");
    DataFile<Account> users_file("users.dat");

    BPlusTree<StringKey<21>, 60> isbn_index("isbn_index.dat");
    BPlusTree<IndexKey, 60> name_index("name_index.dat");
    BPlusTree<IndexKey, 60> author_index("author_index.dat");
    BPlusTree<IndexKey, 60> keyword_index("keyword_index.dat");
    DataFile<Book> books_file("books.dat");

    FinanceFile finance_file("finance.dat");

    int dummy_uid;
    if (!user_index.find(StringKey<31>("root"), dummy_uid)) {
        Account root_acc;
        memset(&root_acc, 0, sizeof(Account));
        strcpy(root_acc.UserID, "root");
        strcpy(root_acc.Password, "sjtu");
        strcpy(root_acc.Username, "root");
        root_acc.Privilege = 7;
        int idx = users_file.insert(root_acc);
        user_index.insert(StringKey<31>("root"), idx);
    }

    string line;
    while (getline(cin, line)) {
        if (line.empty() || line.find_first_not_of(" \r\n\t") == string::npos) continue;
        auto tokens = parse_tokens(line);
        if (tokens.empty()) continue;
        string cmd = tokens[0];
        
        if (cmd == "exit" || cmd == "quit") {
            if (tokens.size() != 1) { cout << "Invalid\n"; continue; }
            break;
        }

        int cur_priv = login_stack.empty() ? 0 : login_stack.back().acc.Privilege;

        if (cmd == "su") {
            if (tokens.size() != 2 && tokens.size() != 3) { cout << "Invalid\n"; continue; }
            string uid_str = tokens[1];
            if (!is_valid_userid(uid_str)) { cout << "Invalid\n"; continue; }
            int acc_idx;
            if (!user_index.find(StringKey<31>(uid_str.c_str()), acc_idx)) {
                cout << "Invalid\n"; continue;
            }
            Account acc;
            users_file.read(acc_idx, acc);
            
            if (tokens.size() == 2) {
                if (cur_priv <= acc.Privilege) { cout << "Invalid\n"; continue; }
            } else {
                string pwd_str = tokens[2];
                if (pwd_str != acc.Password) { cout << "Invalid\n"; continue; }
            }
            Session sess;
            sess.acc = acc;
            sess.has_selected = false;
            login_stack.push_back(sess);
        } else if (cmd == "logout") {
            if (cur_priv < 1 || login_stack.empty() || tokens.size() != 1) { cout << "Invalid\n"; continue; }
            login_stack.pop_back();
        } else if (cmd == "register") {
            if (cur_priv < 0 || tokens.size() != 4) { cout << "Invalid\n"; continue; }
            string uid_str = tokens[1], pwd_str = tokens[2], uname_str = tokens[3];
            if (!is_valid_userid(uid_str) || !is_valid_password(pwd_str) || !is_valid_username(uname_str)) {
                cout << "Invalid\n"; continue;
            }
            int dummy;
            if (user_index.find(StringKey<31>(uid_str.c_str()), dummy)) {
                cout << "Invalid\n"; continue;
            }
            Account acc;
            memset(&acc, 0, sizeof(Account));
            strcpy(acc.UserID, uid_str.c_str());
            strcpy(acc.Password, pwd_str.c_str());
            strcpy(acc.Username, uname_str.c_str());
            acc.Privilege = 1;
            int idx = users_file.insert(acc);
            user_index.insert(StringKey<31>(uid_str.c_str()), idx);
        } else if (cmd == "passwd") {
            if (cur_priv < 1) { cout << "Invalid\n"; continue; }
            if (tokens.size() != 3 && tokens.size() != 4) { cout << "Invalid\n"; continue; }
            string uid_str = tokens[1];
            int acc_idx;
            if (!user_index.find(StringKey<31>(uid_str.c_str()), acc_idx)) {
                cout << "Invalid\n"; continue;
            }
            Account acc;
            users_file.read(acc_idx, acc);
            
            if (tokens.size() == 3) {
                if (cur_priv != 7) { cout << "Invalid\n"; continue; }
                string new_pwd = tokens[2];
                if (!is_valid_password(new_pwd)) { cout << "Invalid\n"; continue; }
                strcpy(acc.Password, new_pwd.c_str());
                users_file.update(acc_idx, acc);
            } else {
                string cur_pwd = tokens[2];
                string new_pwd = tokens[3];
                if (!is_valid_password(new_pwd) || !is_valid_password(cur_pwd)) { cout << "Invalid\n"; continue; }
                if (cur_pwd != acc.Password) { cout << "Invalid\n"; continue; }
                strcpy(acc.Password, new_pwd.c_str());
                users_file.update(acc_idx, acc);
            }
        } else if (cmd == "useradd") {
            if (cur_priv < 3) { cout << "Invalid\n"; continue; }
            if (tokens.size() != 5) { cout << "Invalid\n"; continue; }
            string uid_str = tokens[1], pwd_str = tokens[2], priv_str = tokens[3], uname_str = tokens[4];
            if (!is_valid_userid(uid_str) || !is_valid_password(pwd_str) || !is_valid_privilege(priv_str) || !is_valid_username(uname_str)) {
                cout << "Invalid\n"; continue;
            }
            int new_priv = stoi(priv_str);
            if (new_priv >= cur_priv) { cout << "Invalid\n"; continue; }
            int dummy;
            if (user_index.find(StringKey<31>(uid_str.c_str()), dummy)) {
                cout << "Invalid\n"; continue;
            }
            Account acc;
            memset(&acc, 0, sizeof(Account));
            strcpy(acc.UserID, uid_str.c_str());
            strcpy(acc.Password, pwd_str.c_str());
            strcpy(acc.Username, uname_str.c_str());
            acc.Privilege = new_priv;
            int idx = users_file.insert(acc);
            user_index.insert(StringKey<31>(uid_str.c_str()), idx);
        } else if (cmd == "delete") {
            if (cur_priv < 7 || tokens.size() != 2) { cout << "Invalid\n"; continue; }
            string uid_str = tokens[1];
            int acc_idx;
            if (!user_index.find(StringKey<31>(uid_str.c_str()), acc_idx)) {
                cout << "Invalid\n"; continue;
            }
            bool logged_in = false;
            for (const auto& s : login_stack) {
                if (s.acc.UserID == uid_str) { logged_in = true; break; }
            }
            if (logged_in) { cout << "Invalid\n"; continue; }
            user_index.erase(StringKey<31>(uid_str.c_str()));
        } else if (cmd == "show") {
            if (cur_priv < 1) { cout << "Invalid\n"; continue; }
            if (tokens.size() == 1) {
                vector<int> results;
                isbn_index.get_all(results);
                if (results.empty()) { cout << "\n"; }
                else {
                    for (int idx : results) {
                        Book b; books_file.read(idx, b);
                        print_book(b);
                    }
                }
            } else if (tokens.size() == 2 && tokens[1] == "finance") {
                if (cur_priv < 7) { cout << "Invalid\n"; continue; }
                int total = finance_file.get_count();
                if (total == 0) {
                    printf("+ 0.00 - 0.00\n");
                } else {
                    FinanceRecord last;
                    finance_file.read(total - 1, last);
                    printf("+ %.2f - %.2f\n", last.income, last.expenditure);
                }
            } else if (tokens.size() == 3 && tokens[1] == "finance") {
                if (cur_priv < 7) { cout << "Invalid\n"; continue; }
                string c_str = tokens[2];
                if (!is_valid_quantity(c_str)) { cout << "Invalid\n"; continue; }
                long long count = stoll(c_str);
                if (count < 0) { cout << "Invalid\n"; continue; }
                int total = finance_file.get_count();
                if (count > total) { cout << "Invalid\n"; continue; }
                if (count == 0) { cout << "\n"; continue; }
                
                FinanceRecord last;
                finance_file.read(total - 1, last);
                if (count == total) {
                    printf("+ %.2f - %.2f\n", last.income, last.expenditure);
                } else {
                    FinanceRecord earlier;
                    finance_file.read(total - 1 - count, earlier);
                    printf("+ %.2f - %.2f\n", last.income - earlier.income, last.expenditure - earlier.expenditure);
                }
            } else if (tokens.size() == 2) {
                string k, v;
                if (!parse_arg(tokens[1], k, v) || v.empty()) { cout << "Invalid\n"; continue; }
                vector<int> results;
                if (k == "ISBN") {
                    int idx;
                    if (isbn_index.find(StringKey<21>(v.c_str()), idx)) results.push_back(idx);
                } else if (k == "name") {
                    name_index.find_prefix(v.c_str(), results);
                } else if (k == "author") {
                    author_index.find_prefix(v.c_str(), results);
                } else if (k == "keyword") {
                    auto kws = split_keywords(v);
                    if (kws.size() > 1) { cout << "Invalid\n"; continue; }
                    keyword_index.find_prefix(v.c_str(), results);
                } else { cout << "Invalid\n"; continue; }
                
                if (results.empty()) cout << "\n";
                else {
                    for (int idx : results) {
                        Book b; books_file.read(idx, b);
                        print_book(b);
                    }
                }
            } else {
                cout << "Invalid\n"; continue;
            }
        } else if (cmd == "buy") {
            if (cur_priv < 1 || tokens.size() != 3) { cout << "Invalid\n"; continue; }
            string isbn_str = tokens[1], qty_str = tokens[2];
            if (!is_valid_isbn(isbn_str) || !is_valid_quantity(qty_str)) { cout << "Invalid\n"; continue; }
            long long qty = stoll(qty_str);
            if (qty <= 0) { cout << "Invalid\n"; continue; }
            int b_idx;
            if (!isbn_index.find(StringKey<21>(isbn_str.c_str()), b_idx)) { cout << "Invalid\n"; continue; }
            Book b; books_file.read(b_idx, b);
            if (b.StockQuantity < qty) { cout << "Invalid\n"; continue; }
            b.StockQuantity -= qty;
            books_file.update(b_idx, b);
            double cost = b.Price * qty;
            finance_file.add(cost, 0.0);
            printf("%.2f\n", cost);
        } else if (cmd == "select") {
            if (cur_priv < 3 || tokens.size() != 2) { cout << "Invalid\n"; continue; }
            string isbn_str = tokens[1];
            if (!is_valid_isbn(isbn_str)) { cout << "Invalid\n"; continue; }
            int b_idx;
            if (!isbn_index.find(StringKey<21>(isbn_str.c_str()), b_idx)) {
                Book b;
                memset(&b, 0, sizeof(Book));
                strcpy(b.ISBN, isbn_str.c_str());
                b_idx = books_file.insert(b);
                isbn_index.insert(StringKey<21>(isbn_str.c_str()), b_idx);
            }
            login_stack.back().selected_isbn = isbn_str;
            login_stack.back().has_selected = true;
        } else if (cmd == "modify") {
            if (cur_priv < 3 || tokens.size() < 2) { cout << "Invalid\n"; continue; }
            if (!login_stack.back().has_selected) { cout << "Invalid\n"; continue; }
            
            bool has_err = false;
            string new_isbn, new_name, new_author, new_keyword, new_price;
            bool m_isbn = false, m_name = false, m_author = false, m_keyword = false, m_price = false;
            for (size_t i = 1; i < tokens.size(); ++i) {
                string k, v;
                if (!parse_arg(tokens[i], k, v) || v.empty()) { has_err = true; break; }
                if (k == "ISBN") {
                    if (m_isbn) { has_err = true; break; }
                    m_isbn = true; new_isbn = v;
                    if (!is_valid_isbn(new_isbn)) { has_err = true; break; }
                } else if (k == "name") {
                    if (m_name) { has_err = true; break; }
                    m_name = true; new_name = v;
                    if (!is_valid_name(new_name)) { has_err = true; break; }
                } else if (k == "author") {
                    if (m_author) { has_err = true; break; }
                    m_author = true; new_author = v;
                    if (!is_valid_name(new_author)) { has_err = true; break; }
                } else if (k == "keyword") {
                    if (m_keyword) { has_err = true; break; }
                    m_keyword = true; new_keyword = v;
                    if (!is_valid_keyword(new_keyword)) { has_err = true; break; }
                    auto kws = split_keywords(new_keyword);
                    if (kws.empty() || has_duplicate_segments(kws)) { has_err = true; break; }
                } else if (k == "price") {
                    if (m_price) { has_err = true; break; }
                    m_price = true; new_price = v;
                    if (!is_valid_price(new_price)) { has_err = true; break; }
                }
            }
            if (has_err) { cout << "Invalid\n"; continue; }
            
            string old_isbn = login_stack.back().selected_isbn;
            if (m_isbn) {
                if (new_isbn == old_isbn) { cout << "Invalid\n"; continue; }
                int dummy;
                if (isbn_index.find(StringKey<21>(new_isbn.c_str()), dummy)) { cout << "Invalid\n"; continue; }
            }
            
            int b_idx;
            isbn_index.find(StringKey<21>(old_isbn.c_str()), b_idx);
            Book b; books_file.read(b_idx, b);
            
            if (m_isbn || m_name) {
                if (b.BookName[0]) name_index.erase(IndexKey(b.BookName, old_isbn.c_str()));
            }
            if (m_isbn || m_author) {
                if (b.Author[0]) author_index.erase(IndexKey(b.Author, old_isbn.c_str()));
            }
            if (m_isbn || m_keyword) {
                if (b.Keyword[0]) {
                    auto kws = split_keywords(b.Keyword);
                    for (const auto& kw : kws) keyword_index.erase(IndexKey(kw.c_str(), old_isbn.c_str()));
                }
            }
            if (m_isbn) {
                isbn_index.erase(StringKey<21>(old_isbn.c_str()));
            }
            
            if (m_isbn) strcpy(b.ISBN, new_isbn.c_str());
            if (m_name) strcpy(b.BookName, new_name.c_str());
            if (m_author) strcpy(b.Author, new_author.c_str());
            if (m_keyword) strcpy(b.Keyword, new_keyword.c_str());
            if (m_price) b.Price = stod(new_price);
            books_file.update(b_idx, b);
            
            string cur_isbn = m_isbn ? new_isbn : old_isbn;
            if (m_isbn || m_name) {
                if (b.BookName[0]) name_index.insert(IndexKey(b.BookName, cur_isbn.c_str()), b_idx);
            }
            if (m_isbn || m_author) {
                if (b.Author[0]) author_index.insert(IndexKey(b.Author, cur_isbn.c_str()), b_idx);
            }
            if (m_isbn || m_keyword) {
                if (b.Keyword[0]) {
                    auto kws = split_keywords(b.Keyword);
                    for (const auto& kw : kws) keyword_index.insert(IndexKey(kw.c_str(), cur_isbn.c_str()), b_idx);
                }
            }
            if (m_isbn) {
                isbn_index.insert(StringKey<21>(cur_isbn.c_str()), b_idx);
                for (auto& s : login_stack) {
                    if (s.has_selected && s.selected_isbn == old_isbn) {
                        s.selected_isbn = new_isbn;
                    }
                }
            }
        } else if (cmd == "import") {
            if (cur_priv < 3 || tokens.size() != 3) { cout << "Invalid\n"; continue; }
            if (!login_stack.back().has_selected) { cout << "Invalid\n"; continue; }
            string qty_str = tokens[1], cost_str = tokens[2];
            if (!is_valid_quantity(qty_str) || !is_valid_price(cost_str)) { cout << "Invalid\n"; continue; }
            long long qty = stoll(qty_str);
            double cost = stod(cost_str);
            if (qty <= 0 || cost <= 0) { cout << "Invalid\n"; continue; }
            
            int b_idx;
            isbn_index.find(StringKey<21>(login_stack.back().selected_isbn.c_str()), b_idx);
            Book b; books_file.read(b_idx, b);
            b.StockQuantity += qty;
            books_file.update(b_idx, b);
            finance_file.add(0.0, cost);
        } else if (cmd == "log") {
            if (cur_priv < 7 || tokens.size() != 1) { cout << "Invalid\n"; continue; }
        } else if (cmd == "report") {
            if (cur_priv < 7 || tokens.size() != 2 || (tokens[1] != "finance" && tokens[1] != "employee")) { cout << "Invalid\n"; continue; }
        } else {
            cout << "Invalid\n";
        }
    }
    return 0;
}
