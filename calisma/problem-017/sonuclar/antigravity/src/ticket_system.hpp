#ifndef TICKET_SYSTEM_HPP
#define TICKET_SYSTEM_HPP

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include "hashmap.hpp"
#include "utils.hpp"
#include "vector.hpp"

static const int kMaxSta = 100;
static const int kMaxDays = 92;
static const int kNameLen = 32;
static const int kIdLen = 24;
static const int kMailLen = 32;
static const int kPassLen = 32;
static const int kStaLen = 40;

struct User {
  char username[kIdLen];
  char password[kPassLen];
  char name[kNameLen];
  char mail[kMailLen];
  int privilege;
  int order_cnt;  // number of orders
  User() {
    username[0] = password[0] = name[0] = mail[0] = 0;
    privilege = 0;
    order_cnt = 0;
  }
};

struct Train {
  char trainID[kIdLen];
  int stationNum;
  int seatNum;
  char stations[kMaxSta][kStaLen];
  int prices[kMaxSta];       // prices[i] = price station i -> i+1
  int startTime;             // minutes from midnight
  int travelTimes[kMaxSta];  // i -> i+1
  int stopoverTimes[kMaxSta];
  int saleBegin;  // day id
  int saleEnd;
  char type;
  bool released;
  bool deleted;
  // Precomputed: leave_offset[i] = minutes from start departure to leaving station i
  // arrive_offset[i] = minutes from start departure to arriving at station i
  int leave_offset[kMaxSta];
  int arrive_offset[kMaxSta];
  int prefix_price[kMaxSta];  // price from station 0 to i

  Train() {
    trainID[0] = 0;
    stationNum = seatNum = 0;
    startTime = 0;
    saleBegin = saleEnd = 0;
    type = 'G';
    released = false;
    deleted = false;
    memset(prices, 0, sizeof(prices));
    memset(travelTimes, 0, sizeof(travelTimes));
    memset(stopoverTimes, 0, sizeof(stopoverTimes));
    memset(leave_offset, 0, sizeof(leave_offset));
    memset(arrive_offset, 0, sizeof(arrive_offset));
    memset(prefix_price, 0, sizeof(prefix_price));
    for (int i = 0; i < kMaxSta; ++i) stations[i][0] = 0;
  }

  void compute_offsets() {
    leave_offset[0] = 0;
    arrive_offset[0] = 0;
    prefix_price[0] = 0;
    int t = 0;
    for (int i = 0; i + 1 < stationNum; ++i) {
      t += travelTimes[i];
      arrive_offset[i + 1] = t;
      if (i + 1 < stationNum - 1) {
        t += stopoverTimes[i];  // stopover at station i+1
      }
      leave_offset[i + 1] = t;
      prefix_price[i + 1] = prefix_price[i] + prices[i];
    }
  }

  int find_station(const char *name) const {
    for (int i = 0; i < stationNum; ++i) {
      if (std::strcmp(stations[i], name) == 0) return i;
    }
    return -1;
  }
};

enum OrderStatus { kSuccess = 0, kPending = 1, kRefunded = 2 };

struct Order {
  int order_id;  // global sequential id (transaction time)
  int user_idx;
  int train_idx;
  int start_day;  // departure day from train origin
  int from;
  int to;
  int num;
  int price;  // unit price
  OrderStatus status;
  Order() {
    order_id = user_idx = train_idx = start_day = from = to = num = price = 0;
    status = kSuccess;
  }
};

struct StationRef {
  int train_idx;
  int sta_idx;
};

struct TicketCand {
  int train_idx;
  int from;
  int to;
  int start_day;
  int leave_abs;
  int arrive_abs;
  int price;
  int seat;
  int time_cost;
};

struct TransferCand {
  TicketCand a, b;
  int total_time;
  int total_cost;
  int ride1_time;
};

class TicketSystem {
  Vector<User> users_;
  Vector<Train> trains_;
  HashMap<int> user_idx_;   // username -> index
  HashMap<int> train_idx_;  // trainID -> index
  StringSet logged_in_;

  // station name -> list of StationRef (only released trains)
  HashMap<Vector<StationRef> *> station_map_;
  Vector<Vector<StationRef> *> station_vecs_;  // owned pointers for cleanup

