// Problem 015 - File Storage (ACMOJ 2545)
// A persistent key-value store on disk providing std::map-like behaviour.
//
// Key   = (index string[<=64 bytes], int value), unique as a pair.
// Ops   : insert index value | delete index value | find index
// find  : print all values for index in ascending order, else "null".
//
// Constraints: memory is tight (5-6 MiB) so data lives on disk and only a
// bounded LRU page cache is kept in RAM.  The database file persists between
// runs, so on start-up we open the existing file (or create a fresh one).
//
// Implementation: an on-disk B+ tree.  Insertions split overflowing nodes;
// deletions simply remove the entry (no merging) which keeps the code simple
// and correct - wasted space is bounded and far below the disk limit.

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <climits>
#include <list>
#include <unordered_map>

static const char* DB_FILE = "storage.db";

// Maximum number of keys held in a node before it must split.
// A leaf stores up to M entries; an internal node up to M separator keys
// (and up to M+1 children).  M chosen so a node is a few KB.
static const int M = 60;

struct Key {
    char idx[64];
    int  val;
};

// Total ordering: index bytes first (memcmp over the fixed 64-byte field,
// which is always zero-padded), then value.
static inline int cmpKey(const Key& a, const Key& b) {
    int c = memcmp(a.idx, b.idx, 64);
    if (c != 0) return c < 0 ? -1 : 1;
    if (a.val < b.val) return -1;
    if (a.val > b.val) return 1;
    return 0;
}

struct Node {
    int  isLeaf;        // 1 = leaf, 0 = internal
    int  cnt;           // number of keys currently stored
    int  nxt;           // leaf: id of next leaf (-1 if none); unused otherwise
    Key  key[M + 1];    // one extra slot to allow overflow before a split
    int  child[M + 2];  // internal: cnt+1 child ids
};

struct Header {
    int root;       // node id of the root, -1 when the tree is empty
    int nodeCount;  // number of node slots allocated (next free id)
    int freeHead;   // head of the free list (unused: we never free nodes)
};

static FILE*  fp = nullptr;
static Header hdr;

// ---- raw block I/O ---------------------------------------------------------
// Block 0 is reserved for the header; node id i lives at offset (i+1)*blockSize.
static inline long nodeOffset(int id) {
    return (long)sizeof(Node) * (long)(id + 1);
}

static void readRaw(int id, Node& n) {
    fseek(fp, nodeOffset(id), SEEK_SET);
    if (fread(&n, sizeof(Node), 1, fp) != 1) {
        // Should not happen for a valid id; zero-initialise defensively.
        memset(&n, 0, sizeof(Node));
    }
}

static void writeRaw(int id, const Node& n) {
    fseek(fp, nodeOffset(id), SEEK_SET);
    fwrite(&n, sizeof(Node), 1, fp);
}

// ---- bounded LRU page cache ------------------------------------------------
struct Entry {
    int  id;
    Node node;
    bool dirty;
};

static std::list<Entry> lru;                                  // front = MRU
static std::unordered_map<int, std::list<Entry>::iterator> cmap;
static const size_t CACHE_CAP = 256;                          // ~1.1 MiB

static void evict() {
    while (lru.size() > CACHE_CAP) {
        Entry& e = lru.back();
        if (e.dirty) writeRaw(e.id, e.node);
        cmap.erase(e.id);
        lru.pop_back();
    }
}

static Node getNode(int id) {
    std::unordered_map<int, std::list<Entry>::iterator>::iterator it = cmap.find(id);
    if (it != cmap.end()) {
        lru.splice(lru.begin(), lru, it->second);
        return it->second->node;
    }
    Entry e;
    e.id = id;
    e.dirty = false;
    readRaw(id, e.node);
    lru.push_front(e);
    cmap[id] = lru.begin();
    evict();
    return lru.begin()->node;
}

// Read-only pointer into the cached node (no 4.4 KB copy).  Valid only while
// the caller does not access enough *other* nodes to evict it; find() holds at
// most the current node while fetching the next, so a small cache suffices.
static Node* cacheGet(int id) {
    std::unordered_map<int, std::list<Entry>::iterator>::iterator it = cmap.find(id);
    if (it != cmap.end()) {
        lru.splice(lru.begin(), lru, it->second);
        return &it->second->node;
    }
    Entry e;
    e.id = id;
    e.dirty = false;
    readRaw(id, e.node);
    lru.push_front(e);
    cmap[id] = lru.begin();
    Node* p = &lru.begin()->node;  // freshly inserted at front
    evict();                       // only evicts from the back, never the front
    return p;
}

static void putNode(int id, const Node& n) {
    std::unordered_map<int, std::list<Entry>::iterator>::iterator it = cmap.find(id);
    if (it != cmap.end()) {
        it->second->node = n;
        it->second->dirty = true;
        lru.splice(lru.begin(), lru, it->second);
        return;
    }
    Entry e;
    e.id = id;
    e.node = n;
    e.dirty = true;
    lru.push_front(e);
    cmap[id] = lru.begin();
    evict();
}

