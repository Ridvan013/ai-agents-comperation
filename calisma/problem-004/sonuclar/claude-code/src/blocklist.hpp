#ifndef BOOKSTORE_BLOCKLIST_HPP
#define BOOKSTORE_BLOCKLIST_HPP

#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

// Disk-backed unrolled linked list (block list / sqrt-decomposition).
//
// The bulk of the data (the entries themselves) lives on disk in fixed-size
// blocks and is read/written on demand. Only a small block-index header table
// (one entry per block, ~ sqrt(N) of them) is kept in memory. This satisfies
// the requirement that main data is not stored in memory and is read/written
// from files in real time.
//
// Requirements on T:
//   * POD / trivially copyable (written to disk with raw bytes).
//   * total order via operator<. Two elements a, b are considered equal iff
//     !(a < b) && !(b < a). For "unique key" tables (accounts, books) the
//     ordering only depends on the key, so payload changes keep ordering.
template <class T, int BLOCK_MAX = 64>
class BlockList {
    static_assert(BLOCK_MAX >= 8, "block capacity too small");

    struct Header {
        int   slot;    // physical block slot in the data file
        int   count;   // number of live entries in the block
        T     maxKey;  // largest entry in the block (data[count-1])
    };

    std::string        dataPath;
    std::string        idxPath;
    std::fstream       dataFile;
    std::vector<Header> headers;   // sorted by maxKey, non-overlapping ranges
    std::vector<int>   freeSlots;  // reusable block slots
    int                totalSlots = 0;
    std::vector<T>     buf;        // scratch buffer for a single block

    static constexpr long blockBytes() { return (long)BLOCK_MAX * (long)sizeof(T); }

    static bool eq(const T &a, const T &b) { return !(a < b) && !(b < a); }

    void openData() {
        dataFile.open(dataPath, std::ios::in | std::ios::out | std::ios::binary);
        if (!dataFile.is_open()) {
            dataFile.clear();
            dataFile.open(dataPath, std::ios::out | std::ios::binary);
            dataFile.close();
            dataFile.open(dataPath, std::ios::in | std::ios::out | std::ios::binary);
        }
    }

    void readBlock(int slot, int count) {
        dataFile.clear();
        dataFile.seekg((std::streamoff)slot * blockBytes(), std::ios::beg);
        dataFile.read(reinterpret_cast<char *>(buf.data()),
                      (std::streamsize)count * (std::streamsize)sizeof(T));
    }

    void writeBlock(int slot, int count) {
        dataFile.clear();
        dataFile.seekp((std::streamoff)slot * blockBytes(), std::ios::beg);
        dataFile.write(reinterpret_cast<const char *>(buf.data()),
                       (std::streamsize)count * (std::streamsize)sizeof(T));
        // No explicit flush: every read/write is preceded by a seek, which is
        // the intervening positioning operation the standard requires when
        // alternating input and output on one stream. Flushing per write would
        // force buffer drains on the hot path and cripple throughput.
    }

    int allocSlot() {
        if (!freeSlots.empty()) {
            int s = freeSlots.back();
            freeSlots.pop_back();
            return s;
        }
        return totalSlots++;
    }

    // first header index whose maxKey is >= v; headers.size() if none
    int headerLowerBound(const T &v) const {
        int lo = 0, hi = (int)headers.size();
        while (lo < hi) {
            int mid = (lo + hi) >> 1;
            if (headers[mid].maxKey < v)
                lo = mid + 1;
            else
                hi = mid;
        }
        return lo;
    }

    // first position in buf[0,count) whose entry is >= v
    static int blockLowerBound(const std::vector<T> &b, int count, const T &v) {
        int lo = 0, hi = count;
        while (lo < hi) {
            int mid = (lo + hi) >> 1;
            if (b[mid] < v)
                lo = mid + 1;
            else
                hi = mid;
        }
        return lo;
    }

    void loadIndex() {
        std::ifstream in(idxPath, std::ios::binary);
        if (!in.is_open()) return;
        in.read(reinterpret_cast<char *>(&totalSlots), sizeof(totalSlots));
        int freeCnt = 0;
        in.read(reinterpret_cast<char *>(&freeCnt), sizeof(freeCnt));
        freeSlots.resize(freeCnt);
        if (freeCnt) in.read(reinterpret_cast<char *>(freeSlots.data()),
                             (std::streamsize)freeCnt * sizeof(int));
        int hdrCnt = 0;
        in.read(reinterpret_cast<char *>(&hdrCnt), sizeof(hdrCnt));
        headers.resize(hdrCnt);
        if (hdrCnt) in.read(reinterpret_cast<char *>(headers.data()),
                            (std::streamsize)hdrCnt * sizeof(Header));
    }

  public:
    BlockList() : buf(BLOCK_MAX + 2) {}
    BlockList(const BlockList &) = delete;
    BlockList &operator=(const BlockList &) = delete;

