#ifndef BOOKSTORE_RECORDSTORE_HPP
#define BOOKSTORE_RECORDSTORE_HPP

#include <fstream>
#include <string>
#include <vector>

// A flat file of fixed-size records addressed by an integer id. Records live on
// disk and are read/written individually on demand; only a small free-id list
// is kept in memory. Pairing this with a BlockList that maps key -> id keeps the
// ordered index entries tiny, so the expensive block shuffles move only a key
// and an id instead of a whole record. Bulk fields (name, author, ...) and
// in-place mutations (stock, price) never disturb the ordered index.
template <class T>
class RecordStore {
    std::string        dataPath;
    std::string        idxPath;
    std::fstream       file;
    int                count = 0;   // number of record slots ever allocated
    std::vector<int>   freeList;    // reusable record ids

    void openData() {
        file.open(dataPath, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            file.clear();
            file.open(dataPath, std::ios::out | std::ios::binary);
            file.close();
            file.open(dataPath, std::ios::in | std::ios::out | std::ios::binary);
        }
    }

  public:
    bool open(const std::string &name) {
        dataPath = name + ".dat";
        idxPath  = name + ".idx";
        bool fresh;
        {
            std::ifstream probe(idxPath, std::ios::binary);
            fresh = !probe.is_open();
        }
        openData();
        if (!fresh) {
            std::ifstream in(idxPath, std::ios::binary);
            in.read(reinterpret_cast<char *>(&count), sizeof(count));
            int freeCnt = 0;
            in.read(reinterpret_cast<char *>(&freeCnt), sizeof(freeCnt));
            freeList.resize(freeCnt);
            if (freeCnt) in.read(reinterpret_cast<char *>(freeList.data()),
                                 (std::streamsize)freeCnt * sizeof(int));
        }
        return fresh;
    }

    void save() {
        std::ofstream out(idxPath, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char *>(&count), sizeof(count));
        int freeCnt = (int)freeList.size();
        out.write(reinterpret_cast<const char *>(&freeCnt), sizeof(freeCnt));
        if (freeCnt) out.write(reinterpret_cast<const char *>(freeList.data()),
                               (std::streamsize)freeCnt * sizeof(int));
    }

    ~RecordStore() {
        if (!idxPath.empty()) save();
        if (file.is_open()) file.close();
    }

    void read(int id, T &out) {
        file.clear();
        file.seekg((std::streamoff)id * (std::streamoff)sizeof(T), std::ios::beg);
        file.read(reinterpret_cast<char *>(&out), sizeof(T));
    }

    void write(int id, const T &v) {
        file.clear();
        file.seekp((std::streamoff)id * (std::streamoff)sizeof(T), std::ios::beg);
        file.write(reinterpret_cast<const char *>(&v), sizeof(T));
    }

    int append(const T &v) {
        int id;
        if (!freeList.empty()) {
            id = freeList.back();
            freeList.pop_back();
        } else {
            id = count++;
        }
        write(id, v);
        return id;
    }

    void remove(int id) { freeList.push_back(id); }
};

#endif  // BOOKSTORE_RECORDSTORE_HPP
