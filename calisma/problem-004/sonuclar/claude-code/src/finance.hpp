#ifndef BOOKSTORE_FINANCE_HPP
#define BOOKSTORE_FINANCE_HPP

#include <fstream>
#include <string>

#include "types.hpp"

// Append-only financial log. For every completed transaction we store the
// cumulative income and expenditure (in cents) up to and including that
// transaction. Querying the last k transactions is then O(1): subtract the
// cumulative totals recorded (n-k) transactions ago from the current totals.
//
// File layout: [int count][record 1][record 2] ... where each record is two
// Int128 values (cumulative income, cumulative expenditure).
class Finance {
    struct Record {
        Int128 income;
        Int128 expend;
    };
    std::string  path;
    std::fstream file;
    int          count = 0;
    Int128     totalIncome = 0;
    Int128     totalExpend = 0;

    static constexpr long HEADER = sizeof(int);
    static constexpr long REC    = sizeof(Record);

    Record readRecord(int idx) {  // 1-indexed; idx 0 -> all zeros
        Record r{0, 0};
        if (idx <= 0) return r;
        file.clear();
        file.seekg(HEADER + (std::streamoff)(idx - 1) * REC, std::ios::beg);
        file.read(reinterpret_cast<char *>(&r), REC);
        return r;
    }

    void writeHeader() {
        file.clear();
        file.seekp(0, std::ios::beg);
        file.write(reinterpret_cast<const char *>(&count), sizeof(count));
    }

  public:
    void open(const std::string &name) {
        path = name + ".dat";
        file.open(path, std::ios::in | std::ios::out | std::ios::binary);
        if (!file.is_open()) {
            file.clear();
            file.open(path, std::ios::out | std::ios::binary);
            file.close();
            file.open(path, std::ios::in | std::ios::out | std::ios::binary);
            count = 0;
            writeHeader();
        } else {
            file.seekg(0, std::ios::beg);
            file.read(reinterpret_cast<char *>(&count), sizeof(count));
            if (count > 0) {
                Record last = readRecord(count);
                totalIncome = last.income;
                totalExpend = last.expend;
            }
        }
    }

    // Record one transaction (income and/or expenditure delta in cents).
    void record(Int128 incomeDelta, Int128 expendDelta) {
        totalIncome += incomeDelta;
        totalExpend += expendDelta;
        Record r{totalIncome, totalExpend};
        file.clear();
        file.seekp(HEADER + (std::streamoff)count * REC, std::ios::beg);
        file.write(reinterpret_cast<const char *>(&r), REC);
        ++count;
        writeHeader();
    }

    int total() const { return count; }

    // Sum of the last k transactions (k < 0 means "all").
    void sumLast(long long k, Int128 &income, Int128 &expend) {
        if (k < 0 || k >= count) {
            income = totalIncome;
            expend = totalExpend;
            return;
        }
        Record base = readRecord(count - (int)k);
        income = totalIncome - base.income;
        expend = totalExpend - base.expend;
    }
};

#endif  // BOOKSTORE_FINANCE_HPP