static void flushAll() {
    for (std::list<Entry>::iterator it = lru.begin(); it != lru.end(); ++it)
        if (it->dirty) writeRaw(it->id, it->node);
}

static int allocNode() {
    return hdr.nodeCount++;
}

// Index of the child to descend into for key K (also the leaf insert branch).
static inline int childIndex(const Node& n, const Key& K) {
    int i = 0;
    while (i < n.cnt && cmpKey(K, n.key[i]) >= 0) ++i;
    return i;
}

// ---- B+ tree operations ----------------------------------------------------
static void makeKey(Key& k, const char* idx, int len, int val) {
    if (len > 64) len = 64;   // spec guarantees <= 64; clamp defensively
    memset(k.idx, 0, 64);
    memcpy(k.idx, idx, len);
    k.val = val;
}

static void insertKey(const Key& K) {
    if (hdr.root == -1) {
        Node leaf;
        leaf.isLeaf = 1;
        leaf.cnt = 1;
        leaf.nxt = -1;
        leaf.key[0] = K;
        int id = allocNode();
        putNode(id, leaf);
        hdr.root = id;
        return;
    }

    // Descend to the target leaf, recording (nodeId, childIndex) along the way.
    static int pathId[64];
    static int pathCi[64];
    int depth = 0;

    int cur = hdr.root;
    Node n = getNode(cur);
    while (!n.isLeaf) {
        int i = childIndex(n, K);
        pathId[depth] = cur;
        pathCi[depth] = i;
        ++depth;
        cur = n.child[i];
        n = getNode(cur);
    }

    // Locate insert position; skip if the exact pair already exists.
    int pos = 0;
    while (pos < n.cnt && cmpKey(n.key[pos], K) < 0) ++pos;
    if (pos < n.cnt && cmpKey(n.key[pos], K) == 0) return;

    for (int j = n.cnt; j > pos; --j) n.key[j] = n.key[j - 1];
    n.key[pos] = K;
    ++n.cnt;

    if (n.cnt <= M) {
        putNode(cur, n);
        return;
    }

    // Split the leaf: right half moves to a new leaf, separator = right's first.
    int split = (M + 1) / 2;
    Node r;
    r.isLeaf = 1;
    r.cnt = n.cnt - split;
    for (int j = 0; j < r.cnt; ++j) r.key[j] = n.key[split + j];
    n.cnt = split;
    int rid = allocNode();
    r.nxt = n.nxt;
    n.nxt = rid;
    putNode(cur, n);
    putNode(rid, r);

    Key  sep = r.key[0];
    int  newChild = rid;

    // Propagate splits up the recorded path.
    while (depth > 0) {
        --depth;
        int pid = pathId[depth];
        int ci  = pathCi[depth];
        Node p = getNode(pid);

        for (int j = p.cnt; j > ci; --j) p.key[j] = p.key[j - 1];
        for (int j = p.cnt + 1; j > ci + 1; --j) p.child[j] = p.child[j - 1];
        p.key[ci] = sep;
        p.child[ci + 1] = newChild;
        ++p.cnt;

        if (p.cnt <= M) {
            putNode(pid, p);
            return;
        }

        // Split internal node: middle key moves up.
        int mid = p.cnt / 2;
        Node rn;
        rn.isLeaf = 0;
        rn.cnt = p.cnt - mid - 1;
        for (int j = 0; j < rn.cnt; ++j) rn.key[j] = p.key[mid + 1 + j];
        for (int j = 0; j <= rn.cnt; ++j) rn.child[j] = p.child[mid + 1 + j];
        Key up = p.key[mid];
        p.cnt = mid;
        int rnid = allocNode();
        putNode(pid, p);
        putNode(rnid, rn);

        sep = up;
        newChild = rnid;
    }

    // Root split: create a new root one level higher.
    Node nr;
    nr.isLeaf = 0;
    nr.cnt = 1;
    nr.key[0] = sep;
    nr.child[0] = hdr.root;
    nr.child[1] = newChild;
    int nrid = allocNode();
    putNode(nrid, nr);
    hdr.root = nrid;
}

static void deleteKey(const Key& K) {
    if (hdr.root == -1) return;
    int cur = hdr.root;
    Node n = getNode(cur);
    while (!n.isLeaf) {
        int i = childIndex(n, K);
        cur = n.child[i];
        n = getNode(cur);
    }
    int pos = 0;
    while (pos < n.cnt && cmpKey(n.key[pos], K) < 0) ++pos;
    if (pos < n.cnt && cmpKey(n.key[pos], K) == 0) {
        for (int j = pos; j < n.cnt - 1; ++j) n.key[j] = n.key[j + 1];
        --n.cnt;
        putNode(cur, n);
    }
    // Entry absent: nothing to do (allowed by the spec).
}

// ---- fast output -----------------------------------------------------------
static char obuf[1 << 16];
static int  olen = 0;

