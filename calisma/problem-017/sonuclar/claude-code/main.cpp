#include <cstdio>
#include <cstring>
#include <string>

#include "src/bplustree.hpp"
#include "src/datetime.hpp"
#include "src/records.hpp"
#include "src/storage.hpp"
#include "src/utils.hpp"

static const int INF = 0x7fffffff;

// ---------------------------------------------------------------------------
// Fast line-oriented input.
// ---------------------------------------------------------------------------
class Input {
  static const int BUF = 1 << 16;
  char buf_[BUF];
  int len_ = 0, pos_ = 0;
  int getc_() {
    if (pos_ == len_) {
      len_ = (int)std::fread(buf_, 1, BUF, stdin);
      pos_ = 0;
      if (len_ == 0) return -1;
    }
    return buf_[pos_++];
  }

 public:
  // read one line into `out`; returns false at EOF with empty line
  bool getline(std::string &out) {
    out.clear();
    int c = getc_();
    if (c == -1) return false;
    while (c != -1 && c != '\n') {
      if (c != '\r') out.push_back((char)c);
      c = getc_();
    }
    return true;
  }
};

// ---------------------------------------------------------------------------
// Buffered output.
// ---------------------------------------------------------------------------
class Output {
  std::string buf_;

 public:
  Output() { buf_.reserve(1 << 20); }
  void put(const std::string &s) {
    buf_ += s;
    if (buf_.size() > (1u << 20)) flush();
  }
  void putLine(const std::string &s) {
    buf_ += s;
    buf_.push_back('\n');
    if (buf_.size() > (1u << 20)) flush();
  }
  void putInt(long v) {
    char tmp[24];
    std::snprintf(tmp, sizeof(tmp), "%ld", v);
    buf_ += tmp;
    buf_.push_back('\n');
    if (buf_.size() > (1u << 20)) flush();
  }
  void flush() {
    if (!buf_.empty()) {
      std::fwrite(buf_.data(), 1, buf_.size(), stdout);
      buf_.clear();
    }
    std::fflush(stdout);
  }
};

// ---------------------------------------------------------------------------
// Command argument parsing.
// ---------------------------------------------------------------------------
struct Args {
  bool has[26];
  std::string val[26];
  std::string cmd;
  void parse(const std::string &line) {
    for (int i = 0; i < 26; ++i) has[i] = false;
    cmd.clear();
    int n = (int)line.size();
    int i = 0;
    // first token = command (skip leading timestamp token if present)
    auto readToken = [&](std::string &out) {
      while (i < n && line[i] == ' ') ++i;
      out.clear();
      while (i < n && line[i] != ' ') out.push_back(line[i++]);
    };
    readToken(cmd);
    std::string key, value;
    while (i < n) {
      readToken(key);
      if (key.empty()) break;
      readToken(value);
      if (key.size() >= 2 && key[0] == '-') {
        int idx = key[1] - 'a';
        if (idx >= 0 && idx < 26) {
          has[idx] = true;
          val[idx] = value;
        }
      }
    }
  }
  bool operator()(char c) const { return has[c - 'a']; }
  const std::string &operator[](char c) const { return val[c - 'a']; }
};

// split a '|'-separated field into parts
static void splitPipe(const std::string &s, Vector<std::string> &out) {
  out.clear();
  std::string cur;
  for (int i = 0; i < (int)s.size(); ++i) {
    if (s[i] == '|') {
      out.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(s[i]);
    }
  }
  out.push_back(cur);
}

static int toInt(const std::string &s) {
  int v = 0, i = 0;
  bool neg = false;
  if (i < (int)s.size() && s[i] == '-') { neg = true; ++i; }
  for (; i < (int)s.size(); ++i) v = v * 10 + (s[i] - '0');
  return neg ? -v : v;
}

// ---------------------------------------------------------------------------
// The system.
// ---------------------------------------------------------------------------
class TicketSystem {
  BPlusTree<FixedStr<24>, UserRec, 32, 256> users_;
  BPlusTree<FixedStr<24>, int, 64, 256> trainIdx_;
  MemoryRiver<TrainRec, 2> trainData_;
  RawIntFile seats_;
  BPlusTree<StationKey, StaVal, 40, 512> station_;
  MemoryRiver<OrderRec, 4> orders_;      // info[0] = global ts counter
  BPlusTree<OrderKey, int, 48, 256> orderIdx_;
  BPlusTree<PendingKey, int, 64, 256> pending_;