  // seats_[train_idx] is a flat array: day_offset * (stationNum-1) + seg
  // day_offset = start_day - saleBegin
  Vector<int *> seats_;
  Vector<int> seats_len_;

  Vector<Order> orders_;
  // user_orders_[user_idx] = list of order indices (newest last; query reverses)
  Vector<Vector<int> > user_orders_;

  // pending queue: order indices in FIFO
  Vector<int> pending_;

  int next_order_id_;

  void index_station(const char *sta, int ti, int si) {
    Vector<StationRef> **pp = station_map_.find(sta);
    if (!pp) {
      Vector<StationRef> *v = new Vector<StationRef>();
      StationRef r;
      r.train_idx = ti;
      r.sta_idx = si;
      v->push_back(r);
      station_map_.put(sta, v);
      station_vecs_.push_back(v);
    } else {
      StationRef r;
      r.train_idx = ti;
      r.sta_idx = si;
      (*pp)->push_back(r);
    }
  }

  int *get_seats(int ti, int start_day) {
    Train &tr = trains_[ti];
    int day_off = start_day - tr.saleBegin;
    int segs = tr.stationNum - 1;
    return seats_[ti] + day_off * segs;
  }

  int remaining(int ti, int start_day, int from, int to) {
    int *s = get_seats(ti, start_day);
    int mn = s[from];
    for (int i = from + 1; i < to; ++i) {
      if (s[i] < mn) mn = s[i];
    }
    return mn;
  }

  void deduct(int ti, int start_day, int from, int to, int n) {
    int *s = get_seats(ti, start_day);
    for (int i = from; i < to; ++i) s[i] -= n;
  }

  void restore(int ti, int start_day, int from, int to, int n) {
    int *s = get_seats(ti, start_day);
    for (int i = from; i < to; ++i) s[i] += n;
  }

  void process_queue() {
    for (int i = 0; i < pending_.size();) {
      int oi = pending_[i];
      Order &o = orders_[oi];
      if (o.status != kPending) {
        pending_.erase_at(i);
        continue;
      }
      int rem = remaining(o.train_idx, o.start_day, o.from, o.to);
      if (rem >= o.num) {
        deduct(o.train_idx, o.start_day, o.from, o.to, o.num);
        o.status = kSuccess;
        pending_.erase_at(i);
        // continue from same i after erase
      } else {
        ++i;
      }
    }
  }

  void print_user(const User &u) {
    std::cout << u.username << ' ' << u.name << ' ' << u.mail << ' ' << u.privilege
              << '\n';
  }

  static void cpy(char *dst, const std::string &src, int maxlen) {
    int n = (int)src.size();
    if (n >= maxlen) n = maxlen - 1;
    std::memcpy(dst, src.c_str(), n);
    dst[n] = 0;
  }

 public:
  TicketSystem() : next_order_id_(0) {}

  ~TicketSystem() { clear_all(); }

  void clear_all() {
    for (int i = 0; i < station_vecs_.size(); ++i) delete station_vecs_[i];
    station_vecs_.clear();
    station_map_.clear();
    for (int i = 0; i < seats_.size(); ++i) delete[] seats_[i];
    seats_.clear();
    seats_len_.clear();
    users_.clear();
    trains_.clear();
    user_idx_.clear();
    train_idx_.clear();
    logged_in_.clear();
    orders_.clear();
    user_orders_.clear();
    pending_.clear();
    next_order_id_ = 0;
  }