    // Returns true if this table was freshly created (no index file existed).
    bool open(const std::string &name) {
        dataPath = name + ".dat";
        idxPath  = name + ".idx";
        bool fresh;
        {
            std::ifstream probe(idxPath, std::ios::binary);
            fresh = !probe.is_open();
        }
        openData();
        if (!fresh) loadIndex();
        return fresh;
    }

    void save() {
        std::ofstream out(idxPath, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char *>(&totalSlots), sizeof(totalSlots));
        int freeCnt = (int)freeSlots.size();
        out.write(reinterpret_cast<const char *>(&freeCnt), sizeof(freeCnt));
        if (freeCnt) out.write(reinterpret_cast<const char *>(freeSlots.data()),
                               (std::streamsize)freeCnt * sizeof(int));
        int hdrCnt = (int)headers.size();
        out.write(reinterpret_cast<const char *>(&hdrCnt), sizeof(hdrCnt));
        if (hdrCnt) out.write(reinterpret_cast<const char *>(headers.data()),
                              (std::streamsize)hdrCnt * sizeof(Header));
    }

    ~BlockList() {
        if (!idxPath.empty()) save();
        if (dataFile.is_open()) dataFile.close();
    }

    // Insert v. Returns false if an equal element already exists.
    bool insert(const T &v) {
        if (headers.empty()) {
            int slot = allocSlot();
            buf[0] = v;
            writeBlock(slot, 1);
            headers.push_back({slot, 1, v});
            return true;
        }
        int hi = headerLowerBound(v);
        if (hi == (int)headers.size()) hi = (int)headers.size() - 1;
        Header &h = headers[hi];
        readBlock(h.slot, h.count);
        int pos = blockLowerBound(buf, h.count, v);
        if (pos < h.count && eq(buf[pos], v)) return false;  // duplicate
        for (int i = h.count; i > pos; --i) buf[i] = buf[i - 1];
        buf[pos] = v;
        int newCount = h.count + 1;
        if (newCount <= BLOCK_MAX) {
            h.count  = newCount;
            h.maxKey = buf[newCount - 1];
            writeBlock(h.slot, newCount);
            return true;
        }
        // split into two blocks
        int leftCount  = newCount / 2;
        int rightCount = newCount - leftCount;
        writeBlock(h.slot, leftCount);
        h.count  = leftCount;
        h.maxKey = buf[leftCount - 1];
        int rightSlot = allocSlot();
        for (int i = 0; i < rightCount; ++i) buf[i] = buf[leftCount + i];
        writeBlock(rightSlot, rightCount);
        Header rh{rightSlot, rightCount, buf[rightCount - 1]};
        headers.insert(headers.begin() + hi + 1, rh);
        return true;
    }

    // Remove the element equal to v. Returns false if not present.
    bool erase(const T &v) {
        int hi = headerLowerBound(v);
        if (hi == (int)headers.size()) return false;
        Header &h = headers[hi];
        readBlock(h.slot, h.count);
        int pos = blockLowerBound(buf, h.count, v);
        if (pos >= h.count || !eq(buf[pos], v)) return false;
        for (int i = pos; i + 1 < h.count; ++i) buf[i] = buf[i + 1];
        int newCount = h.count - 1;
        if (newCount == 0) {
            freeSlots.push_back(h.slot);
            headers.erase(headers.begin() + hi);
        } else {
            h.count  = newCount;
            h.maxKey = buf[newCount - 1];
            writeBlock(h.slot, newCount);
        }
        return true;
    }

    // Find the element equal to probe (by ordering). Returns true and fills out.
    bool find(const T &probe, T &out) {
        int hi = headerLowerBound(probe);
        if (hi == (int)headers.size()) return false;
        Header &h = headers[hi];
        readBlock(h.slot, h.count);
        int pos = blockLowerBound(buf, h.count, probe);
        if (pos < h.count && eq(buf[pos], probe)) {
            out = buf[pos];
            return true;
        }
        return false;
    }

    // Overwrite the element equal to v with v (payload update, key unchanged).
    bool update(const T &v) {
        int hi = headerLowerBound(v);
        if (hi == (int)headers.size()) return false;
        Header &h = headers[hi];
        readBlock(h.slot, h.count);
        int pos = blockLowerBound(buf, h.count, v);
        if (pos < h.count && eq(buf[pos], v)) {
            buf[pos] = v;
            writeBlock(h.slot, h.count);
            return true;
        }
        return false;
    }

    // Iterate entries in ascending order starting from the first entry >= low.
    // cb returns false to stop iteration early.
    void forEachFrom(const T &low, const std::function<bool(const T &)> &cb) {
        int hi = headerLowerBound(low);
        for (int bi = hi; bi < (int)headers.size(); ++bi) {
            Header h = headers[bi];
            readBlock(h.slot, h.count);
            int start = (bi == hi) ? blockLowerBound(buf, h.count, low) : 0;
            for (int j = start; j < h.count; ++j)
                if (!cb(buf[j])) return;
        }
    }
};

#endif  // BOOKSTORE_BLOCKLIST_HPP