  LoginTable login_;
  Output out_;

  static FixedStr<24> maxTrainKey() {
    FixedStr<24> k;
    std::memset(k.s, (int)0xFF, 24);
    return k;
  }
  static FixedStr<24> minTrainKey() {
    FixedStr<24> k;  // all zero
    return k;
  }

  int nextTs() {
    int v = orders_.readInfo(0);
    orders_.writeInfo(0, v + 1);
    return v;
  }

 public:
  void open() {
    users_.open("tk_users.db");
    trainIdx_.open("tk_trainidx.db");
    trainData_.open("tk_traindata.db");
    seats_.open("tk_seats.db");
    station_.open("tk_station.db");
    orders_.open("tk_orders.db");
    orderIdx_.open("tk_orderidx.db");
    pending_.open("tk_pending.db");
  }
  void flush() {
    users_.flush();
    trainIdx_.flush();
    trainData_.close();
    seats_.close();
    station_.flush();
    orders_.close();
    orderIdx_.flush();
    pending_.flush();
    out_.flush();
    // reopen rivers/raw so system stays usable after flush (used at exit only)
  }
  void closeAll() {
    users_.close();
    trainIdx_.close();
    trainData_.close();
    seats_.close();
    station_.close();
    orders_.close();
    orderIdx_.close();
    pending_.close();
    out_.flush();
  }

  Output &output() { return out_; }

  // ---- command handlers ---------------------------------------------------
  void cmdAddUser(const Args &a) {
    int userCount = users_.getExtra(0);
    FixedStr<24> uKey(a['u']);
    UserRec rec;
    if (userCount == 0) {
      // first user: privilege 10, ignore -c and -g
      UserRec dummy;
      if (users_.find(uKey, dummy)) { out_.putInt(-1); return; }
      rec.password.assign(a['p']);
      rec.name.assign(a['n']);
      rec.mail.assign(a['m']);
      rec.privilege = 10;
      rec.orderCount = 0;
      users_.insert(uKey, rec);
      users_.setExtra(0, 1);
      out_.putInt(0);
      return;
    }
    FixedStr<24> cKey(a['c']);
    int cPriv = login_.getPriv(cKey);
    if (cPriv < 0) { out_.putInt(-1); return; }
    int g = toInt(a['g']);
    if (!(g < cPriv)) { out_.putInt(-1); return; }
    UserRec exist;
    if (users_.find(uKey, exist)) { out_.putInt(-1); return; }
    rec.password.assign(a['p']);
    rec.name.assign(a['n']);
    rec.mail.assign(a['m']);
    rec.privilege = g;
    rec.orderCount = 0;
    users_.insert(uKey, rec);
    users_.setExtra(0, userCount + 1);
    out_.putInt(0);
  }

  void cmdLogin(const Args &a) {
    FixedStr<24> uKey(a['u']);
    UserRec rec;
    if (!users_.find(uKey, rec)) { out_.putInt(-1); return; }
    if (rec.password != FixedStr<32>(a['p'])) { out_.putInt(-1); return; }
    if (login_.contains(uKey)) { out_.putInt(-1); return; }
    login_.insert(uKey, rec.privilege);
    out_.putInt(0);
  }

  void cmdLogout(const Args &a) {
    FixedStr<24> uKey(a['u']);
    if (!login_.contains(uKey)) { out_.putInt(-1); return; }
    login_.erase(uKey);
    out_.putInt(0);
  }