  // ---------- persistence ----------
  void save(const char *prefix = "") {
    std::string p(prefix);
    {
      std::ofstream f(p + "users.dat", std::ios::binary);
      int n = users_.size();
      f.write((char *)&n, sizeof(n));
      f.write((char *)&next_order_id_, sizeof(next_order_id_));
      for (int i = 0; i < n; ++i) f.write((char *)&users_[i], sizeof(User));
    }
    {
      std::ofstream f(p + "trains.dat", std::ios::binary);
      int n = trains_.size();
      f.write((char *)&n, sizeof(n));
      for (int i = 0; i < n; ++i) f.write((char *)&trains_[i], sizeof(Train));
    }
    {
      std::ofstream f(p + "seats.dat", std::ios::binary);
      int n = seats_.size();
      f.write((char *)&n, sizeof(n));
      for (int i = 0; i < n; ++i) {
        int len = seats_len_[i];
        f.write((char *)&len, sizeof(len));
        if (len > 0 && seats_[i]) f.write((char *)seats_[i], sizeof(int) * len);
      }
    }
    {
      std::ofstream f(p + "orders.dat", std::ios::binary);
      int n = orders_.size();
      f.write((char *)&n, sizeof(n));
      for (int i = 0; i < n; ++i) f.write((char *)&orders_[i], sizeof(Order));
      int un = user_orders_.size();
      f.write((char *)&un, sizeof(un));
      for (int i = 0; i < un; ++i) {
        int m = user_orders_[i].size();
        f.write((char *)&m, sizeof(m));
        for (int j = 0; j < m; ++j) {
          int v = user_orders_[i][j];
          f.write((char *)&v, sizeof(v));
        }
      }
      int pn = pending_.size();
      f.write((char *)&pn, sizeof(pn));
      for (int i = 0; i < pn; ++i) {
        int v = pending_[i];
        f.write((char *)&v, sizeof(v));
      }
    }
  }

  bool load(const char *prefix = "") {
    std::string p(prefix);
    std::ifstream fu(p + "users.dat", std::ios::binary);
    if (!fu) return false;
    clear_all();
    int n = 0;
    fu.read((char *)&n, sizeof(n));
    fu.read((char *)&next_order_id_, sizeof(next_order_id_));
    users_.resize(n);
    for (int i = 0; i < n; ++i) {
      fu.read((char *)&users_[i], sizeof(User));
      user_idx_.put(users_[i].username, i);
    }
    user_orders_.resize(n);

    std::ifstream ft(p + "trains.dat", std::ios::binary);
    int tn = 0;
    ft.read((char *)&tn, sizeof(tn));
    trains_.resize(tn);
    seats_.resize(tn);
    seats_len_.resize(tn);
    for (int i = 0; i < tn; ++i) {
      seats_[i] = nullptr;
      seats_len_[i] = 0;
      ft.read((char *)&trains_[i], sizeof(Train));
      if (!trains_[i].deleted) {
        train_idx_.put(trains_[i].trainID, i);
        if (trains_[i].released) {
          for (int si = 0; si < trains_[i].stationNum; ++si) {
            index_station(trains_[i].stations[si], i, si);
          }
        }
      }
    }

    std::ifstream fs(p + "seats.dat", std::ios::binary);
    int sn = 0;
    fs.read((char *)&sn, sizeof(sn));
    for (int i = 0; i < sn; ++i) {
      int len = 0;
      fs.read((char *)&len, sizeof(len));
      seats_len_[i] = len;
      if (len > 0) {
        seats_[i] = new int[len];
        fs.read((char *)seats_[i], sizeof(int) * len);
      }
    }

    std::ifstream fo(p + "orders.dat", std::ios::binary);
    int on = 0;
    fo.read((char *)&on, sizeof(on));
    orders_.resize(on);
    for (int i = 0; i < on; ++i) fo.read((char *)&orders_[i], sizeof(Order));
    int un = 0;
    fo.read((char *)&un, sizeof(un));
    if (un > user_orders_.size()) user_orders_.resize(un);
    for (int i = 0; i < un; ++i) {
      int m = 0;
      fo.read((char *)&m, sizeof(m));
      user_orders_[i].resize(m);
      for (int j = 0; j < m; ++j) {
        int v = 0;
        fo.read((char *)&v, sizeof(v));
        user_orders_[i][j] = v;
      }
    }
    int pn = 0;
    fo.read((char *)&pn, sizeof(pn));
    pending_.resize(pn);
    for (int i = 0; i < pn; ++i) {
      int v = 0;
      fo.read((char *)&v, sizeof(v));
      pending_[i] = v;
    }
    return true;
  }

