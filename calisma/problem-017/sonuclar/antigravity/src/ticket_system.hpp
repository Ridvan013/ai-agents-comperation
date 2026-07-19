#ifndef TICKET_SYSTEM_HPP
#define TICKET_SYSTEM_HPP

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include "file_array.hpp"
#include "hashmap.hpp"
#include "utils.hpp"
#include "vector.hpp"

static const int kMaxSta = 100;
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
  User() {
    username[0] = 0;
    password[0] = 0;
    name[0] = 0;
    mail[0] = 0;
    privilege = 0;
  }
};

struct Train {
  char trainID[kIdLen];
  int stationNum;
  int seatNum;
  char stations[kMaxSta][kStaLen];
  int prices[kMaxSta];
  int startTime;
  int travelTimes[kMaxSta];
  int stopoverTimes[kMaxSta];
  int saleBegin;
  int saleEnd;
  char type;
  bool released;
  bool deleted;
  int seat_offset;
  int leave_offset[kMaxSta];
  int arrive_offset[kMaxSta];
  int prefix_price[kMaxSta];

  Train() {
    trainID[0] = 0;
    stationNum = 0;
    seatNum = 0;
    startTime = 0;
    saleBegin = 0;
    saleEnd = 0;
    type = 'G';
    released = false;
    deleted = false;
    seat_offset = -1;
    std::memset(prices, 0, sizeof(prices));
    std::memset(travelTimes, 0, sizeof(travelTimes));
    std::memset(stopoverTimes, 0, sizeof(stopoverTimes));
    std::memset(leave_offset, 0, sizeof(leave_offset));
    std::memset(arrive_offset, 0, sizeof(arrive_offset));
    std::memset(prefix_price, 0, sizeof(prefix_price));
    for (int i = 0; i < kMaxSta; ++i) stations[i][0] = 0;
  }