  void cmdQueryProfile(const Args &a) {
    FixedStr<24> cKey(a['c']);
    FixedStr<24> uKey(a['u']);
    int cPriv = login_.getPriv(cKey);
    if (cPriv < 0) { out_.putInt(-1); return; }
    UserRec u;
    if (!users_.find(uKey, u)) { out_.putInt(-1); return; }
    bool same = (cKey == uKey);
    if (!(same || cPriv > u.privilege)) { out_.putInt(-1); return; }
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s %s %s %d", a['u'].c_str(),
                  u.name.str().c_str(), u.mail.str().c_str(), u.privilege);
    out_.putLine(buf);
  }

  void cmdModifyProfile(const Args &a) {
    FixedStr<24> cKey(a['c']);
    FixedStr<24> uKey(a['u']);
    int cPriv = login_.getPriv(cKey);
    if (cPriv < 0) { out_.putInt(-1); return; }
    UserRec u;
    if (!users_.find(uKey, u)) { out_.putInt(-1); return; }
    bool same = (cKey == uKey);
    if (!(same || cPriv > u.privilege)) { out_.putInt(-1); return; }
    if (a('g')) {
      int g = toInt(a['g']);
      if (!(g < cPriv)) { out_.putInt(-1); return; }
      u.privilege = g;
    }
    if (a('p')) u.password.assign(a['p']);
    if (a('n')) u.name.assign(a['n']);
    if (a('m')) u.mail.assign(a['m']);
    users_.insert(uKey, u);
    if (login_.contains(uKey)) login_.setIfPresent(uKey, u.privilege);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s %s %s %d", a['u'].c_str(),
                  u.name.str().c_str(), u.mail.str().c_str(), u.privilege);
    out_.putLine(buf);
  }

  void cmdAddTrain(const Args &a) {
    FixedStr<24> id(a['i']);
    int existIdx;
    if (trainIdx_.find(id, existIdx)) { out_.putInt(-1); return; }
    TrainRec t;
    std::memset(&t, 0, sizeof(t));
    t.trainID = id;
    t.stationNum = toInt(a['n']);
    t.seatNum = toInt(a['m']);
    t.type = a['y'][0];
    t.released = 0;
    t.startMin = dt::parseTime(a['x']);
    t.seatBase = -1;
    Vector<std::string> stations, prices, travels, stops, sale;
    splitPipe(a['s'], stations);
    splitPipe(a['p'], prices);
    splitPipe(a['t'], travels);
    splitPipe(a['o'], stops);
    splitPipe(a['d'], sale);
    int n = t.stationNum;
    for (int i = 0; i < n; ++i) t.stations[i].assign(stations[i]);
    t.cumPrice[0] = 0;
    for (int i = 1; i < n; ++i) t.cumPrice[i] = t.cumPrice[i - 1] + toInt(prices[i - 1]);
    // travel/stop offsets
    t.leaveOff[0] = 0;
    t.arriveOff[0] = 0;
    for (int i = 1; i < n; ++i) {
      t.arriveOff[i] = t.leaveOff[i - 1] + toInt(travels[i - 1]);
      if (i < n - 1)
        t.leaveOff[i] = t.arriveOff[i] + toInt(stops[i - 1]);
      else
        t.leaveOff[i] = t.arriveOff[i];
    }
    t.saleStart = dt::parseDate(sale[0]);
    t.saleEnd = dt::parseDate(sale[1]);
    t.numDays = t.saleEnd - t.saleStart + 1;
    int idx = trainData_.allocate(t);
    trainIdx_.insert(id, idx);
    out_.putInt(0);
  }

  void cmdReleaseTrain(const Args &a) {
    FixedStr<24> id(a['i']);
    int idx;
    if (!trainIdx_.find(id, idx)) { out_.putInt(-1); return; }
    TrainRec t;
    trainData_.read(idx, t);
    if (t.released) { out_.putInt(-1); return; }
    t.released = 1;
    int segs = t.stationNum - 1;
    long base = seats_.appendFill(t.numDays * segs, t.seatNum);
    t.seatBase = base;
    trainData_.update(idx, t);
    // add station index entries
    for (int i = 0; i < t.stationNum; ++i) {
      StationKey k;
      k.sta = t.stations[i];
      k.tr = id;
      StaVal v;
      v.trainIdx = idx;
      v.pos = i;
      station_.insert(k, v);
    }
    out_.putInt(0);
  }

  void cmdDeleteTrain(const Args &a) {
    FixedStr<24> id(a['i']);
    int idx;
    if (!trainIdx_.find(id, idx)) { out_.putInt(-1); return; }
    TrainRec t;
    trainData_.read(idx, t);
    if (t.released) { out_.putInt(-1); return; }
    trainIdx_.erase(id);
    trainData_.erase(idx);
    out_.putInt(0);
  }

  void cmdQueryTrain(const Args &a) {
    FixedStr<24> id(a['i']);
    int idx;
    if (!trainIdx_.find(id, idx)) { out_.putInt(-1); return; }
    TrainRec t;
    trainData_.read(idx, t);
    int D = dt::parseDate(a['d']);
    if (D < t.saleStart || D > t.saleEnd) { out_.putInt(-1); return; }
    int segs = t.stationNum - 1;
    int seatBuf[MAX_ST];
    if (t.released) {
      seats_.readRange(t.seatBase + (long)(D - t.saleStart) * segs, segs, seatBuf);
    } else {
      for (int i = 0; i < segs; ++i) seatBuf[i] = t.seatNum;
    }
    char head[64];
    std::snprintf(head, sizeof(head), "%s %c", id.str().c_str(), t.type);
    out_.putLine(head);
    long dayBase = (long)D * 1440 + t.startMin;
    for (int i = 0; i < t.stationNum; ++i) {
      std::string arr = (i == 0) ? "xx-xx xx:xx" : dt::formatAbs(dayBase + t.arriveOff[i]);
      std::string lev = (i == t.stationNum - 1) ? "xx-xx xx:xx"
                                                : dt::formatAbs(dayBase + t.leaveOff[i]);
      char line[160];
      if (i == t.stationNum - 1) {
        std::snprintf(line, sizeof(line), "%s %s -> %s %d x", t.stations[i].str().c_str(),
                      arr.c_str(), lev.c_str(), t.cumPrice[i]);
      } else {
        std::snprintf(line, sizeof(line), "%s %s -> %s %d %d", t.stations[i].str().c_str(),
                      arr.c_str(), lev.c_str(), t.cumPrice[i], seatBuf[i]);
      }
      out_.putLine(line);
    }
  }

  // Gather trains passing through station `st`, sorted by trainID.
  struct StaHit {
    FixedStr<24> tr;
    int trainIdx;
    int pos;
  };
  void gatherStation(const std::string &st, Vector<StaHit> &out) {
    out.clear();
    StationKey lo, hi;
    lo.sta.assign(st);
    lo.tr = minTrainKey();
    hi.sta.assign(st);
    hi.tr = maxTrainKey();
    station_.scan(lo, hi, [&](const StationKey &k, const StaVal &v) {
      StaHit h;
      h.tr = k.tr;
      h.trainIdx = v.trainIdx;
      h.pos = v.pos;
      out.push_back(h);
      return true;
    });
  }

  // min seats over segments [from, to) for start-day D
  int minSeat(const TrainRec &t, int D, int from, int to) {
    int segs = t.stationNum - 1;
    int buf[MAX_ST];
    long start = t.seatBase + (long)(D - t.saleStart) * segs + from;
    seats_.readRange(start, to - from, buf);
    int m = INF;
    for (int i = 0; i < to - from; ++i) m = my_min(m, buf[i]);
    return m;
  }

  struct TicketResult {
    FixedStr<24> tr;
    long leaveAbs, arriveAbs;
    int price;
    int seat;
  };

  void cmdQueryTicket(const Args &a) {
    std::string s = a['s'], t = a['t'];
    int d = dt::parseDate(a['d']);
    bool byCost = (a('p') && a['p'] == "cost");
    Vector<StaHit> sl, tl;
    gatherStation(s, sl);
    gatherStation(t, tl);
    Vector<TicketResult> res;
    // merge join on trainID (both sorted ascending by trainID)
    int i = 0, j = 0;
    while (i < sl.size() && j < tl.size()) {
      if (sl[i].tr < tl[j].tr) { ++i; continue; }
      if (tl[j].tr < sl[i].tr) { ++j; continue; }
      // same train
      if (sl[i].pos < tl[j].pos) {
        int idx = sl[i].trainIdx;
        int ps = sl[i].pos, pt = tl[j].pos;
        TrainRec tr;
        trainData_.read(idx, tr);
        int ldo = (tr.startMin + tr.leaveOff[ps]) / 1440;
        int D = d - ldo;
        if (D >= tr.saleStart && D <= tr.saleEnd) {
          TicketResult r;
          r.tr = tr.trainID;
          long dayBase = (long)D * 1440 + tr.startMin;
          r.leaveAbs = dayBase + tr.leaveOff[ps];
          r.arriveAbs = dayBase + tr.arriveOff[pt];
          r.price = tr.cumPrice[pt] - tr.cumPrice[ps];
          r.seat = minSeat(tr, D, ps, pt);
          res.push_back(r);
        }
      }
      ++i;
      ++j;
    }
    if (byCost) {
      sort_vec(res, [](const TicketResult &x, const TicketResult &y) {
        if (x.price != y.price) return x.price < y.price;
        return x.tr < y.tr;
      });
    } else {
      sort_vec(res, [](const TicketResult &x, const TicketResult &y) {
        long tx = x.arriveAbs - x.leaveAbs, ty = y.arriveAbs - y.leaveAbs;
        if (tx != ty) return tx < ty;
        return x.tr < y.tr;
      });
    }
    out_.putInt(res.size());
    for (int k = 0; k < res.size(); ++k) {
      char line[256];
      std::snprintf(line, sizeof(line), "%s %s %s -> %s %s %d %d", res[k].tr.str().c_str(),
                    s.c_str(), dt::formatAbs(res[k].leaveAbs).c_str(), t.c_str(),
                    dt::formatAbs(res[k].arriveAbs).c_str(), res[k].price, res[k].seat);
      out_.putLine(line);
    }
  }

  void cmdQueryTransfer(const Args &a) {
    std::string s = a['s'], tt = a['t'];
    int d = dt::parseDate(a['d']);
    bool byCost = (a('p') && a['p'] == "cost");
    Vector<StaHit> sl, tl;
    gatherStation(s, sl);
    gatherStation(tt, tl);
    // For each train through s (leg1), for each intermediate station x after s,
    // find a leg2 train through x (!= leg1) reaching t after x, departing x no
    // earlier than leg1 arrival. Keep the optimal per the sort key.
    bool found = false;
    // best leg descriptors
    FixedStr<24> b1tr, b2tr;
    std::string bMid;
    long b1leave = 0, b1arrive = 0, b2leave = 0, b2arrive = 0;
    int b1price = 0, b1seat = 0, b2price = 0, b2seat = 0;
    long bestTotalTime = 0;
    int bestTotalCost = 0;
    long bestLeg1Time = 0;

    for (int i = 0; i < sl.size(); ++i) {
      int idx1 = sl[i].trainIdx;
      int ps = sl[i].pos;
      TrainRec t1;
      trainData_.read(idx1, t1);
      int ldo = (t1.startMin + t1.leaveOff[ps]) / 1440;
      int D1 = d - ldo;
      if (D1 < t1.saleStart || D1 > t1.saleEnd) continue;
      long day1Base = (long)D1 * 1440 + t1.startMin;
      long leave1s = day1Base + t1.leaveOff[ps];
      // consider each transfer station x (position px > ps)
      for (int px = ps + 1; px < t1.stationNum; ++px) {
        std::string midName = t1.stations[px].str();
        long arrive1x = day1Base + t1.arriveOff[px];  // absolute arrival at x
        int price1 = t1.cumPrice[px] - t1.cumPrice[ps];
        long time1 = arrive1x - leave1s;
        // gather trains through x
        Vector<StaHit> xl;
        gatherStation(midName, xl);
        // need those also through tt after their x-position
        // tl is sorted by trainID; build merge for membership+pos of tt.
        for (int q = 0; q < xl.size(); ++q) {
          if (xl[q].tr == t1.trainID) continue;  // must differ
          int idx2 = xl[q].trainIdx;
          int px2 = xl[q].pos;
          // find tt position for this train in tl
          int pt2 = -1;
          // binary search tl by trainID
          int lo = 0, hi = tl.size() - 1, foundPos = -1;
          while (lo <= hi) {
            int mid = (lo + hi) >> 1;
            if (tl[mid].tr < xl[q].tr) lo = mid + 1;
            else if (xl[q].tr < tl[mid].tr) hi = mid - 1;
            else { foundPos = mid; break; }
          }
          if (foundPos < 0) continue;
          pt2 = tl[foundPos].pos;
          if (px2 >= pt2) continue;  // must go x -> tt forward
          TrainRec t2;
          trainData_.read(idx2, t2);
          // earliest feasible departure day D2 at x so that leave2x >= arrive1x
          int ldo2 = (t2.startMin + t2.leaveOff[px2]) / 1440;
          // leave time at x for start-day D2 = D2*1440 + startMin + leaveOff[px2]
          // = (D2 + ldo2day...) easier: leave2x(D2) = (long)D2*1440 + t2.startMin + t2.leaveOff[px2]
          // find min D2 in [saleStart,saleEnd] with leave2x(D2) >= arrive1x
          long fixed = (long)t2.startMin + t2.leaveOff[px2];
          // D2*1440 + fixed >= arrive1x  => D2 >= (arrive1x - fixed)/1440 (ceil)
          long need = arrive1x - fixed;
          int D2;
          if (need <= (long)t2.saleStart * 1440) {
            D2 = t2.saleStart;
          } else {
            long q2 = need / 1440;
            if (q2 * 1440 < need) q2 += 1;  // ceil
            D2 = (int)q2;
          }
          if (D2 < t2.saleStart) D2 = t2.saleStart;
          if (D2 > t2.saleEnd) continue;  // no feasible day
          long day2Base = (long)D2 * 1440 + t2.startMin;
          long leave2x = day2Base + t2.leaveOff[px2];
          long arrive2t = day2Base + t2.arriveOff[pt2];
          int price2 = t2.cumPrice[pt2] - t2.cumPrice[px2];
          long totalTime = arrive2t - leave1s;
          int totalCost = price1 + price2;
          long time2 = arrive2t - leave2x;

          // Ordering (README Q&A): primary metric, then less riding time on
          // train 1, then the other metric, then trainID1, then trainID2.
          bool better;
          if (!found) {
            better = true;
          } else {
            long primNew = byCost ? (long)totalCost : totalTime;
            long primBest = byCost ? (long)bestTotalCost : bestTotalTime;
            long secNew = byCost ? totalTime : (long)totalCost;
            long secBest = byCost ? bestTotalTime : (long)bestTotalCost;
            if (primNew != primBest) better = primNew < primBest;
            else if (time1 != bestLeg1Time) better = time1 < bestLeg1Time;
            else if (secNew != secBest) better = secNew < secBest;
            else if (t1.trainID != b1tr) better = t1.trainID < b1tr;
            else better = t2.trainID < b2tr;
          }
          if (better) {
            found = true;
            bestTotalTime = totalTime;
            bestTotalCost = totalCost;
            bestLeg1Time = time1;
            b1tr = t1.trainID;
            b2tr = t2.trainID;
            bMid = midName;
            b1leave = leave1s;
            b1arrive = arrive1x;
            b1price = price1;
            b1seat = minSeat(t1, D1, ps, px);
            b2leave = leave2x;
            b2arrive = arrive2t;
            b2price = price2;
            b2seat = minSeat(t2, D2, px2, pt2);
          }
        }
      }
    }
    if (!found) { out_.putInt(0); return; }
    char l1[256], l2[256];
    std::snprintf(l1, sizeof(l1), "%s %s %s -> %s %s %d %d", b1tr.str().c_str(), s.c_str(),
                  dt::formatAbs(b1leave).c_str(), bMid.c_str(),
                  dt::formatAbs(b1arrive).c_str(), b1price, b1seat);
    std::snprintf(l2, sizeof(l2), "%s %s %s -> %s %s %d %d", b2tr.str().c_str(), bMid.c_str(),
                  dt::formatAbs(b2leave).c_str(), tt.c_str(),
                  dt::formatAbs(b2arrive).c_str(), b2price, b2seat);
    out_.putLine(l1);
    out_.putLine(l2);
  }

  void cmdBuyTicket(const Args &a) {
    FixedStr<24> uKey(a['u']);
    if (!login_.contains(uKey)) { out_.putInt(-1); return; }
    FixedStr<24> id(a['i']);
    int idx;
    if (!trainIdx_.find(id, idx)) { out_.putInt(-1); return; }
    TrainRec t;
    trainData_.read(idx, t);
    if (!t.released) { out_.putInt(-1); return; }
    std::string from = a['f'], to = a['t'];
    int ps = -1, pt = -1;
    for (int i = 0; i < t.stationNum; ++i) {
      if (ps < 0 && t.stations[i].str() == from) ps = i;
      if (pt < 0 && t.stations[i].str() == to) pt = i;
    }
    if (ps < 0 || pt < 0 || ps >= pt) { out_.putInt(-1); return; }
    int num = toInt(a['n']);
    if (num > t.seatNum) { out_.putInt(-1); return; }
    int d = dt::parseDate(a['d']);
    int ldo = (t.startMin + t.leaveOff[ps]) / 1440;
    int D = d - ldo;
    if (D < t.saleStart || D > t.saleEnd) { out_.putInt(-1); return; }
    bool queueOk = (a('q') && a['q'] == "true");
    int segs = t.stationNum - 1;
    int buf[MAX_ST];
    long start = t.seatBase + (long)(D - t.saleStart) * segs + ps;
    seats_.readRange(start, pt - ps, buf);
    int m = INF;
    for (int i = 0; i < pt - ps; ++i) m = my_min(m, buf[i]);
    long dayBase = (long)D * 1440 + t.startMin;
    int price = t.cumPrice[pt] - t.cumPrice[ps];
    int ts = nextTs();

    UserRec ur;
    users_.find(uKey, ur);
    int localId = ur.orderCount++;

    OrderRec o;
    std::memset(&o, 0, sizeof(o));
    o.trainIdx = idx;
    o.trainID = id;
    o.fromSta = t.stations[ps];
    o.toSta = t.stations[pt];
    o.fromPos = ps;
    o.toPos = pt;
    o.startDay = D;
    o.leaveAbs = dayBase + t.leaveOff[ps];
    o.arriveAbs = dayBase + t.arriveOff[pt];
    o.price = price;
    o.num = num;
    o.ts = ts;
    o.user = uKey;

    if (m >= num) {
      // buy immediately
      for (int i = 0; i < pt - ps; ++i) buf[i] -= num;
      seats_.writeRange(start, pt - ps, buf);
      o.status = 0;
      int oidx = orders_.allocate(o);
      OrderKey ok;
      ok.user = uKey;
      ok.localId = localId;
      orderIdx_.insert(ok, oidx);
      users_.insert(uKey, ur);
      out_.putInt((long)price * num);
    } else if (queueOk) {
      o.status = 1;
      int oidx = orders_.allocate(o);
      OrderKey ok;
      ok.user = uKey;
      ok.localId = localId;
      orderIdx_.insert(ok, oidx);
      PendingKey pk;
      pk.trainIdx = idx;
      pk.day = D;
      pk.ts = ts;
      pending_.insert(pk, oidx);
      users_.insert(uKey, ur);
      out_.putLine("queue");
    } else {
      out_.putInt(-1);
    }
  }

  void cmdQueryOrder(const Args &a) {
    FixedStr<24> uKey(a['u']);
    if (!login_.contains(uKey)) { out_.putInt(-1); return; }
    OrderKey lo, hi;
    lo.user = uKey;
    lo.localId = 0;
    hi.user = uKey;
    hi.localId = INF;
    Vector<int> oidxs;
    orderIdx_.scan(lo, hi, [&](const OrderKey &, const int &v) {
      oidxs.push_back(v);
      return true;
    });
    out_.putInt(oidxs.size());
    // newest first = descending localId = reverse of ascending scan
    for (int i = oidxs.size() - 1; i >= 0; --i) {
      OrderRec o;
      orders_.read(oidxs[i], o);
      const char *st = o.status == 0 ? "success" : (o.status == 1 ? "pending" : "refunded");
      char line[320];
      std::snprintf(line, sizeof(line), "[%s] %s %s %s -> %s %s %d %d", st,
                    o.trainID.str().c_str(), o.fromSta.str().c_str(),
                    dt::formatAbs(o.leaveAbs).c_str(), o.toSta.str().c_str(),
                    dt::formatAbs(o.arriveAbs).c_str(), o.price, o.num);
      out_.putLine(line);
    }
  }

  void cmdRefund(const Args &a) {
    FixedStr<24> uKey(a['u']);
    if (!login_.contains(uKey)) { out_.putInt(-1); return; }
    int n = a('n') ? toInt(a['n']) : 1;
    OrderKey lo, hi;
    lo.user = uKey;
    lo.localId = 0;
    hi.user = uKey;
    hi.localId = INF;
    Vector<int> oidxs;
    orderIdx_.scan(lo, hi, [&](const OrderKey &, const int &v) {
      oidxs.push_back(v);
      return true;
    });
    int c = oidxs.size();
    if (n < 1 || n > c) { out_.putInt(-1); return; }
    int target = oidxs[c - n];  // n-th newest
    OrderRec o;
    orders_.read(target, o);
    if (o.status == 2) { out_.putInt(-1); return; }

    if (o.status == 1) {
      // pending -> just remove from queue
      o.status = 2;
      orders_.update(target, o);
      PendingKey pk;
      pk.trainIdx = o.trainIdx;
      pk.day = o.startDay;
      pk.ts = o.ts;
      pending_.erase(pk);
      out_.putInt(0);
      return;
    }

    // success -> return seats, then try to satisfy pending queue
    o.status = 2;
    orders_.update(target, o);
    TrainRec t;
    trainData_.read(o.trainIdx, t);
    int segs = t.stationNum - 1;
    int buf[MAX_ST];
    long start = t.seatBase + (long)(o.startDay - t.saleStart) * segs + o.fromPos;
    int len = o.toPos - o.fromPos;
    seats_.readRange(start, len, buf);
    for (int i = 0; i < len; ++i) buf[i] += o.num;
    seats_.writeRange(start, len, buf);

    // scan pending for (trainIdx, day) in ts order
    PendingKey plo, phi;
    plo.trainIdx = o.trainIdx;
    plo.day = o.startDay;
    plo.ts = 0;
    phi.trainIdx = o.trainIdx;
    phi.day = o.startDay;
    phi.ts = INF;
    Vector<int> pendIdx;
    pending_.scan(plo, phi, [&](const PendingKey &, const int &v) {
      pendIdx.push_back(v);
      return true;
    });
    for (int i = 0; i < pendIdx.size(); ++i) {
      OrderRec po;
      orders_.read(pendIdx[i], po);
      if (po.status != 1) continue;
      int plen = po.toPos - po.fromPos;
      long pstart = t.seatBase + (long)(po.startDay - t.saleStart) * segs + po.fromPos;
      int pbuf[MAX_ST];
      seats_.readRange(pstart, plen, pbuf);
      int mm = INF;
      for (int j = 0; j < plen; ++j) mm = my_min(mm, pbuf[j]);
      if (mm >= po.num) {
        for (int j = 0; j < plen; ++j) pbuf[j] -= po.num;
        seats_.writeRange(pstart, plen, pbuf);
        po.status = 0;
        orders_.update(pendIdx[i], po);
        PendingKey pk;
        pk.trainIdx = po.trainIdx;
        pk.day = po.startDay;
        pk.ts = po.ts;
        pending_.erase(pk);
      }
    }
    out_.putInt(0);
  }

  void cmdClean() {
    users_.reset();
    trainIdx_.reset();
    trainData_.reset();
    seats_.reset();
    station_.reset();
    orders_.reset();
    orderIdx_.reset();
    pending_.reset();
    login_.clear();
    out_.putInt(0);
  }

  // returns false if this was the exit command
  bool dispatch(const Args &a) {
    const std::string &c = a.cmd;
    if (c == "add_user") cmdAddUser(a);
    else if (c == "login") cmdLogin(a);
    else if (c == "logout") cmdLogout(a);
    else if (c == "query_profile") cmdQueryProfile(a);
    else if (c == "modify_profile") cmdModifyProfile(a);
    else if (c == "add_train") cmdAddTrain(a);
    else if (c == "release_train") cmdReleaseTrain(a);
    else if (c == "query_train") cmdQueryTrain(a);
    else if (c == "delete_train") cmdDeleteTrain(a);
    else if (c == "query_ticket") cmdQueryTicket(a);
    else if (c == "query_transfer") cmdQueryTransfer(a);
    else if (c == "buy_ticket") cmdBuyTicket(a);
    else if (c == "query_order") cmdQueryOrder(a);
    else if (c == "refund_ticket") cmdRefund(a);
    else if (c == "clean") cmdClean();
    else if (c == "exit") {
      out_.putLine("bye");
      return false;
    }
    return true;
  }
};

int main() {
  TicketSystem *sys = new TicketSystem();
  sys->open();
  Input in;
  Args args;
  std::string line;
  while (in.getline(line)) {
    if (line.empty()) continue;
    args.parse(line);
    if (args.cmd.empty()) continue;
    if (!sys->dispatch(args)) break;
  }
  sys->closeAll();
  delete sys;
  return 0;
}