static inline void oflush() {
    fwrite(obuf, 1, olen, stdout);
    olen = 0;
}
static inline void oputc(char c) {
    if (olen >= (int)sizeof(obuf)) oflush();
    obuf[olen++] = c;
}
static inline void oputint(int x) {
    if (x == 0) { oputc('0'); return; }
    char tmp[12];
    int t = 0;
    if (x < 0) { oputc('-'); }
    // value is non-negative per spec, but handle generally.
    unsigned int ux = (x < 0) ? (unsigned int)(-(long long)x) : (unsigned int)x;
    while (ux) { tmp[t++] = char('0' + ux % 10); ux /= 10; }
    while (t) oputc(tmp[--t]);
}

static void findKey(const char* idx, int len) {
    if (hdr.root == -1) { oputc('n'); oputc('u'); oputc('l'); oputc('l'); oputc('\n'); return; }

    Key sk;
    makeKey(sk, idx, len, INT_MIN);  // lower bound for this index

    Node* n = cacheGet(hdr.root);
    while (!n->isLeaf) {
        int i = childIndex(*n, sk);
        n = cacheGet(n->child[i]);
    }

    // First entry with index >= target within this leaf.
    int pos = 0;
    while (pos < n->cnt && cmpKey(n->key[pos], sk) < 0) ++pos;

    bool any = false;
    while (n != nullptr) {
        bool stop = false;
        int cnt = n->cnt;
        for (; pos < cnt; ++pos) {
            int c = memcmp(n->key[pos].idx, sk.idx, 64);
            if (c == 0) {
                if (any) oputc(' ');
                oputint(n->key[pos].val);
                any = true;
            } else if (c > 0) {
                stop = true;
                break;
            }
            // c < 0 cannot occur past the lower bound.
        }
        if (stop) break;
        int next = n->nxt;
        if (next == -1) break;
        n = cacheGet(next);  // fetching next never evicts current (it is MRU)
        pos = 0;
    }

    if (!any) { oputc('n'); oputc('u'); oputc('l'); oputc('l'); }
    oputc('\n');
}

// ---- fast input ------------------------------------------------------------
static char ibuf[1 << 16];
static int  ilen = 0, ipos = 0;

static inline int gc() {
    if (ipos >= ilen) {
        ilen = (int)fread(ibuf, 1, sizeof(ibuf), stdin);
        ipos = 0;
        if (ilen <= 0) return -1;
    }
    return (unsigned char)ibuf[ipos++];
}

// Read a whitespace-delimited token into dst; returns length (<=64 used).
static int readToken(char* dst) {
    int c = gc();
    while (c != -1 && (c == ' ' || c == '\n' || c == '\r' || c == '\t')) c = gc();
    if (c == -1) return -1;
    int len = 0;
    while (c != -1 && c != ' ' && c != '\n' && c != '\r' && c != '\t') {
        if (len < 64) dst[len] = (char)c;
        ++len;
        c = gc();
    }
    return len;
}

static bool readInt(int& out) {
    int c = gc();
    while (c != -1 && (c == ' ' || c == '\n' || c == '\r' || c == '\t')) c = gc();
    if (c == -1) return false;
    bool neg = false;
    if (c == '-') { neg = true; c = gc(); }
    long long v = 0;
    while (c >= '0' && c <= '9') { v = v * 10 + (c - '0'); c = gc(); }
    out = (int)(neg ? -v : v);
    return true;
}

int main() {
    // Open the persistent database, creating it if absent.
    fp = fopen(DB_FILE, "rb+");
    if (fp == nullptr) {
        fp = fopen(DB_FILE, "wb+");
        if (fp == nullptr) return 1;
        hdr.root = -1;
        hdr.nodeCount = 0;
        hdr.freeHead = -1;
        fseek(fp, 0, SEEK_SET);
        fwrite(&hdr, sizeof(hdr), 1, fp);
    } else {
        fseek(fp, 0, SEEK_SET);
        if (fread(&hdr, sizeof(hdr), 1, fp) != 1) {
            hdr.root = -1;
            hdr.nodeCount = 0;
            hdr.freeHead = -1;
        }
    }

    int n;
    if (!readInt(n)) n = 0;

    char cmd[80];
    char idx[80];
    for (int q = 0; q < n; ++q) {
        int clen = readToken(cmd);
        if (clen == -1) break;

        if (clen == 6 && cmd[0] == 'i') {          // insert
            int ilenTok = readToken(idx);
            int val;
            readInt(val);
            Key k;
            makeKey(k, idx, ilenTok, val);
            insertKey(k);
        } else if (clen == 6 && cmd[0] == 'd') {    // delete
            int ilenTok = readToken(idx);
            int val;
            readInt(val);
            Key k;
            makeKey(k, idx, ilenTok, val);
            deleteKey(k);
        } else if (clen == 4 && cmd[0] == 'f') {    // find
            int ilenTok = readToken(idx);
            findKey(idx, ilenTok);
        }
    }

    oflush();
    flushAll();
    fseek(fp, 0, SEEK_SET);
    fwrite(&hdr, sizeof(hdr), 1, fp);
    fflush(fp);
    fclose(fp);
    return 0;
}
