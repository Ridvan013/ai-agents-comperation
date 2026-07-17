#ifndef TICKET_RECORDS_HPP
#define TICKET_RECORDS_HPP

#include "utils.hpp"

static const int MAX_ST = 100;  // max stations per train

// ---- User -----------------------------------------------------------------
struct UserRec {
  FixedStr<32> password;
  FixedStr<20> name;
  FixedStr<32> mail;
  int privilege;
  int orderCount;  // per-user monotonically increasing order id
};

// ---- Train ----------------------------------------------------------------
struct TrainRec {
  FixedStr<24> trainID;
  int stationNum;
  int seatNum;
  char type;
  char released;
  int startMin;             // start time as minute of day
  int saleStart, saleEnd;   // day indices (departure at starting station)
  int numDays;
  long seatBase;            // start index into seat file, -1 if unreleased
  FixedStr<40> stations[MAX_ST];
  int cumPrice[MAX_ST];     // cumulative price at station i (cumPrice[0]=0)
  int arriveOff[MAX_ST];    // minutes from start-departure to arrival at i
  int leaveOff[MAX_ST];     // minutes from start-departure to leaving i
};

// ---- Order ----------------------------------------------------------------
struct OrderRec {
  char status;   // 0 success, 1 pending, 2 refunded
  int trainIdx;  // train river index
  FixedStr<24> trainID;
  FixedStr<40> fromSta;
  FixedStr<40> toSta;
  int fromPos, toPos;
  int startDay;             // D: departure day at starting station
  long leaveAbs, arriveAbs; // absolute minutes
  int price;                // per-ticket price (from -> to)
  int num;
  int ts;                   // global order timestamp
  FixedStr<24> user;
};

// ---- Composite index keys -------------------------------------------------
struct StationKey {
  FixedStr<40> sta;
  FixedStr<24> tr;
  bool operator<(const StationKey &o) const {
    if (sta != o.sta) return sta < o.sta;
    return tr < o.tr;
  }
  bool operator==(const StationKey &o) const { return sta == o.sta && tr == o.tr; }
};
struct StaVal {
  int trainIdx;
  int pos;
};

struct OrderKey {
  FixedStr<24> user;
  int localId;
  bool operator<(const OrderKey &o) const {
    if (user != o.user) return user < o.user;
    return localId < o.localId;
  }
  bool operator==(const OrderKey &o) const {
    return user == o.user && localId == o.localId;
  }
};

struct PendingKey {
  int trainIdx;
  int day;
  int ts;
  bool operator<(const PendingKey &o) const {
    if (trainIdx != o.trainIdx) return trainIdx < o.trainIdx;
    if (day != o.day) return day < o.day;
    return ts < o.ts;
  }
  bool operator==(const PendingKey &o) const {
    return trainIdx == o.trainIdx && day == o.day && ts == o.ts;
  }
};

// ---- In-memory login table (open addressing, session-only) ----------------
class LoginTable {
  static const int CAP = 40009;
  FixedStr<24> keys_[CAP];
  int priv_[CAP];
  bool used_[CAP];

  unsigned hashKey(const FixedStr<24> &k) const {
    unsigned h = 2166136261u;
    for (int i = 0; i < 24; ++i) h = (h ^ (unsigned char)k.s[i]) * 16777619u;
    return h % CAP;
  }

 public:
  LoginTable() {
    for (int i = 0; i < CAP; ++i) used_[i] = false;
  }
  void clear() {
    for (int i = 0; i < CAP; ++i) used_[i] = false;
  }
  // returns privilege or -1 if not logged in
  int getPriv(const FixedStr<24> &k) const {
    int i = hashKey(k);
    while (used_[i]) {
      if (keys_[i] == k) return priv_[i];
      i = (i + 1) % CAP;
    }
    return -1;
  }
  bool contains(const FixedStr<24> &k) const { return getPriv(k) >= 0; }
  void insert(const FixedStr<24> &k, int priv) {
    int i = hashKey(k);
    while (used_[i]) {
      if (keys_[i] == k) {
        priv_[i] = priv;
        return;
      }
      i = (i + 1) % CAP;
    }
    used_[i] = true;
    keys_[i] = k;
    priv_[i] = priv;
  }
  void setIfPresent(const FixedStr<24> &k, int priv) {
    int i = hashKey(k);
    while (used_[i]) {
      if (keys_[i] == k) {
        priv_[i] = priv;
        return;
      }
      i = (i + 1) % CAP;
    }
  }
  // erase with backward-shift to keep probe chains valid
  void erase(const FixedStr<24> &k) {
    int i = hashKey(k);
    while (used_[i] && !(keys_[i] == k)) i = (i + 1) % CAP;
    if (!used_[i]) return;
    used_[i] = false;
    int j = (i + 1) % CAP;
    while (used_[j]) {
      int home = hashKey(keys_[j]);
      // if keys_[j] can move back into slot i, shift it
      bool canShift;
      if (i <= j) canShift = !(home > i && home <= j);
      else canShift = !(home > i || home <= j);
      if (canShift) {
        keys_[i] = keys_[j];
        priv_[i] = priv_[j];
        used_[i] = true;
        used_[j] = false;
        i = j;
      }
      j = (j + 1) % CAP;
    }
  }
};

#endif