  // ---------- commands ----------
  void add_user(const std::string &c, const std::string &u, const std::string &p,
                const std::string &n, const std::string &m, int g) {
    if (user_idx_.contains(u.c_str())) {
      std::cout << "-1\n";
      return;
    }
    User nu;
    cpy(nu.username, u, kIdLen);
    cpy(nu.password, p, kPassLen);
    cpy(nu.name, n, kNameLen);
    cpy(nu.mail, m, kMailLen);
    if (users_.empty()) {
      nu.privilege = 10;
    } else {
      if (!logged_in_.contains(c)) {
        std::cout << "-1\n";
        return;
      }
      int ci;
      user_idx_.get(c, ci);
      if (g >= users_[ci].privilege) {
        std::cout << "-1\n";
        return;
      }
      nu.privilege = g;
    }
    int idx = users_.size();
    users_.push_back(nu);
    user_idx_.put(u, idx);
    user_orders_.push_back(Vector<int>());
    std::cout << "0\n";
  }

  void login(const std::string &u, const std::string &p) {
    int ui;
    if (!user_idx_.get(u, ui)) {
      std::cout << "-1\n";
      return;
    }
    if (logged_in_.contains(u)) {
      std::cout << "-1\n";
      return;
    }
    if (std::strcmp(users_[ui].password, p.c_str()) != 0) {
      std::cout << "-1\n";
      return;
    }
    logged_in_.insert(u);
    std::cout << "0\n";
  }

  void logout(const std::string &u) {
    if (!logged_in_.contains(u)) {
      std::cout << "-1\n";
      return;
    }
    logged_in_.erase(u);
    std::cout << "0\n";
  }

  void query_profile(const std::string &c, const std::string &u) {
    if (!logged_in_.contains(c)) {
      std::cout << "-1\n";
      return;
    }
    int ci, ui;
    if (!user_idx_.get(c, ci) || !user_idx_.get(u, ui)) {
      std::cout << "-1\n";
      return;
    }
    if (c != u && users_[ci].privilege <= users_[ui].privilege) {
      std::cout << "-1\n";
      return;
    }
    print_user(users_[ui]);
  }

  void modify_profile(const std::string &c, const std::string &u, bool has_p,
                      const std::string &p, bool has_n, const std::string &n,
                      bool has_m, const std::string &m, bool has_g, int g) {
    if (!logged_in_.contains(c)) {
      std::cout << "-1\n";
      return;
    }
    int ci, ui;
    if (!user_idx_.get(c, ci) || !user_idx_.get(u, ui)) {
      std::cout << "-1\n";
      return;
    }
    if (c != u && users_[ci].privilege <= users_[ui].privilege) {
      std::cout << "-1\n";
      return;
    }
    if (has_g && g >= users_[ci].privilege) {
      std::cout << "-1\n";
      return;
    }
    if (has_p) cpy(users_[ui].password, p, kPassLen);
    if (has_n) cpy(users_[ui].name, n, kNameLen);
    if (has_m) cpy(users_[ui].mail, m, kMailLen);
    if (has_g) users_[ui].privilege = g;
    print_user(users_[ui]);
  }

  void add_train(const std::string &id, int n, int m, const std::string &s,
                 const std::string &p, const std::string &x, const std::string &t,
                 const std::string &o, const std::string &d, char y) {
    if (train_idx_.contains(id.c_str())) {
      std::cout << "-1\n";
      return;
    }
    Train tr;
    cpy(tr.trainID, id, kIdLen);
    tr.stationNum = n;
    tr.seatNum = m;
    tr.type = y;
    Vector<std::string> sts = split_pipe(s);
    for (int i = 0; i < n; ++i) cpy(tr.stations[i], sts[i], kStaLen);
    Vector<int> pr = split_pipe_int(p);
    for (int i = 0; i < n - 1; ++i) tr.prices[i] = pr[i];
    tr.startTime = parse_time(x);
    Vector<int> tt = split_pipe_int(t);
    for (int i = 0; i < n - 1; ++i) tr.travelTimes[i] = tt[i];
    Vector<int> so = split_pipe_int(o);
    for (int i = 0; i < n - 2; ++i) tr.stopoverTimes[i] = so[i];
    Vector<std::string> dd = split_pipe(d);
    tr.saleBegin = parse_date(dd[0]);
    tr.saleEnd = parse_date(dd[1]);
    tr.released = false;
    tr.deleted = false;
    tr.compute_offsets();

    int ti = trains_.size();
    trains_.push_back(tr);
    train_idx_.put(id, ti);

    int days = tr.saleEnd - tr.saleBegin + 1;
    int segs = n - 1;
    int len = days * segs;
    int *arr = new int[len];
    for (int i = 0; i < len; ++i) arr[i] = m;
    seats_.push_back(arr);
    seats_len_.push_back(len);
    std::cout << "0\n";
  }