  void compute_offsets() {
    leave_offset[0] = 0;
    arrive_offset[0] = 0;
    prefix_price[0] = 0;
    int minutes = 0;
    for (int i = 0; i + 1 < stationNum; ++i) {
      minutes += travelTimes[i];
      arrive_offset[i + 1] = minutes;
      if (i + 1 < stationNum - 1) minutes += stopoverTimes[i];
      leave_offset[i + 1] = minutes;
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
  int order_id;
  int user_idx;
  int train_idx;
  int start_day;
  int from;
  int to;
  int num;
  int price;
  OrderStatus status;
  Order() {
    order_id = 0;
    user_idx = 0;
    train_idx = 0;
    start_day = 0;
    from = 0;
    to = 0;
    num = 0;
    price = 0;
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
  char train_id[kIdLen];
  TicketCand() {
    train_idx = 0;
    from = 0;
    to = 0;
    start_day = 0;
    leave_abs = 0;
    arrive_abs = 0;
    price = 0;
    seat = 0;
    time_cost = 0;
    train_id[0] = 0;
  }
};

struct TransferCand {
  TicketCand a;
  TicketCand b;
  int total_time;
  int total_cost;
  int ride1_time;
  TransferCand() {
    total_time = 0;
    total_cost = 0;
    ride1_time = 0;
  }
};

class TicketSystem {
  Vector<User> users_;
  CachedFileArray<Train, 64> trains_;
  CachedFileArray<Order, 128> orders_;
  HashMap<int> user_idx_;
  HashMap<int> train_idx_;
  StringSet logged_in_;
  HashMap<Vector<StationRef> *> station_map_;
  Vector<Vector<StationRef> *> station_vecs_;
  Vector<Vector<int> > user_orders_;
  Vector<int> pending_;
  std::fstream seats_fs_;
  int next_order_id_;

  static void cpy(char *dst, const std::string &src, int maxlen) {
    int n = (int)src.size();
    if (n >= maxlen) n = maxlen - 1;
    std::memcpy(dst, src.c_str(), n);
    dst[n] = 0;
  }

  void print_user(const User &u) const {
    std::cout << u.username << ' ' << u.name << ' ' << u.mail << ' ' << u.privilege
              << '\n';
  }

  void index_station(const char *sta, int train_idx, int sta_idx) {
    Vector<StationRef> **pp = station_map_.find(sta);
    StationRef ref;
    ref.train_idx = train_idx;
    ref.sta_idx = sta_idx;
    if (!pp) {
      Vector<StationRef> *vec = new Vector<StationRef>();
      vec->push_back(ref);
      station_map_.put(sta, vec);
      station_vecs_.push_back(vec);
    } else {
      (*pp)->push_back(ref);
    }
  }

  void clear_indexes() {
    for (int i = 0; i < station_vecs_.size(); ++i) delete station_vecs_[i];
    station_vecs_.clear();
    station_map_.clear();
    user_idx_.clear();
    train_idx_.clear();
    logged_in_.clear();
  }

  void close_storage() {
    trains_.close();
    orders_.close();
    if (seats_fs_.is_open()) {
      seats_fs_.flush();
      seats_fs_.close();
    }
  }

  void open_storage(const std::string &prefix) {
    trains_.open(prefix + "trains.dat");
    orders_.open(prefix + "orders.dat");
    seats_fs_.open((prefix + "seats.dat").c_str(),
                   std::ios::in | std::ios::out | std::ios::binary);
    if (!seats_fs_) {
      seats_fs_.clear();
      seats_fs_.open((prefix + "seats.dat").c_str(), std::ios::out | std::ios::binary);
      seats_fs_.close();
      seats_fs_.open((prefix + "seats.dat").c_str(),
                     std::ios::in | std::ios::out | std::ios::binary);
    }
  }

  void remove_storage_files(const std::string &prefix) {
    close_storage();
    std::remove((prefix + "meta.dat").c_str());
    std::remove((prefix + "trains.dat").c_str());
    std::remove((prefix + "orders.dat").c_str());
    std::remove((prefix + "seats.dat").c_str());
  }

  void read_seats(const Train &tr, int start_day, int from, int len, int *buf) {
    int segs = tr.stationNum - 1;
    int day_off = start_day - tr.saleBegin;
    int offset = tr.seat_offset + (day_off * segs + from) * (int)sizeof(int);
    seats_fs_.clear();
    seats_fs_.seekg(offset, std::ios::beg);
    seats_fs_.read(reinterpret_cast<char *>(buf), len * sizeof(int));
  }

  void write_seats(const Train &tr, int start_day, int from, int len, int *buf) {
    int segs = tr.stationNum - 1;
    int day_off = start_day - tr.saleBegin;
    int offset = tr.seat_offset + (day_off * segs + from) * (int)sizeof(int);
    seats_fs_.clear();
    seats_fs_.seekp(offset, std::ios::beg);
    seats_fs_.write(reinterpret_cast<const char *>(buf), len * sizeof(int));
    seats_fs_.flush();
  }

  int remaining(const Train &tr, int start_day, int from, int to) {
    int len = to - from;
    int stack_buf[kMaxSta];
    int *buf = (len <= kMaxSta) ? stack_buf : new int[len];
    read_seats(tr, start_day, from, len, buf);
    int ans = buf[0];
    for (int i = 1; i < len; ++i) {
      if (buf[i] < ans) ans = buf[i];
    }
    if (buf != stack_buf) delete[] buf;
    return ans;
  }

  void change_seats(const Train &tr, int start_day, int from, int to, int delta) {
    int len = to - from;
    int stack_buf[kMaxSta];
    int *buf = (len <= kMaxSta) ? stack_buf : new int[len];
    read_seats(tr, start_day, from, len, buf);
    for (int i = 0; i < len; ++i) buf[i] += delta;
    write_seats(tr, start_day, from, len, buf);
    if (buf != stack_buf) delete[] buf;
  }

  void fill_ticket(TicketCand &cand, const Train &tr, int train_idx, int from, int to,
                   int start_day) {
    cand.train_idx = train_idx;
    cand.from = from;
    cand.to = to;
    cand.start_day = start_day;
    int base = start_day * 1440 + tr.startTime;
    cand.leave_abs = base + tr.leave_offset[from];
    cand.arrive_abs = base + tr.arrive_offset[to];
    cand.price = tr.prefix_price[to] - tr.prefix_price[from];
    cand.seat = remaining(tr, start_day, from, to);
    cand.time_cost = cand.arrive_abs - cand.leave_abs;
    cpy(cand.train_id, tr.trainID, kIdLen);
  }

  bool better_transfer(const TransferCand &x, const TransferCand &y, bool by_time) const {
    if (by_time) {
      if (x.total_time != y.total_time) return x.total_time < y.total_time;
    } else {
      if (x.total_cost != y.total_cost) return x.total_cost < y.total_cost;
    }
    if (x.ride1_time != y.ride1_time) return x.ride1_time < y.ride1_time;
    int cmp1 = std::strcmp(x.a.train_id, y.a.train_id);
    if (cmp1 != 0) return cmp1 < 0;
    return std::strcmp(x.b.train_id, y.b.train_id) < 0;
  }

  void process_queue() {
    for (int i = 0; i < pending_.size();) {
      int order_index = pending_[i];
      Order order = orders_.read(order_index);
      if (order.status != kPending) {
        pending_.erase_at(i);
        continue;
      }
      Train tr = trains_.read(order.train_idx);
      int rem = remaining(tr, order.start_day, order.from, order.to);
      if (rem >= order.num) {
        change_seats(tr, order.start_day, order.from, order.to, -order.num);
        order.status = kSuccess;
        orders_.update(order_index, order);
        pending_.erase_at(i);
      } else {
        ++i;
      }
    }
  }

 public:
  TicketSystem() : next_order_id_(0) {}

  ~TicketSystem() {
    close_storage();
    clear_all();
  }

  void clear_all() {
    clear_indexes();
    users_.clear();
    user_orders_.clear();
    pending_.clear();
    next_order_id_ = 0;
  }

  void save(const char *prefix = "") {
    std::string p(prefix);
    close_storage();
    std::ofstream out((p + "meta.dat").c_str(), std::ios::binary);
    int user_count = users_.size();
    out.write(reinterpret_cast<const char *>(&user_count), sizeof(user_count));
    out.write(reinterpret_cast<const char *>(&next_order_id_), sizeof(next_order_id_));
    for (int i = 0; i < user_count; ++i) {
      out.write(reinterpret_cast<const char *>(&users_[i]), sizeof(User));
    }

    int order_vec_count = user_orders_.size();
    out.write(reinterpret_cast<const char *>(&order_vec_count), sizeof(order_vec_count));
    for (int i = 0; i < order_vec_count; ++i) {
      int cnt = user_orders_[i].size();
      out.write(reinterpret_cast<const char *>(&cnt), sizeof(cnt));
      for (int j = 0; j < cnt; ++j) {
        int v = user_orders_[i][j];
        out.write(reinterpret_cast<const char *>(&v), sizeof(v));
      }
    }

    int pending_count = pending_.size();
    out.write(reinterpret_cast<const char *>(&pending_count), sizeof(pending_count));
    for (int i = 0; i < pending_count; ++i) {
      int v = pending_[i];
      out.write(reinterpret_cast<const char *>(&v), sizeof(v));
    }
  }

  bool load(const char *prefix = "") {
    std::string p(prefix);
    clear_all();
    open_storage(p);

    std::ifstream in((p + "meta.dat").c_str(), std::ios::binary);
    if (!in) return false;

    int user_count = 0;
    in.read(reinterpret_cast<char *>(&user_count), sizeof(user_count));
    in.read(reinterpret_cast<char *>(&next_order_id_), sizeof(next_order_id_));
    users_.resize(user_count);
    user_orders_.resize(user_count);
    for (int i = 0; i < user_count; ++i) {
      in.read(reinterpret_cast<char *>(&users_[i]), sizeof(User));
      user_idx_.put(users_[i].username, i);
    }

    int order_vec_count = 0;
    in.read(reinterpret_cast<char *>(&order_vec_count), sizeof(order_vec_count));
    user_orders_.resize(order_vec_count);
    for (int i = 0; i < order_vec_count; ++i) {
      int cnt = 0;
      in.read(reinterpret_cast<char *>(&cnt), sizeof(cnt));
      user_orders_[i].resize(cnt);
      for (int j = 0; j < cnt; ++j) {
        int v = 0;
        in.read(reinterpret_cast<char *>(&v), sizeof(v));
        user_orders_[i][j] = v;
      }
    }

    int pending_count = 0;
    in.read(reinterpret_cast<char *>(&pending_count), sizeof(pending_count));
    pending_.resize(pending_count);
    for (int i = 0; i < pending_count; ++i) {
      int v = 0;
      in.read(reinterpret_cast<char *>(&v), sizeof(v));
      pending_[i] = v;
    }

    int train_count = trains_.size();
    for (int i = 0; i < train_count; ++i) {
      Train tr = trains_.read(i);
      if (tr.deleted) continue;
      train_idx_.put(tr.trainID, i);
      if (tr.released) {
        for (int j = 0; j < tr.stationNum; ++j) index_station(tr.stations[j], i, j);
      }
    }
    return true;
  }

  void add_user(const std::string &c, const std::string &u, const std::string &p,
                const std::string &n, const std::string &m, int g) {
    if (user_idx_.contains(u.c_str())) {
      std::cout << "-1\n";
      return;
    }

    User user;
    cpy(user.username, u, kIdLen);
    cpy(user.password, p, kPassLen);
    cpy(user.name, n, kNameLen);
    cpy(user.mail, m, kMailLen);

    if (users_.empty()) {
      user.privilege = 10;
    } else {
      if (!logged_in_.contains(c)) {
        std::cout << "-1\n";
        return;
      }
      int cur_idx = 0;
      user_idx_.get(c, cur_idx);
      if (g >= users_[cur_idx].privilege) {
        std::cout << "-1\n";
        return;
      }
      user.privilege = g;
    }

    int idx = users_.size();
    users_.push_back(user);
    user_idx_.put(u, idx);
    user_orders_.push_back(Vector<int>());
    std::cout << "0\n";
  }

  void login(const std::string &u, const std::string &p) {
    int idx = 0;
    if (!user_idx_.get(u, idx) || logged_in_.contains(u) ||
        std::strcmp(users_[idx].password, p.c_str()) != 0) {
      std::cout << "-1\n";
      return;
    }
    logged_in_.insert(u);
    std::cout << "0\n";
  }

  void logout(const std::string &u) {
    if (!logged_in_.erase(u)) {
      std::cout << "-1\n";
      return;
    }
    std::cout << "0\n";
  }

  void query_profile(const std::string &c, const std::string &u) {
    if (!logged_in_.contains(c)) {
      std::cout << "-1\n";
      return;
    }
    int cur_idx = 0, user_idx = 0;
    if (!user_idx_.get(c, cur_idx) || !user_idx_.get(u, user_idx)) {
      std::cout << "-1\n";
      return;
    }
    if (c != u && users_[cur_idx].privilege <= users_[user_idx].privilege) {
      std::cout << "-1\n";
      return;
    }
    print_user(users_[user_idx]);
  }

  void modify_profile(const std::string &c, const std::string &u, bool has_p,
                      const std::string &p, bool has_n, const std::string &n,
                      bool has_m, const std::string &m, bool has_g, int g) {
    if (!logged_in_.contains(c)) {
      std::cout << "-1\n";
      return;
    }
    int cur_idx = 0, user_idx = 0;
    if (!user_idx_.get(c, cur_idx) || !user_idx_.get(u, user_idx)) {
      std::cout << "-1\n";
      return;
    }
    if (c != u && users_[cur_idx].privilege <= users_[user_idx].privilege) {
      std::cout << "-1\n";
      return;
    }
    if (has_g && g >= users_[cur_idx].privilege) {
      std::cout << "-1\n";
      return;
    }

    if (has_p) cpy(users_[user_idx].password, p, kPassLen);
    if (has_n) cpy(users_[user_idx].name, n, kNameLen);
    if (has_m) cpy(users_[user_idx].mail, m, kMailLen);
    if (has_g) users_[user_idx].privilege = g;
    print_user(users_[user_idx]);
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

    Vector<std::string> stations = split_pipe(s);
    for (int i = 0; i < n; ++i) cpy(tr.stations[i], stations[i], kStaLen);

    Vector<int> prices = split_pipe_int(p);
    for (int i = 0; i < n - 1; ++i) tr.prices[i] = prices[i];

    tr.startTime = parse_time(x);

    Vector<int> travel = split_pipe_int(t);
    for (int i = 0; i < n - 1; ++i) tr.travelTimes[i] = travel[i];

    Vector<int> stopover = split_pipe_int(o);
    for (int i = 0; i < n - 2; ++i) tr.stopoverTimes[i] = stopover[i];

    Vector<std::string> sale = split_pipe(d);
    tr.saleBegin = parse_date(sale[0]);
    tr.saleEnd = parse_date(sale[1]);
    tr.compute_offsets();

    seats_fs_.clear();
    seats_fs_.seekp(0, std::ios::end);
    std::streamoff end_pos = seats_fs_.tellp();
    if (end_pos < 0) end_pos = 0;
    tr.seat_offset = (int)end_pos;

    int len = (tr.saleEnd - tr.saleBegin + 1) * (tr.stationNum - 1);
    int *buf = new int[len];
    for (int i = 0; i < len; ++i) buf[i] = tr.seatNum;
    seats_fs_.write(reinterpret_cast<const char *>(buf), len * sizeof(int));
    seats_fs_.flush();
    delete[] buf;

    int idx = trains_.push_back(tr);
    train_idx_.put(id, idx);
    std::cout << "0\n";
  }

  void release_train(const std::string &id) {
    int idx = 0;
    if (!train_idx_.get(id, idx)) {
      std::cout << "-1\n";
      return;
    }
    Train tr = trains_.read(idx);
    if (tr.deleted || tr.released) {
      std::cout << "-1\n";
      return;
    }
    tr.released = true;
    trains_.update(idx, tr);
    for (int i = 0; i < tr.stationNum; ++i) index_station(tr.stations[i], idx, i);
    std::cout << "0\n";
  }

  void query_train(const std::string &id, const std::string &ds) {
    int idx = 0;
    if (!train_idx_.get(id, idx)) {
      std::cout << "-1\n";
      return;
    }
    Train tr = trains_.read(idx);
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
      } else if (!tr.released) {
        std::cout << tr.seatNum << '\n';
      } else {
        std::cout << remaining(tr, day, i, i + 1) << '\n';
      }
    }
  }

  void delete_train(const std::string &id) {
    int idx = 0;
    if (!train_idx_.get(id, idx)) {
      std::cout << "-1\n";
      return;
    }
    Train tr = trains_.read(idx);
    if (tr.deleted || tr.released) {
      std::cout << "-1\n";
      return;
    }
    tr.deleted = true;
    trains_.update(idx, tr);
    train_idx_.erase(id);
    std::cout << "0\n";
  }

  void query_ticket(const std::string &from, const std::string &to,
                    const std::string &ds, bool by_time) {
    int day = parse_date(ds);
    Vector<TicketCand> cands;
    Vector<StationRef> **pp = station_map_.find(from);
    if (pp && *pp) {
      Vector<StationRef> &refs = **pp;
      for (int i = 0; i < refs.size(); ++i) {
        int train_index = refs[i].train_idx;
        int from_index = refs[i].sta_idx;
        Train tr = trains_.read(train_index);
        if (tr.deleted || !tr.released) continue;

        int to_index = tr.find_station(to.c_str());
        if (to_index <= from_index) continue;

        int leave_from_start = tr.startTime + tr.leave_offset[from_index];
        int start_day = day - leave_from_start / 1440;
        if (start_day < tr.saleBegin || start_day > tr.saleEnd) continue;

        TicketCand cand;
        fill_ticket(cand, tr, train_index, from_index, to_index, start_day);
        cands.push_back(cand);
      }
    }

    if (by_time) {
      sort_vec(cands, [&](const TicketCand &a, const TicketCand &b) {
        if (a.time_cost != b.time_cost) return a.time_cost < b.time_cost;
        return std::strcmp(a.train_id, b.train_id) < 0;
      });
    } else {
      sort_vec(cands, [&](const TicketCand &a, const TicketCand &b) {
        if (a.price != b.price) return a.price < b.price;
        return std::strcmp(a.train_id, b.train_id) < 0;
      });
    }

    std::cout << cands.size() << '\n';
    for (int i = 0; i < cands.size(); ++i) {
      Train tr = trains_.read(cands[i].train_idx);
      char leave_buf[32], arrive_buf[32];
      format_datetime(cands[i].leave_abs, leave_buf);
      format_datetime(cands[i].arrive_abs, arrive_buf);
      std::cout << cands[i].train_id << ' ' << tr.stations[cands[i].from] << ' '
                << leave_buf << " -> " << tr.stations[cands[i].to] << ' ' << arrive_buf
                << ' ' << cands[i].price << ' ' << cands[i].seat << '\n';
    }
  }

  void query_transfer(const std::string &from, const std::string &to,
                      const std::string &ds, bool by_time) {
    int day = parse_date(ds);
    bool found = false;
    TransferCand best;
    Vector<StationRef> **pp = station_map_.find(from);
    if (pp && *pp) {
      Vector<StationRef> &refs = **pp;
      for (int i = 0; i < refs.size(); ++i) {
        int train1_index = refs[i].train_idx;
        int from_index = refs[i].sta_idx;
        Train tr1 = trains_.read(train1_index);
        if (tr1.deleted || !tr1.released) continue;

        int leave_from_start = tr1.startTime + tr1.leave_offset[from_index];
        int start_day1 = day - leave_from_start / 1440;
        if (start_day1 < tr1.saleBegin || start_day1 > tr1.saleEnd) continue;

        int base1 = start_day1 * 1440 + tr1.startTime;
        for (int mid = from_index + 1; mid < tr1.stationNum; ++mid) {
          if (std::strcmp(tr1.stations[mid], to.c_str()) == 0) continue;
          int arrive_mid = base1 + tr1.arrive_offset[mid];
          Vector<StationRef> **pp2 = station_map_.find(tr1.stations[mid]);
          if (!pp2 || !*pp2) continue;

          Vector<StationRef> &refs2 = **pp2;
          for (int j = 0; j < refs2.size(); ++j) {
            int train2_index = refs2[j].train_idx;
            int mid2 = refs2[j].sta_idx;
            if (train2_index == train1_index) continue;

            Train tr2 = trains_.read(train2_index);
            if (tr2.deleted || !tr2.released) continue;
            int to_index = tr2.find_station(to.c_str());
            if (to_index <= mid2) continue;

            int leave2_from_start = tr2.startTime + tr2.leave_offset[mid2];
            int need_start_day = (arrive_mid - leave2_from_start + 1439) / 1440;
            if (arrive_mid <= leave2_from_start) need_start_day = 0;
            if (need_start_day < tr2.saleBegin) need_start_day = tr2.saleBegin;
            if (need_start_day > tr2.saleEnd) continue;

            int base2 = need_start_day * 1440 + tr2.startTime;
            int leave2 = base2 + tr2.leave_offset[mid2];
            if (leave2 < arrive_mid) {
              ++need_start_day;
              if (need_start_day > tr2.saleEnd) continue;
              base2 = need_start_day * 1440 + tr2.startTime;
              leave2 = base2 + tr2.leave_offset[mid2];
              if (leave2 < arrive_mid) continue;
            }

            TransferCand cand;
            fill_ticket(cand.a, tr1, train1_index, from_index, mid, start_day1);
            fill_ticket(cand.b, tr2, train2_index, mid2, to_index, need_start_day);
            cand.total_time = cand.b.arrive_abs - cand.a.leave_abs;
            cand.total_cost = cand.a.price + cand.b.price;
            cand.ride1_time = cand.a.time_cost;

            if (!found || better_transfer(cand, best, by_time)) {
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

    auto print_leg = [&](const TicketCand &cand) {
      Train tr = trains_.read(cand.train_idx);
      char leave_buf[32], arrive_buf[32];
      format_datetime(cand.leave_abs, leave_buf);
      format_datetime(cand.arrive_abs, arrive_buf);
      std::cout << cand.train_id << ' ' << tr.stations[cand.from] << ' ' << leave_buf
                << " -> " << tr.stations[cand.to] << ' ' << arrive_buf << ' '
                << cand.price << ' ' << cand.seat << '\n';
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

    int user_index = 0, train_index = 0;
    if (!user_idx_.get(u, user_index) || !train_idx_.get(id, train_index)) {
      std::cout << "-1\n";
      return;
    }

    Train tr = trains_.read(train_index);
    if (tr.deleted || !tr.released || n <= 0 || n > tr.seatNum) {
      std::cout << "-1\n";
      return;
    }

    int from_index = tr.find_station(f.c_str());
    int to_index = tr.find_station(t.c_str());
    if (from_index < 0 || to_index <= from_index) {
      std::cout << "-1\n";
      return;
    }

    int day = parse_date(ds);
    int leave_from_start = tr.startTime + tr.leave_offset[from_index];
    int start_day = day - leave_from_start / 1440;
    if (start_day < tr.saleBegin || start_day > tr.saleEnd) {
      std::cout << "-1\n";
      return;
    }

    Order order;
    order.order_id = next_order_id_++;
    order.user_idx = user_index;
    order.train_idx = train_index;
    order.start_day = start_day;
    order.from = from_index;
    order.to = to_index;
    order.num = n;
    order.price = tr.prefix_price[to_index] - tr.prefix_price[from_index];

    int rem = remaining(tr, start_day, from_index, to_index);
    if (rem >= n) {
      change_seats(tr, start_day, from_index, to_index, -n);
      order.status = kSuccess;
      int order_index = orders_.push_back(order);
      user_orders_[user_index].push_back(order_index);
      std::cout << (long long)order.price * n << '\n';
      return;
    }

    if (!queue) {
      std::cout << "-1\n";
      return;
    }

    order.status = kPending;
    int order_index = orders_.push_back(order);
    user_orders_[user_index].push_back(order_index);
    pending_.push_back(order_index);
    std::cout << "queue\n";
  }

  void query_order(const std::string &u) {
    if (!logged_in_.contains(u)) {
      std::cout << "-1\n";
      return;
    }

    int user_index = 0;
    if (!user_idx_.get(u, user_index)) {
      std::cout << "-1\n";
      return;
    }

    Vector<int> &orders = user_orders_[user_index];
    std::cout << orders.size() << '\n';
    for (int i = orders.size() - 1; i >= 0; --i) {
      Order order = orders_.read(orders[i]);
      Train tr = trains_.read(order.train_idx);
      const char *status = order.status == kSuccess
                               ? "success"
                               : (order.status == kPending ? "pending" : "refunded");
      int base = order.start_day * 1440 + tr.startTime;
      char leave_buf[32], arrive_buf[32];
      format_datetime(base + tr.leave_offset[order.from], leave_buf);
      format_datetime(base + tr.arrive_offset[order.to], arrive_buf);
      std::cout << '[' << status << "] " << tr.trainID << ' ' << tr.stations[order.from]
                << ' ' << leave_buf << " -> " << tr.stations[order.to] << ' '
                << arrive_buf << ' ' << order.price << ' ' << order.num << '\n';
    }
  }

  void refund_ticket(const std::string &u, int n) {
    if (!logged_in_.contains(u)) {
      std::cout << "-1\n";
      return;
    }

    int user_index = 0;
    if (!user_idx_.get(u, user_index)) {
      std::cout << "-1\n";
      return;
    }

    Vector<int> &orders = user_orders_[user_index];
    if (n < 1 || n > orders.size()) {
      std::cout << "-1\n";
      return;
    }

    int order_index = orders[orders.size() - n];
    Order order = orders_.read(order_index);
    if (order.status == kRefunded) {
      std::cout << "-1\n";
      return;
    }

    if (order.status == kPending) {
      order.status = kRefunded;
      orders_.update(order_index, order);
      for (int i = 0; i < pending_.size(); ++i) {
        if (pending_[i] == order_index) {
          pending_.erase_at(i);
          break;
        }
      }
      std::cout << "0\n";
      return;
    }

    Train tr = trains_.read(order.train_idx);
    change_seats(tr, order.start_day, order.from, order.to, order.num);
    order.status = kRefunded;
    orders_.update(order_index, order);
    process_queue();
    std::cout << "0\n";
  }

  void clean() {
    clear_all();
    remove_storage_files("");
    open_storage("");
    std::cout << "0\n";
  }

  void exit_cmd() {
    logged_in_.clear();
    save();
    std::cout << "bye\n";
  }
};

#endif