  void release_train(const std::string &id) {
    int ti;
    if (!train_idx_.get(id, ti)) {
      std::cout << "-1\n";
      return;
    }
    Train &tr = trains_[ti];
    if (tr.released || tr.deleted) {
      std::cout << "-1\n";
      return;
    }
    tr.released = true;
    for (int si = 0; si < tr.stationNum; ++si) {
      index_station(tr.stations[si], ti, si);
    }
    std::cout << "0\n";
  }

  void delete_train(const std::string &id) {
    int ti;
    if (!train_idx_.get(id, ti)) {
      std::cout << "-1\n";
      return;
    }
    Train &tr = trains_[ti];
    if (tr.released || tr.deleted) {
      std::cout << "-1\n";
      return;
    }
    tr.deleted = true;
    train_idx_.erase(id);
    std::cout << "0\n";
  }

  void query_train(const std::string &id, const std::string &ds) {
    int ti;
    if (!train_idx_.get(id, ti)) {
      std::cout << "-1\n";
      return;
    }
    Train &tr = trains_[ti];
    int day = parse_date(ds);
    if (day < tr.saleBegin || day > tr.saleEnd) {
      std::cout << "-1\n";
      return;
    }
    std::cout << tr.trainID << ' ' << tr.type << '\n';
    int base = day * 1440 + tr.startTime;
    for (int i = 0; i < tr.stationNum; ++i) {
      std::cout << tr.stations[i] << ' ';
      if (i == 0) {
        std::cout << "xx-xx xx:xx";
      } else {
        char buf[32];
        format_datetime(base + tr.arrive_offset[i], buf);
        std::cout << buf;
      }
      std::cout << " -> ";
      if (i == tr.stationNum - 1) {
        std::cout << "xx-xx xx:xx";
      } else {
        char buf[32];
        format_datetime(base + tr.leave_offset[i], buf);
        std::cout << buf;
      }
      std::cout << ' ' << tr.prefix_price[i] << ' ';
      if (i == tr.stationNum - 1) {
        std::cout << "x\n";
      } else {
        int rem;
        if (!tr.released) {
          rem = tr.seatNum;
        } else {
          rem = get_seats(ti, day)[i];
        }
        std::cout << rem << '\n';
      }
    }
  }

  void query_ticket(const std::string &from, const std::string &to,
                    const std::string &ds, bool by_time) {
    int day = parse_date(ds);
    Vector<StationRef> **pp = station_map_.find(from);
    Vector<TicketCand> cands;
    if (pp && *pp) {
      Vector<StationRef> &refs = **pp;
      for (int i = 0; i < refs.size(); ++i) {
        int ti = refs[i].train_idx;
        int fi = refs[i].sta_idx;
        Train &tr = trains_[ti];
        if (!tr.released || tr.deleted) continue;
        int ti_to = tr.find_station(to.c_str());
        if (ti_to < 0 || ti_to <= fi) continue;
        // leave day from boarding station = day
        // start_day * 1440 + startTime + leave_offset[fi]  's day part == day
        // leave_abs = start_day*1440 + startTime + leave_offset[fi]
        // leave_abs / 1440 == day
        // start_day*1440 + startTime + leave_offset[fi] = day*1440 + (startTime+leave_offset[fi])%1440
        // Actually: leave_tod_total = startTime + leave_offset[fi]
        // leave_day = start_day + leave_tod_total / 1440
        // we need leave_day == day => start_day = day - leave_tod_total/1440
        int leave_from_start = tr.startTime + tr.leave_offset[fi];
        int start_day = day - leave_from_start / 1440;
        if (start_day < tr.saleBegin || start_day > tr.saleEnd) continue;
        TicketCand c;
        c.train_idx = ti;
        c.from = fi;
        c.to = ti_to;
        c.start_day = start_day;
        int base = start_day * 1440 + tr.startTime;
        c.leave_abs = base + tr.leave_offset[fi];
        c.arrive_abs = base + tr.arrive_offset[ti_to];
        c.price = tr.prefix_price[ti_to] - tr.prefix_price[fi];
        c.seat = remaining(ti, start_day, fi, ti_to);
        c.time_cost = c.arrive_abs - c.leave_abs;
        cands.push_back(c);
      }
    }
    if (by_time) {
      sort_vec(cands, [&](const TicketCand &a, const TicketCand &b) {
        if (a.time_cost != b.time_cost) return a.time_cost < b.time_cost;
        return std::strcmp(trains_[a.train_idx].trainID, trains_[b.train_idx].trainID) < 0;
      });
    } else {
      sort_vec(cands, [&](const TicketCand &a, const TicketCand &b) {
        if (a.price != b.price) return a.price < b.price;
        return std::strcmp(trains_[a.train_idx].trainID, trains_[b.train_idx].trainID) < 0;
      });
    }
    std::cout << cands.size() << '\n';
    for (int i = 0; i < cands.size(); ++i) {
      TicketCand &c = cands[i];
      Train &tr = trains_[c.train_idx];
      char lb[32], ab[32];
      format_datetime(c.leave_abs, lb);
      format_datetime(c.arrive_abs, ab);
      std::cout << tr.trainID << ' ' << tr.stations[c.from] << ' ' << lb << " -> "
                << tr.stations[c.to] << ' ' << ab << ' ' << c.price << ' ' << c.seat
                << '\n';
    }
  }

  void query_transfer(const std::string &from, const std::string &to,
                      const std::string &ds, bool by_time) {
    int day = parse_date(ds);
    Vector<StationRef> **pp = station_map_.find(from);
    bool found = false;
    TransferCand best;
    if (pp && *pp) {
      Vector<StationRef> &refs = **pp;
      // Collect all candidate first-leg trains
      for (int i = 0; i < refs.size(); ++i) {
        int ti1 = refs[i].train_idx;
        int fi = refs[i].sta_idx;
        Train &tr1 = trains_[ti1];
        if (!tr1.released || tr1.deleted) continue;
        int leave_from_start = tr1.startTime + tr1.leave_offset[fi];
        int start_day1 = day - leave_from_start / 1440;
        if (start_day1 < tr1.saleBegin || start_day1 > tr1.saleEnd) continue;
        int base1 = start_day1 * 1440 + tr1.startTime;
        int leave1 = base1 + tr1.leave_offset[fi];

        // Transfer at any later station on train1 (not destination)
        for (int mid = fi + 1; mid < tr1.stationNum; ++mid) {
          // Don't transfer if mid is the final destination we're going to
          // (that would be direct, but we need exactly one transfer)
          if (std::strcmp(tr1.stations[mid], to.c_str()) == 0) continue;

          int arrive_mid = base1 + tr1.arrive_offset[mid];
          Vector<StationRef> **pp2 = station_map_.find(tr1.stations[mid]);
          if (!pp2 || !*pp2) continue;
          Vector<StationRef> &refs2 = **pp2;
          for (int j = 0; j < refs2.size(); ++j) {
            int ti2 = refs2[j].train_idx;
            int mid2 = refs2[j].sta_idx;
            if (ti2 == ti1) continue;
            Train &tr2 = trains_[ti2];
            if (!tr2.released || tr2.deleted) continue;
            int ti_to = tr2.find_station(to.c_str());
            if (ti_to < 0 || ti_to <= mid2) continue;

            // train2 must leave mid at or after arrive_mid
            // leave2_abs = start_day2*1440 + startTime2 + leave_offset[mid2]
            // need leave2_abs >= arrive_mid
            // Also boarding day at mid for train2 can be any day >= arrive day
            // Iterate possible start_day2 in sale range
            // leave2 = start_day2*1440 + st2 + lo2 >= arrive_mid
            // start_day2 >= ceil((arrive_mid - st2 - lo2) / 1440)
            int lo2 = tr2.startTime + tr2.leave_offset[mid2];
            // leave = start_day2*1440 + lo2 >= arrive_mid
            // start_day2*1440 >= arrive_mid - lo2
            int need = arrive_mid - lo2;
            int start_day2 = (need <= 0) ? 0 : (need + 1439) / 1440;
            if (start_day2 < tr2.saleBegin) start_day2 = tr2.saleBegin;
            if (start_day2 > tr2.saleEnd) continue;

            int base2 = start_day2 * 1440 + tr2.startTime;
            int leave2 = base2 + tr2.leave_offset[mid2];
            if (leave2 < arrive_mid) {
              // try next day
              ++start_day2;
              if (start_day2 > tr2.saleEnd) continue;
              base2 = start_day2 * 1440 + tr2.startTime;
              leave2 = base2 + tr2.leave_offset[mid2];
              if (leave2 < arrive_mid) continue;
            }

            int arrive2 = base2 + tr2.arrive_offset[ti_to];
            TransferCand cand;
            cand.a.train_idx = ti1;
            cand.a.from = fi;
            cand.a.to = mid;
            cand.a.start_day = start_day1;
            cand.a.leave_abs = leave1;
            cand.a.arrive_abs = arrive_mid;
            cand.a.price = tr1.prefix_price[mid] - tr1.prefix_price[fi];
            cand.a.seat = remaining(ti1, start_day1, fi, mid);
            cand.a.time_cost = arrive_mid - leave1;

            cand.b.train_idx = ti2;
            cand.b.from = mid2;
            cand.b.to = ti_to;
            cand.b.start_day = start_day2;
            cand.b.leave_abs = leave2;
            cand.b.arrive_abs = arrive2;
            cand.b.price = tr2.prefix_price[ti_to] - tr2.prefix_price[mid2];
            cand.b.seat = remaining(ti2, start_day2, mid2, ti_to);
            cand.b.time_cost = arrive2 - leave2;

            cand.total_time = arrive2 - leave1;
            cand.total_cost = cand.a.price + cand.b.price;
            cand.ride1_time = cand.a.time_cost;

            auto better = [&](const TransferCand &x, const TransferCand &y) -> bool {
              if (by_time) {
                if (x.total_time != y.total_time) return x.total_time < y.total_time;
              } else {
                if (x.total_cost != y.total_cost) return x.total_cost < y.total_cost;
              }
              if (x.ride1_time != y.ride1_time) return x.ride1_time < y.ride1_time;
              int cmp1 = std::strcmp(trains_[x.a.train_idx].trainID,
                                     trains_[y.a.train_idx].trainID);
              if (cmp1 != 0) return cmp1 < 0;
              return std::strcmp(trains_[x.b.train_idx].trainID,
                                 trains_[y.b.train_idx].trainID) < 0;
            };

            if (!found || better(cand, best)) {
              best = cand;
              found = true;
            }
          }
        }
      }
    }
    if (!found) {
      std::cout << "0\n";
      return;
    }
    auto print_leg = [&](TicketCand &c) {
      Train &tr = trains_[c.train_idx];
      char lb[32], ab[32];
      format_datetime(c.leave_abs, lb);
      format_datetime(c.arrive_abs, ab);
      std::cout << tr.trainID << ' ' << tr.stations[c.from] << ' ' << lb << " -> "
                << tr.stations[c.to] << ' ' << ab << ' ' << c.price << ' ' << c.seat
                << '\n';
    };
    print_leg(best.a);
    print_leg(best.b);
  }

  void buy_ticket(const std::string &u, const std::string &id, const std::string &ds,
                  int n, const std::string &f, const std::string &t, bool queue) {
    if (!logged_in_.contains(u)) {
      std::cout << "-1\n";
      return;
    }
    int ui, ti;
    if (!user_idx_.get(u, ui) || !train_idx_.get(id, ti)) {
      std::cout << "-1\n";
      return;
    }
    Train &tr = trains_[ti];
    if (!tr.released || tr.deleted) {
      std::cout << "-1\n";
      return;
    }
    if (n <= 0 || n > tr.seatNum) {
      std::cout << "-1\n";
      return;
    }
    int fi = tr.find_station(f.c_str());
    int ti_to = tr.find_station(t.c_str());
    if (fi < 0 || ti_to < 0 || fi >= ti_to) {
      std::cout << "-1\n";
      return;
    }
    int day = parse_date(ds);
    int leave_from_start = tr.startTime + tr.leave_offset[fi];
    int start_day = day - leave_from_start / 1440;
    if (start_day < tr.saleBegin || start_day > tr.saleEnd) {
      std::cout << "-1\n";
      return;
    }
    int price = tr.prefix_price[ti_to] - tr.prefix_price[fi];
    int rem = remaining(ti, start_day, fi, ti_to);

    Order o;
    o.order_id = next_order_id_++;
    o.user_idx = ui;
    o.train_idx = ti;
    o.start_day = start_day;
    o.from = fi;
    o.to = ti_to;
    o.num = n;
    o.price = price;

    if (rem >= n) {
      deduct(ti, start_day, fi, ti_to, n);
      o.status = kSuccess;
      int oi = orders_.size();
      orders_.push_back(o);
      user_orders_[ui].push_back(oi);
      std::cout << (long long)price * n << '\n';
    } else if (queue) {
      o.status = kPending;
      int oi = orders_.size();
      orders_.push_back(o);
      user_orders_[ui].push_back(oi);
      pending_.push_back(oi);
      std::cout << "queue\n";
    } else {
      std::cout << "-1\n";
    }
  }

  void query_order(const std::string &u) {
    if (!logged_in_.contains(u)) {
      std::cout << "-1\n";
      return;
    }
    int ui;
    if (!user_idx_.get(u, ui)) {
      std::cout << "-1\n";
      return;
    }
    Vector<int> &uos = user_orders_[ui];
    std::cout << uos.size() << '\n';
    for (int i = uos.size() - 1; i >= 0; --i) {
      Order &o = orders_[uos[i]];
      Train &tr = trains_[o.train_idx];
      const char *st =
          o.status == kSuccess ? "success" : (o.status == kPending ? "pending" : "refunded");
      int base = o.start_day * 1440 + tr.startTime;
      char lb[32], ab[32];
      format_datetime(base + tr.leave_offset[o.from], lb);
      format_datetime(base + tr.arrive_offset[o.to], ab);
      std::cout << '[' << st << "] " << tr.trainID << ' ' << tr.stations[o.from] << ' '
                << lb << " -> " << tr.stations[o.to] << ' ' << ab << ' ' << o.price << ' '
                << o.num << '\n';
    }
  }

  void refund_ticket(const std::string &u, int n) {
    if (!logged_in_.contains(u)) {
      std::cout << "-1\n";
      return;
    }
    int ui;
    if (!user_idx_.get(u, ui)) {
      std::cout << "-1\n";
      return;
    }
    Vector<int> &uos = user_orders_[ui];
    if (n < 1 || n > uos.size()) {
      std::cout << "-1\n";
      return;
    }
    int oi = uos[uos.size() - n];
    Order &o = orders_[oi];
    if (o.status == kRefunded) {
      std::cout << "-1\n";
      return;
    }
    if (o.status == kSuccess) {
      restore(o.train_idx, o.start_day, o.from, o.to, o.num);
      o.status = kRefunded;
      process_queue();
    } else if (o.status == kPending) {
      o.status = kRefunded;
      // will be cleaned from pending on next process or here
      for (int i = 0; i < pending_.size(); ++i) {
        if (pending_[i] == oi) {
          pending_.erase_at(i);
          break;
        }
      }
    }
    std::cout << "0\n";
  }

  void clean() {
    clear_all();
    // remove data files
    std::remove("users.dat");
    std::remove("trains.dat");
    std::remove("seats.dat");
    std::remove("orders.dat");
    std::cout << "0\n";
  }

  void exit_cmd() {
    logged_in_.clear();
    save();
    std::cout << "bye\n";
  }
};

#endif
