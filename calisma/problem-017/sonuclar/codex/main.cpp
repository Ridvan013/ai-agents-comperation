#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#define FILE_SEEK std::fseek
#define FILE_TELL std::ftell

namespace {

const char META_FILE[] = "db_meta.bin";
const char SEAT_FILE[] = "db_seats.bin";

const int USERNAME_LEN = 32;
const int PASSWORD_LEN = 36;
const int NAME_LEN = 24;
const int MAIL_LEN = 36;
const int TRAIN_ID_LEN = 32;
const int STATION_LEN = 40;

const int MAX_STATIONS = 100;
const int MAX_DAYS = 92;
const int MAX_TOKENS = 256;

const int ORDER_SUCCESS = 0;
const int ORDER_PENDING = 1;
const int ORDER_REFUNDED = 2;

template <typename T>
struct DynamicArray {
  T *data;
  int size;
  int capacity;

  DynamicArray() : data(nullptr), size(0), capacity(0) {}

  ~DynamicArray() { delete[] data; }

  void reserve(int need) {
    if (need <= capacity) return;
    int next = capacity == 0 ? 8 : capacity;
    while (next < need) next <<= 1;
    T *fresh = new T[next];
    for (int i = 0; i < size; ++i) fresh[i] = data[i];
    delete[] data;
    data = fresh;
    capacity = next;
  }

  int push_back(const T &value) {
    reserve(size + 1);
    data[size] = value;
    return size++;
  }

  void clear() { size = 0; }

  T &operator[](int index) { return data[index]; }

  const T &operator[](int index) const { return data[index]; }
};

struct User {
  char username[USERNAME_LEN];
  char password[PASSWORD_LEN];
  char name[NAME_LEN];
  char mail[MAIL_LEN];
  int privilege;
  int lastOrder;
  int orderCount;
  char loggedIn;
};

struct Train {
  char trainID[TRAIN_ID_LEN];
  int stationIds[MAX_STATIONS];
  int cumPrices[MAX_STATIONS];
  short arriveOffset[MAX_STATIONS];
  short departOffset[MAX_STATIONS];
  short arriveDay[MAX_STATIONS];
  short departDay[MAX_STATIONS];
  int queueHead[MAX_DAYS];
  int queueTail[MAX_DAYS];
  long long seatOffset;
  int seatNum;
  short stationNum;
  short saleStart;
  short saleEnd;
  short startTime;
  char type;
  char released;
  char alive;
};

struct Order {
  int userIndex;
  int trainIndex;
  int price;
  int num;
  int prevUserOrder;
  int prevPending;
  int nextPending;
  short fromPos;
  short toPos;
  short startDay;
  char status;
};

struct Station {
  char name[STATION_LEN];
  int head;
};

struct StationEntry {
  int trainIndex;
  int next;
  short pos;
};

struct TicketResult {
  int trainIndex;
  int fromPos;
  int toPos;
  int startDay;
  int price;
  int seat;
  int departAbs;
  int arriveAbs;
};

struct TransferResult {
  int found;
  int primary;
  int rideOne;
  int trainOne;
  int trainTwo;
  int fromPosOne;
  int midPosOne;
  int midPosTwo;
  int toPosTwo;
  int startDayOne;
  int startDayTwo;
  int priceOne;
  int priceTwo;
  int seatOne;
  int seatTwo;
  int departOneAbs;
  int arriveOneAbs;
  int departTwoAbs;
  int arriveTwoAbs;
};

struct MetaHeader {
  int userCount;
  int trainCount;
  int orderCount;
  int stationCount;
  int stationEntryCount;
};

unsigned int hash_bytes(const char *str) {
  unsigned int hash = 2166136261u;
  while (*str) {
    hash ^= static_cast<unsigned char>(*str++);
    hash *= 16777619u;
  }
  return hash;
}

void copy_text(char *dst, const std::string &src, int limit) {
  int len = static_cast<int>(src.size());
  if (len >= limit) len = limit - 1;
  if (len > 0) std::memcpy(dst, src.data(), len);
  dst[len] = '\0';
}

int parse_int(const std::string &s) {
  int value = 0;
  int sign = 1;
  int i = 0;
  if (!s.empty() && s[0] == '-') {
    sign = -1;
    i = 1;
  }
  for (; i < static_cast<int>(s.size()); ++i) value = value * 10 + (s[i] - '0');
  return value * sign;
}

int parse_time(const std::string &s) {
  return (s[0] - '0') * 600 + (s[1] - '0') * 60 + (s[3] - '0') * 10 + (s[4] - '0');
}

int parse_date(const std::string &s) {
  int month = (s[0] - '0') * 10 + (s[1] - '0');
  int day = (s[3] - '0') * 10 + (s[4] - '0');
  if (month == 6) return day - 1;
  if (month == 7) return 30 + day - 1;
  if (month == 8) return 61 + day - 1;
  return 92 + day - 1;
}

void format_date(int dayIndex, char *out) {
  static const int monthDays[4] = {30, 31, 31, 30};
  static const int monthVals[4] = {6, 7, 8, 9};
  int remain = dayIndex;
  for (int i = 0; i < 4; ++i) {
    if (remain < monthDays[i]) {
      int month = monthVals[i];
      int day = remain + 1;
      std::sprintf(out, "%02d-%02d", month, day);
      return;
    }
    remain -= monthDays[i];
  }
  std::sprintf(out, "09-30");
}

void format_abs_time(int absMinute, char *out) {
  int day = absMinute / 1440;
  int minute = absMinute % 1440;
  if (minute < 0) minute += 1440;
  char date[6];
  format_date(day, date);
  std::sprintf(out, "%s %02d:%02d", date, minute / 60, minute % 60);
}

void tokenize_line(const std::string &line, std::string tokens[], int &count) {
  count = 0;
  int n = static_cast<int>(line.size());
  int i = 0;
  while (i < n) {
    while (i < n && line[i] == ' ') ++i;
    if (i >= n) break;
    int start = i;
    while (i < n && line[i] != ' ') ++i;
    tokens[count++] = line.substr(start, i - start);
  }
}

int split_pipe(const std::string &text, std::string parts[]) {
  int count = 0;
  int start = 0;
  int n = static_cast<int>(text.size());
  for (int i = 0; i <= n; ++i) {
    if (i == n || text[i] == '|') {
      parts[count++] = text.substr(start, i - start);
      start = i + 1;
    }
  }
  return count;
}

struct TicketSystem {
  DynamicArray<User> users;
  DynamicArray<Train> trains;
  DynamicArray<Order> orders;
  DynamicArray<Station> stations;
  DynamicArray<StationEntry> stationEntries;

  int *userHash = nullptr;
  int userHashCap = 0;
  int *trainHash = nullptr;
  int trainHashCap = 0;
  int *stationHash = nullptr;
  int stationHashCap = 0;

  FILE *seatFile = nullptr;

  TicketSystem() = default;

  ~TicketSystem() {
    if (seatFile != nullptr) std::fclose(seatFile);
    delete[] userHash;
    delete[] trainHash;
    delete[] stationHash;
  }

  void init() {
    open_seat_file();
    load_meta();
  }

  void open_seat_file() {
    seatFile = std::fopen(SEAT_FILE, "r+b");
    if (seatFile == nullptr) seatFile = std::fopen(SEAT_FILE, "w+b");
  }

  void rebuild_user_hash() {
    delete[] userHash;
    userHashCap = 16;
    while (userHashCap < users.size * 3 + 8) userHashCap <<= 1;
    userHash = new int[userHashCap];
    for (int i = 0; i < userHashCap; ++i) userHash[i] = 0;
    for (int i = 0; i < users.size; ++i) {
      int slot = locate_user_slot(users[i].username);
      userHash[slot] = i + 1;
    }
  }

  void rebuild_train_hash() {
    delete[] trainHash;
    trainHashCap = 16;
    while (trainHashCap < trains.size * 3 + 8) trainHashCap <<= 1;
    trainHash = new int[trainHashCap];
    for (int i = 0; i < trainHashCap; ++i) trainHash[i] = 0;
    for (int i = 0; i < trains.size; ++i) {
      if (!trains[i].alive) continue;
      int slot = locate_train_slot(trains[i].trainID, true);
      trainHash[slot] = i + 1;
    }
  }

  void rebuild_station_hash() {
    delete[] stationHash;
    stationHashCap = 16;
    while (stationHashCap < stations.size * 3 + 8) stationHashCap <<= 1;
    stationHash = new int[stationHashCap];
    for (int i = 0; i < stationHashCap; ++i) stationHash[i] = 0;
    for (int i = 0; i < stations.size; ++i) {
      int slot = locate_station_slot(stations[i].name);
      stationHash[slot] = i + 1;
    }
  }

  void ensure_user_hash() {
    if (userHash == nullptr || (users.size + 1) * 2 >= userHashCap) rebuild_user_hash();
  }

  void ensure_train_hash() {
    if (trainHash == nullptr || (trains.size + 1) * 2 >= trainHashCap) rebuild_train_hash();
  }

  void ensure_station_hash() {
    if (stationHash == nullptr || (stations.size + 1) * 2 >= stationHashCap) rebuild_station_hash();
  }

  int locate_user_slot(const char *name) const {
    unsigned int h = hash_bytes(name) & (userHashCap - 1);
    while (userHash[h] != 0) {
      int idx = userHash[h] - 1;
      if (std::strcmp(users[idx].username, name) == 0) return static_cast<int>(h);
      h = (h + 1) & (userHashCap - 1);
    }
    return static_cast<int>(h);
  }

  int locate_train_slot(const char *name, bool forInsert) const {
    unsigned int h = hash_bytes(name) & (trainHashCap - 1);
    int firstDeleted = -1;
    while (trainHash[h] != 0) {
      if (trainHash[h] == -1) {
        if (firstDeleted == -1) firstDeleted = static_cast<int>(h);
      } else {
        int idx = trainHash[h] - 1;
        if (trains[idx].alive && std::strcmp(trains[idx].trainID, name) == 0) return static_cast<int>(h);
      }
      h = (h + 1) & (trainHashCap - 1);
    }
    if (forInsert && firstDeleted != -1) return firstDeleted;
    return static_cast<int>(h);
  }

  int locate_station_slot(const char *name) const {
    unsigned int h = hash_bytes(name) & (stationHashCap - 1);
    while (stationHash[h] != 0) {
      int idx = stationHash[h] - 1;
      if (std::strcmp(stations[idx].name, name) == 0) return static_cast<int>(h);
      h = (h + 1) & (stationHashCap - 1);
    }
    return static_cast<int>(h);
  }

  int find_user(const std::string &name) const {
    if (userHash == nullptr) return -1;
    char key[USERNAME_LEN];
    copy_text(key, name, USERNAME_LEN);
    int slot = locate_user_slot(key);
    if (userHash[slot] == 0) return -1;
    return userHash[slot] - 1;
  }

  int find_train(const std::string &name) const {
    if (trainHash == nullptr) return -1;
    char key[TRAIN_ID_LEN];
    copy_text(key, name, TRAIN_ID_LEN);
    int slot = locate_train_slot(key, false);
    if (trainHash[slot] <= 0) return -1;
    int idx = trainHash[slot] - 1;
    if (!trains[idx].alive) return -1;
    return idx;
  }

  int find_station(const std::string &name) const {
    if (stationHash == nullptr) return -1;
    char key[STATION_LEN];
    copy_text(key, name, STATION_LEN);
    int slot = locate_station_slot(key);
    if (stationHash[slot] == 0) return -1;
    return stationHash[slot] - 1;
  }

  int get_or_create_station(const std::string &name) {
    int existing = find_station(name);
    if (existing != -1) return existing;
    ensure_station_hash();
    Station station{};
    copy_text(station.name, name, STATION_LEN);
    station.head = -1;
    int index = stations.push_back(station);
    int slot = locate_station_slot(station.name);
    stationHash[slot] = index + 1;
    return index;
  }

  void load_meta() {
    FILE *fp = std::fopen(META_FILE, "rb");
    if (fp == nullptr) {
      rebuild_user_hash();
      rebuild_train_hash();
      rebuild_station_hash();
      return;
    }
    MetaHeader header{};
    if (std::fread(&header, sizeof(header), 1, fp) != 1) {
      std::fclose(fp);
      rebuild_user_hash();
      rebuild_train_hash();
      rebuild_station_hash();
      return;
    }
    users.reserve(header.userCount);
    trains.reserve(header.trainCount);
    orders.reserve(header.orderCount);
    stations.reserve(header.stationCount);
    stationEntries.reserve(header.stationEntryCount);
    users.size = header.userCount;
    trains.size = header.trainCount;
    orders.size = header.orderCount;
    stations.size = header.stationCount;
    stationEntries.size = header.stationEntryCount;
    if (users.size > 0) std::fread(users.data, sizeof(User), users.size, fp);
    if (trains.size > 0) std::fread(trains.data, sizeof(Train), trains.size, fp);
    if (orders.size > 0) std::fread(orders.data, sizeof(Order), orders.size, fp);
    if (stations.size > 0) std::fread(stations.data, sizeof(Station), stations.size, fp);
    if (stationEntries.size > 0) std::fread(stationEntries.data, sizeof(StationEntry), stationEntries.size, fp);
    std::fclose(fp);
    for (int i = 0; i < users.size; ++i) users[i].loggedIn = 0;
    rebuild_user_hash();
    rebuild_train_hash();
    rebuild_station_hash();
  }

  void save_meta() {
    for (int i = 0; i < users.size; ++i) users[i].loggedIn = 0;
    FILE *fp = std::fopen(META_FILE, "wb");
    if (fp == nullptr) return;
    MetaHeader header{};
    header.userCount = users.size;
    header.trainCount = trains.size;
    header.orderCount = orders.size;
    header.stationCount = stations.size;
    header.stationEntryCount = stationEntries.size;
    std::fwrite(&header, sizeof(header), 1, fp);
    if (users.size > 0) std::fwrite(users.data, sizeof(User), users.size, fp);
    if (trains.size > 0) std::fwrite(trains.data, sizeof(Train), trains.size, fp);
    if (orders.size > 0) std::fwrite(orders.data, sizeof(Order), orders.size, fp);
    if (stations.size > 0) std::fwrite(stations.data, sizeof(Station), stations.size, fp);
    if (stationEntries.size > 0) std::fwrite(stationEntries.data, sizeof(StationEntry), stationEntries.size, fp);
    std::fclose(fp);
    if (seatFile != nullptr) std::fflush(seatFile);
  }

  void reset_all() {
    users.clear();
    trains.clear();
    orders.clear();
    stations.clear();
    stationEntries.clear();
    if (userHash != nullptr) for (int i = 0; i < userHashCap; ++i) userHash[i] = 0;
    if (trainHash != nullptr) for (int i = 0; i < trainHashCap; ++i) trainHash[i] = 0;
    if (stationHash != nullptr) for (int i = 0; i < stationHashCap; ++i) stationHash[i] = 0;
    if (seatFile != nullptr) {
      std::fclose(seatFile);
      seatFile = std::fopen(SEAT_FILE, "w+b");
    }
  }

  void append_empty_seats(Train &train) {
    FILE_SEEK(seatFile, 0, SEEK_END);
    train.seatOffset = FILE_TELL(seatFile);
    int row[MAX_STATIONS];
    for (int i = 0; i < train.stationNum - 1; ++i) row[i] = train.seatNum;
    int days = train.saleEnd - train.saleStart + 1;
    for (int d = 0; d < days; ++d) std::fwrite(row, sizeof(int), train.stationNum - 1, seatFile);
    std::fflush(seatFile);
  }

  void read_seat_row(const Train &train, int startDay, int row[]) {
    long long offset = train.seatOffset + 1LL * (startDay - train.saleStart) * (train.stationNum - 1) * static_cast<int>(sizeof(int));
    FILE_SEEK(seatFile, offset, SEEK_SET);
    std::fread(row, sizeof(int), train.stationNum - 1, seatFile);
  }

  void write_seat_row(const Train &train, int startDay, const int row[]) {
    long long offset = train.seatOffset + 1LL * (startDay - train.saleStart) * (train.stationNum - 1) * static_cast<int>(sizeof(int));
    FILE_SEEK(seatFile, offset, SEEK_SET);
    std::fwrite(row, sizeof(int), train.stationNum - 1, seatFile);
    std::fflush(seatFile);
  }

  int min_range(const int row[], int left, int right) const {
    int result = row[left];
    for (int i = left + 1; i < right; ++i) {
      if (row[i] < result) result = row[i];
    }
    return result;
  }

  void queue_push(Train &train, int startDay, int orderIndex) {
    int dayIdx = startDay - train.saleStart;
    orders[orderIndex].prevPending = train.queueTail[dayIdx];
    orders[orderIndex].nextPending = -1;
    if (train.queueTail[dayIdx] != -1) {
      orders[train.queueTail[dayIdx]].nextPending = orderIndex;
    } else {
      train.queueHead[dayIdx] = orderIndex;
    }
    train.queueTail[dayIdx] = orderIndex;
  }

  void queue_remove(Train &train, int startDay, int orderIndex) {
    int dayIdx = startDay - train.saleStart;
    int prev = orders[orderIndex].prevPending;
    int next = orders[orderIndex].nextPending;
    if (prev != -1) {
      orders[prev].nextPending = next;
    } else {
      train.queueHead[dayIdx] = next;
    }
    if (next != -1) {
      orders[next].prevPending = prev;
    } else {
      train.queueTail[dayIdx] = prev;
    }
    orders[orderIndex].prevPending = -1;
    orders[orderIndex].nextPending = -1;
  }

  void process_pending_queue(Train &train, int startDay, int row[]) {
    int dayIdx = startDay - train.saleStart;
    int current = train.queueHead[dayIdx];
    while (current != -1) {
      int next = orders[current].nextPending;
      if (orders[current].status == ORDER_PENDING) {
        int available = min_range(row, orders[current].fromPos, orders[current].toPos);
        if (available >= orders[current].num) {
          for (int i = orders[current].fromPos; i < orders[current].toPos; ++i) row[i] -= orders[current].num;
          orders[current].status = ORDER_SUCCESS;
          queue_remove(train, startDay, current);
        }
      }
      current = next;
    }
  }

  int find_position_in_train(const Train &train, int stationId) const {
    for (int i = 0; i < train.stationNum; ++i) {
      if (train.stationIds[i] == stationId) return i;
    }
    return -1;
  }

  bool get_trip_positions(const Train &train, int fromStation, int toStation, int &fromPos, int &toPos) const {
    fromPos = -1;
    toPos = -1;
    for (int i = 0; i < train.stationNum; ++i) {
      if (train.stationIds[i] == fromStation) fromPos = i;
      if (fromPos != -1 && train.stationIds[i] == toStation) {
        toPos = i;
        return true;
      }
    }
    return false;
  }

  int train_depart_abs(const Train &train, int startDay, int pos) const {
    return startDay * 1440 + train.startTime + train.departOffset[pos];
  }

  int train_arrive_abs(const Train &train, int startDay, int pos) const {
    return startDay * 1440 + train.startTime + train.arriveOffset[pos];
  }

  void print_user_info(const User &user) {
    std::cout << user.username << ' ' << user.name << ' ' << user.mail << ' ' << user.privilege << '\n';
  }

  void print_ticket_line(int trainIndex, int fromPos, int toPos, int startDay, int price, int seat) {
    const Train &train = trains[trainIndex];
    char depart[20];
    char arrive[20];
    format_abs_time(train_depart_abs(train, startDay, fromPos), depart);
    format_abs_time(train_arrive_abs(train, startDay, toPos), arrive);
    std::cout << train.trainID << ' ' << stations[train.stationIds[fromPos]].name << ' ' << depart
              << " -> " << stations[train.stationIds[toPos]].name << ' ' << arrive << ' ' << price << ' ' << seat << '\n';
  }

  bool transfer_better(const TransferResult &cand, const TransferResult &best) const {
    if (!best.found) return true;
    if (cand.primary != best.primary) return cand.primary < best.primary;
    if (cand.rideOne != best.rideOne) return cand.rideOne < best.rideOne;
    int cmpOne = std::strcmp(trains[cand.trainOne].trainID, trains[best.trainOne].trainID);
    if (cmpOne != 0) return cmpOne < 0;
    int cmpTwo = std::strcmp(trains[cand.trainTwo].trainID, trains[best.trainTwo].trainID);
    if (cmpTwo != 0) return cmpTwo < 0;
    return cand.departOneAbs < best.departOneAbs;
  }

  bool ticket_less(const TicketResult &lhs, const TicketResult &rhs, bool byTime) const {
    if (byTime) {
      int lhsTime = lhs.arriveAbs - lhs.departAbs;
      int rhsTime = rhs.arriveAbs - rhs.departAbs;
      if (lhsTime != rhsTime) return lhsTime < rhsTime;
    } else {
      if (lhs.price != rhs.price) return lhs.price < rhs.price;
    }
    return std::strcmp(trains[lhs.trainIndex].trainID, trains[rhs.trainIndex].trainID) < 0;
  }

  void sort_ticket_results(TicketResult *arr, int left, int right, bool byTime) {
    if (left >= right) return;
    int i = left;
    int j = right;
    TicketResult pivot = arr[(left + right) >> 1];
    while (i <= j) {
      while (ticket_less(arr[i], pivot, byTime)) ++i;
      while (ticket_less(pivot, arr[j], byTime)) --j;
      if (i <= j) {
        TicketResult tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
        ++i;
        --j;
      }
    }
    if (left < j) sort_ticket_results(arr, left, j, byTime);
    if (i < right) sort_ticket_results(arr, i, right, byTime);
  }

  void command_add_user(const std::string &cur, const std::string &username, const std::string &password,
                        const std::string &name, const std::string &mail, int privilege) {
    if (find_user(username) != -1) {
      std::cout << "-1\n";
      return;
    }
    if (users.size != 0) {
      int curIdx = find_user(cur);
      if (curIdx == -1 || !users[curIdx].loggedIn || users[curIdx].privilege <= privilege) {
        std::cout << "-1\n";
        return;
      }
    }
    ensure_user_hash();
    User user{};
    copy_text(user.username, username, USERNAME_LEN);
    copy_text(user.password, password, PASSWORD_LEN);
    copy_text(user.name, name, NAME_LEN);
    copy_text(user.mail, mail, MAIL_LEN);
    user.privilege = users.size == 0 ? 10 : privilege;
    user.lastOrder = -1;
    user.orderCount = 0;
    user.loggedIn = 0;
    int index = users.push_back(user);
    int slot = locate_user_slot(user.username);
    userHash[slot] = index + 1;
    std::cout << "0\n";
  }

  void command_login(const std::string &username, const std::string &password) {
    int userIdx = find_user(username);
    if (userIdx == -1 || users[userIdx].loggedIn || std::strcmp(users[userIdx].password, password.c_str()) != 0) {
      std::cout << "-1\n";
      return;
    }
    users[userIdx].loggedIn = 1;
    std::cout << "0\n";
  }

  void command_logout(const std::string &username) {
    int userIdx = find_user(username);
    if (userIdx == -1 || !users[userIdx].loggedIn) {
      std::cout << "-1\n";
      return;
    }
    users[userIdx].loggedIn = 0;
    std::cout << "0\n";
  }

  void command_query_profile(const std::string &cur, const std::string &target) {
    int curIdx = find_user(cur);
    int targetIdx = find_user(target);
    if (curIdx == -1 || targetIdx == -1 || !users[curIdx].loggedIn) {
      std::cout << "-1\n";
      return;
    }
    if (curIdx != targetIdx && users[curIdx].privilege <= users[targetIdx].privilege) {
      std::cout << "-1\n";
      return;
    }
    print_user_info(users[targetIdx]);
  }

  void command_modify_profile(const std::string &cur, const std::string &target, const std::string *password,
                              const std::string *name, const std::string *mail, const int *privilege) {
    int curIdx = find_user(cur);
    int targetIdx = find_user(target);
    if (curIdx == -1 || targetIdx == -1 || !users[curIdx].loggedIn) {
      std::cout << "-1\n";
      return;
    }
    if (curIdx != targetIdx && users[curIdx].privilege <= users[targetIdx].privilege) {
      std::cout << "-1\n";
      return;
    }
    if (privilege != nullptr && *privilege >= users[curIdx].privilege) {
      std::cout << "-1\n";
      return;
    }
    if (password != nullptr) copy_text(users[targetIdx].password, *password, PASSWORD_LEN);
    if (name != nullptr) copy_text(users[targetIdx].name, *name, NAME_LEN);
    if (mail != nullptr) copy_text(users[targetIdx].mail, *mail, MAIL_LEN);
    if (privilege != nullptr) users[targetIdx].privilege = *privilege;
    print_user_info(users[targetIdx]);
  }

  void command_add_train(const std::string &trainID, int stationNum, int seatNum, const std::string &stationsText,
                         const std::string &pricesText, const std::string &startTimeText, const std::string &travelText,
                         const std::string &stopText, const std::string &saleText, char type) {
    if (find_train(trainID) != -1) {
      std::cout << "-1\n";
      return;
    }
    ensure_train_hash();
    Train train{};
    copy_text(train.trainID, trainID, TRAIN_ID_LEN);
    train.stationNum = static_cast<short>(stationNum);
    train.seatNum = seatNum;
    train.startTime = static_cast<short>(parse_time(startTimeText));
    train.type = type;
    train.released = 0;
    train.alive = 1;
    for (int i = 0; i < MAX_DAYS; ++i) {
      train.queueHead[i] = -1;
      train.queueTail[i] = -1;
    }
    std::string parts[MAX_STATIONS];
    std::string nums[MAX_STATIONS];
    int countStations = split_pipe(stationsText, parts);
    split_pipe(pricesText, nums);
    for (int i = 0; i < countStations; ++i) train.stationIds[i] = get_or_create_station(parts[i]);
    train.cumPrices[0] = 0;
    for (int i = 1; i < countStations; ++i) train.cumPrices[i] = train.cumPrices[i - 1] + parse_int(nums[i - 1]);
    split_pipe(travelText, nums);
    if (!(stopText.size() == 1 && stopText[0] == '_')) split_pipe(stopText, parts);
    train.arriveOffset[0] = 0;
    train.departOffset[0] = 0;
    train.arriveDay[0] = 0;
    train.departDay[0] = 0;
    for (int i = 1; i < countStations; ++i) {
      train.arriveOffset[i] = static_cast<short>(train.departOffset[i - 1] + parse_int(nums[i - 1]));
      train.arriveDay[i] = static_cast<short>((train.startTime + train.arriveOffset[i]) / 1440);
      if (i + 1 < countStations) {
        train.departOffset[i] = static_cast<short>(train.arriveOffset[i] + parse_int(parts[i - 1]));
      } else {
        train.departOffset[i] = train.arriveOffset[i];
      }
      train.departDay[i] = static_cast<short>((train.startTime + train.departOffset[i]) / 1440);
    }
    std::string saleParts[2];
    split_pipe(saleText, saleParts);
    train.saleStart = static_cast<short>(parse_date(saleParts[0]));
    train.saleEnd = static_cast<short>(parse_date(saleParts[1]));
    int index = trains.push_back(train);
    int slot = locate_train_slot(train.trainID, true);
    trainHash[slot] = index + 1;
    std::cout << "0\n";
  }

  void command_delete_train(const std::string &trainID) {
    int trainIdx = find_train(trainID);
    if (trainIdx == -1 || trains[trainIdx].released) {
      std::cout << "-1\n";
      return;
    }
    char key[TRAIN_ID_LEN];
    copy_text(key, trainID, TRAIN_ID_LEN);
    int slot = locate_train_slot(key, false);
    if (slot >= 0 && trainHash[slot] > 0) trainHash[slot] = -1;
    trains[trainIdx].alive = 0;
    std::cout << "0\n";
  }

  void command_release_train(const std::string &trainID) {
    int trainIdx = find_train(trainID);
    if (trainIdx == -1 || trains[trainIdx].released) {
      std::cout << "-1\n";
      return;
    }
    Train &train = trains[trainIdx];
    append_empty_seats(train);
    train.released = 1;
    for (int i = 0; i < train.stationNum; ++i) {
      StationEntry entry{};
      entry.trainIndex = trainIdx;
      entry.pos = static_cast<short>(i);
      entry.next = stations[train.stationIds[i]].head;
      int entryIndex = stationEntries.push_back(entry);
      stations[train.stationIds[i]].head = entryIndex;
    }
    std::cout << "0\n";
  }

  void command_query_train(const std::string &trainID, int date) {
    int trainIdx = find_train(trainID);
    if (trainIdx == -1) {
      std::cout << "-1\n";
      return;
    }
    const Train &train = trains[trainIdx];
    if (date < train.saleStart || date > train.saleEnd) {
      std::cout << "-1\n";
      return;
    }
    int seats[MAX_STATIONS];
    if (train.released) {
      read_seat_row(train, date, seats);
    } else {
      for (int i = 0; i < train.stationNum - 1; ++i) seats[i] = train.seatNum;
    }
    std::cout << train.trainID << ' ' << train.type << '\n';
    for (int i = 0; i < train.stationNum; ++i) {
      std::cout << stations[train.stationIds[i]].name << ' ';
      if (i == 0) {
        std::cout << "xx-xx xx:xx";
      } else {
        char timeText[20];
        format_abs_time(train_arrive_abs(train, date, i), timeText);
        std::cout << timeText;
      }
      std::cout << " -> ";
      if (i + 1 == train.stationNum) {
        std::cout << "xx-xx xx:xx";
      } else {
        char timeText[20];
        format_abs_time(train_depart_abs(train, date, i), timeText);
        std::cout << timeText;
      }
      std::cout << ' ' << train.cumPrices[i] << ' ';
      if (i + 1 == train.stationNum) {
        std::cout << 'x';
      } else {
        std::cout << seats[i];
      }
      std::cout << '\n';
    }
  }

  void command_query_ticket(const std::string &from, const std::string &to, int queryDay, bool sortByTime) {
    int fromStation = find_station(from);
    int toStation = find_station(to);
    if (fromStation == -1 || toStation == -1) {
      std::cout << "0\n";
      return;
    }
    DynamicArray<TicketResult> results;
    for (int entry = stations[fromStation].head; entry != -1; entry = stationEntries[entry].next) {
      int trainIdx = stationEntries[entry].trainIndex;
      const Train &train = trains[trainIdx];
      int fromPos = stationEntries[entry].pos;
      int toPos = -1;
      for (int i = fromPos + 1; i < train.stationNum; ++i) {
        if (train.stationIds[i] == toStation) {
          toPos = i;
          break;
        }
      }
      if (toPos == -1) continue;
      int startDay = queryDay - train.departDay[fromPos];
      if (startDay < train.saleStart || startDay > train.saleEnd) continue;
      int seats[MAX_STATIONS];
      read_seat_row(train, startDay, seats);
      TicketResult result{};
      result.trainIndex = trainIdx;
      result.fromPos = fromPos;
      result.toPos = toPos;
      result.startDay = startDay;
      result.price = train.cumPrices[toPos] - train.cumPrices[fromPos];
      result.seat = min_range(seats, fromPos, toPos);
      result.departAbs = train_depart_abs(train, startDay, fromPos);
      result.arriveAbs = train_arrive_abs(train, startDay, toPos);
      results.push_back(result);
    }
    if (results.size > 1) sort_ticket_results(results.data, 0, results.size - 1, sortByTime);
    std::cout << results.size << '\n';
    for (int i = 0; i < results.size; ++i) {
      print_ticket_line(results[i].trainIndex, results[i].fromPos, results[i].toPos, results[i].startDay, results[i].price, results[i].seat);
    }
  }

  void command_query_transfer(const std::string &from, const std::string &to, int queryDay, bool byTime) {
    int fromStation = find_station(from);
    int toStation = find_station(to);
    if (fromStation == -1 || toStation == -1) {
      std::cout << "0\n";
      return;
    }
    TransferResult best{};
    best.found = 0;
    for (int entryOne = stations[fromStation].head; entryOne != -1; entryOne = stationEntries[entryOne].next) {
      int trainOneIdx = stationEntries[entryOne].trainIndex;
      const Train &trainOne = trains[trainOneIdx];
      int fromPosOne = stationEntries[entryOne].pos;
      int startDayOne = queryDay - trainOne.departDay[fromPosOne];
      if (startDayOne < trainOne.saleStart || startDayOne > trainOne.saleEnd) continue;
      int seatsOne[MAX_STATIONS];
      read_seat_row(trainOne, startDayOne, seatsOne);
      int seatPrefix[MAX_STATIONS];
      seatPrefix[fromPosOne] = seatsOne[fromPosOne];
      for (int i = fromPosOne + 1; i < trainOne.stationNum - 1; ++i) {
        seatPrefix[i] = seatPrefix[i - 1] < seatsOne[i] ? seatPrefix[i - 1] : seatsOne[i];
      }
      int departOneAbs = train_depart_abs(trainOne, startDayOne, fromPosOne);
      for (int midPosOne = fromPosOne + 1; midPosOne < trainOne.stationNum; ++midPosOne) {
        int transferStation = trainOne.stationIds[midPosOne];
        int arriveOneAbs = train_arrive_abs(trainOne, startDayOne, midPosOne);
        int priceOne = trainOne.cumPrices[midPosOne] - trainOne.cumPrices[fromPosOne];
        int seatOne = seatPrefix[midPosOne - 1];
        for (int entryTwo = stations[transferStation].head; entryTwo != -1; entryTwo = stationEntries[entryTwo].next) {
          int trainTwoIdx = stationEntries[entryTwo].trainIndex;
          if (trainTwoIdx == trainOneIdx) continue;
          const Train &trainTwo = trains[trainTwoIdx];
          int midPosTwo = stationEntries[entryTwo].pos;
          int toPosTwo = -1;
          for (int i = midPosTwo + 1; i < trainTwo.stationNum; ++i) {
            if (trainTwo.stationIds[i] == toStation) {
              toPosTwo = i;
              break;
            }
          }
          if (toPosTwo == -1) continue;
          int baseDepartTwo = trainTwo.startTime + trainTwo.departOffset[midPosTwo];
          int startDayTwo = 0;
          if (arriveOneAbs > baseDepartTwo) startDayTwo = (arriveOneAbs - baseDepartTwo + 1439) / 1440;
          if (startDayTwo < trainTwo.saleStart) startDayTwo = trainTwo.saleStart;
          if (startDayTwo > trainTwo.saleEnd) continue;
          int departTwoAbs = train_depart_abs(trainTwo, startDayTwo, midPosTwo);
          if (departTwoAbs < arriveOneAbs) continue;
          int seatsTwo[MAX_STATIONS];
          read_seat_row(trainTwo, startDayTwo, seatsTwo);
          int seatTwo = min_range(seatsTwo, midPosTwo, toPosTwo);
          int priceTwo = trainTwo.cumPrices[toPosTwo] - trainTwo.cumPrices[midPosTwo];
          TransferResult cand{};
          cand.found = 1;
          cand.trainOne = trainOneIdx;
          cand.trainTwo = trainTwoIdx;
          cand.fromPosOne = fromPosOne;
          cand.midPosOne = midPosOne;
          cand.midPosTwo = midPosTwo;
          cand.toPosTwo = toPosTwo;
          cand.startDayOne = startDayOne;
          cand.startDayTwo = startDayTwo;
          cand.priceOne = priceOne;
          cand.priceTwo = priceTwo;
          cand.seatOne = seatOne;
          cand.seatTwo = seatTwo;
          cand.departOneAbs = departOneAbs;
          cand.arriveOneAbs = arriveOneAbs;
          cand.departTwoAbs = departTwoAbs;
          cand.arriveTwoAbs = train_arrive_abs(trainTwo, startDayTwo, toPosTwo);
          cand.rideOne = arriveOneAbs - departOneAbs;
          cand.primary = byTime ? cand.arriveTwoAbs - departOneAbs : priceOne + priceTwo;
          if (transfer_better(cand, best)) best = cand;
        }
      }
    }
    if (!best.found) {
      std::cout << "0\n";
      return;
    }
    print_ticket_line(best.trainOne, best.fromPosOne, best.midPosOne, best.startDayOne, best.priceOne, best.seatOne);
    print_ticket_line(best.trainTwo, best.midPosTwo, best.toPosTwo, best.startDayTwo, best.priceTwo, best.seatTwo);
  }

  void command_buy_ticket(const std::string &username, const std::string &trainID, int boardDay, int count,
                          const std::string &from, const std::string &to, bool allowQueue) {
    int userIdx = find_user(username);
    int trainIdx = find_train(trainID);
    if (userIdx == -1 || !users[userIdx].loggedIn || trainIdx == -1 || !trains[trainIdx].released || count <= 0) {
      std::cout << "-1\n";
      return;
    }
    Train &train = trains[trainIdx];
    int fromStation = find_station(from);
    int toStation = find_station(to);
    int fromPos, toPos;
    if (fromStation == -1 || toStation == -1 || !get_trip_positions(train, fromStation, toStation, fromPos, toPos) || count > train.seatNum) {
      std::cout << "-1\n";
      return;
    }
    int startDay = boardDay - train.departDay[fromPos];
    if (startDay < train.saleStart || startDay > train.saleEnd) {
      std::cout << "-1\n";
      return;
    }
    int row[MAX_STATIONS];
    read_seat_row(train, startDay, row);
    int available = min_range(row, fromPos, toPos);
    int price = train.cumPrices[toPos] - train.cumPrices[fromPos];
    Order order{};
    order.userIndex = userIdx;
    order.trainIndex = trainIdx;
    order.price = price;
    order.num = count;
    order.prevUserOrder = users[userIdx].lastOrder;
    order.prevPending = -1;
    order.nextPending = -1;
    order.fromPos = static_cast<short>(fromPos);
    order.toPos = static_cast<short>(toPos);
    order.startDay = static_cast<short>(startDay);
    if (available >= count) {
      for (int i = fromPos; i < toPos; ++i) row[i] -= count;
      write_seat_row(train, startDay, row);
      order.status = ORDER_SUCCESS;
      users[userIdx].lastOrder = orders.push_back(order);
      users[userIdx].orderCount++;
      std::cout << price * count << '\n';
      return;
    }
    if (!allowQueue) {
      std::cout << "-1\n";
      return;
    }
    order.status = ORDER_PENDING;
    int orderIdx = orders.push_back(order);
    users[userIdx].lastOrder = orderIdx;
    users[userIdx].orderCount++;
    queue_push(train, startDay, orderIdx);
    std::cout << "queue\n";
  }

  void command_query_order(const std::string &username) {
    int userIdx = find_user(username);
    if (userIdx == -1 || !users[userIdx].loggedIn) {
      std::cout << "-1\n";
      return;
    }
    std::cout << users[userIdx].orderCount << '\n';
    int current = users[userIdx].lastOrder;
    while (current != -1) {
      const Order &order = orders[current];
      if (order.status == ORDER_SUCCESS) {
        std::cout << "[success] ";
      } else if (order.status == ORDER_PENDING) {
        std::cout << "[pending] ";
      } else {
        std::cout << "[refunded] ";
      }
      print_ticket_line(order.trainIndex, order.fromPos, order.toPos, order.startDay, order.price, order.num);
      current = order.prevUserOrder;
    }
  }

  void command_refund_ticket(const std::string &username, int nth) {
    int userIdx = find_user(username);
    if (userIdx == -1 || !users[userIdx].loggedIn || nth <= 0 || nth > users[userIdx].orderCount) {
      std::cout << "-1\n";
      return;
    }
    int current = users[userIdx].lastOrder;
    for (int i = 1; i < nth && current != -1; ++i) current = orders[current].prevUserOrder;
    if (current == -1 || orders[current].status == ORDER_REFUNDED) {
      std::cout << "-1\n";
      return;
    }
    Order &order = orders[current];
    Train &train = trains[order.trainIndex];
    if (order.status == ORDER_PENDING) {
      queue_remove(train, order.startDay, current);
      order.status = ORDER_REFUNDED;
      std::cout << "0\n";
      return;
    }
    int row[MAX_STATIONS];
    read_seat_row(train, order.startDay, row);
    for (int i = order.fromPos; i < order.toPos; ++i) row[i] += order.num;
    order.status = ORDER_REFUNDED;
    process_pending_queue(train, order.startDay, row);
    write_seat_row(train, order.startDay, row);
    std::cout << "0\n";
  }

  void command_clean() {
    reset_all();
    std::cout << "0\n";
  }

  void process_command(const std::string &line) {
    std::string tokens[MAX_TOKENS];
    int tokenCount = 0;
    tokenize_line(line, tokens, tokenCount);
    if (tokenCount == 0) return;
    const std::string &cmd = tokens[0];
    if (cmd == "add_user") {
      std::string cur, username, password, name, mail;
      int privilege = 0;
      for (int i = 1; i + 1 < tokenCount; i += 2) {
        if (tokens[i] == "-c") cur = tokens[i + 1];
        else if (tokens[i] == "-u") username = tokens[i + 1];
        else if (tokens[i] == "-p") password = tokens[i + 1];
        else if (tokens[i] == "-n") name = tokens[i + 1];
        else if (tokens[i] == "-m") mail = tokens[i + 1];
        else if (tokens[i] == "-g") privilege = parse_int(tokens[i + 1]);
      }
      command_add_user(cur, username, password, name, mail, privilege);
      return;
    }
    if (cmd == "login") {
      std::string username, password;
      for (int i = 1; i + 1 < tokenCount; i += 2) {
        if (tokens[i] == "-u") username = tokens[i + 1];
        else if (tokens[i] == "-p") password = tokens[i + 1];
      }
      command_login(username, password);
      return;
    }
    if (cmd == "logout") {
      std::string username;
      for (int i = 1; i + 1 < tokenCount; i += 2) if (tokens[i] == "-u") username = tokens[i + 1];
      command_logout(username);
      return;
    }
    if (cmd == "query_profile") {
      std::string cur, username;
      for (int i = 1; i + 1 < tokenCount; i += 2) {
        if (tokens[i] == "-c") cur = tokens[i + 1];
        else if (tokens[i] == "-u") username = tokens[i + 1];
      }
      command_query_profile(cur, username);
      return;
    }
    if (cmd == "modify_profile") {
      std::string cur, username;
      std::string password, name, mail;
      int privilege = 0;
      bool hasPassword = false, hasName = false, hasMail = false, hasPrivilege = false;
      for (int i = 1; i + 1 < tokenCount; i += 2) {
        if (tokens[i] == "-c") cur = tokens[i + 1];
        else if (tokens[i] == "-u") username = tokens[i + 1];
        else if (tokens[i] == "-p") password = tokens[i + 1], hasPassword = true;
        else if (tokens[i] == "-n") name = tokens[i + 1], hasName = true;
        else if (tokens[i] == "-m") mail = tokens[i + 1], hasMail = true;
        else if (tokens[i] == "-g") privilege = parse_int(tokens[i + 1]), hasPrivilege = true;
      }
      command_modify_profile(cur, username, hasPassword ? &password : nullptr, hasName ? &name : nullptr,
                             hasMail ? &mail : nullptr, hasPrivilege ? &privilege : nullptr);
      return;
    }
    if (cmd == "add_train") {
      std::string trainID, stationsText, pricesText, startTimeText, travelText, stopText, saleText;
      int stationNum = 0, seatNum = 0;
      char type = 'A';
      for (int i = 1; i + 1 < tokenCount; i += 2) {
        if (tokens[i] == "-i") trainID = tokens[i + 1];
        else if (tokens[i] == "-n") stationNum = parse_int(tokens[i + 1]);
        else if (tokens[i] == "-m") seatNum = parse_int(tokens[i + 1]);
        else if (tokens[i] == "-s") stationsText = tokens[i + 1];
        else if (tokens[i] == "-p") pricesText = tokens[i + 1];
        else if (tokens[i] == "-x") startTimeText = tokens[i + 1];
        else if (tokens[i] == "-t") travelText = tokens[i + 1];
        else if (tokens[i] == "-o") stopText = tokens[i + 1];
        else if (tokens[i] == "-d") saleText = tokens[i + 1];
        else if (tokens[i] == "-y") type = tokens[i + 1][0];
      }
      command_add_train(trainID, stationNum, seatNum, stationsText, pricesText, startTimeText, travelText, stopText, saleText, type);
      return;
    }
    if (cmd == "release_train") {
      std::string trainID;
      for (int i = 1; i + 1 < tokenCount; i += 2) if (tokens[i] == "-i") trainID = tokens[i + 1];
      command_release_train(trainID);
      return;
    }
    if (cmd == "query_train") {
      std::string trainID, dateText;
      for (int i = 1; i + 1 < tokenCount; i += 2) {
        if (tokens[i] == "-i") trainID = tokens[i + 1];
        else if (tokens[i] == "-d") dateText = tokens[i + 1];
      }
      command_query_train(trainID, parse_date(dateText));
      return;
    }
    if (cmd == "delete_train") {
      std::string trainID;
      for (int i = 1; i + 1 < tokenCount; i += 2) if (tokens[i] == "-i") trainID = tokens[i + 1];
      command_delete_train(trainID);
      return;
    }
    if (cmd == "query_ticket") {
      std::string from, to, dateText, mode = "cost";
      for (int i = 1; i + 1 < tokenCount; i += 2) {
        if (tokens[i] == "-s") from = tokens[i + 1];
        else if (tokens[i] == "-t") to = tokens[i + 1];
        else if (tokens[i] == "-d") dateText = tokens[i + 1];
        else if (tokens[i] == "-p") mode = tokens[i + 1];
      }
      command_query_ticket(from, to, parse_date(dateText), mode == "time");
      return;
    }
    if (cmd == "query_transfer") {
      std::string from, to, dateText, mode = "cost";
      for (int i = 1; i + 1 < tokenCount; i += 2) {
        if (tokens[i] == "-s") from = tokens[i + 1];
        else if (tokens[i] == "-t") to = tokens[i + 1];
        else if (tokens[i] == "-d") dateText = tokens[i + 1];
        else if (tokens[i] == "-p") mode = tokens[i + 1];
      }
      command_query_transfer(from, to, parse_date(dateText), mode == "time");
      return;
    }
    if (cmd == "buy_ticket") {
      std::string username, trainID, dateText, from, to, queueMode = "false";
      int count = 0;
      for (int i = 1; i + 1 < tokenCount; i += 2) {
        if (tokens[i] == "-u") username = tokens[i + 1];
        else if (tokens[i] == "-i") trainID = tokens[i + 1];
        else if (tokens[i] == "-d") dateText = tokens[i + 1];
        else if (tokens[i] == "-n") count = parse_int(tokens[i + 1]);
        else if (tokens[i] == "-f") from = tokens[i + 1];
        else if (tokens[i] == "-t") to = tokens[i + 1];
        else if (tokens[i] == "-q") queueMode = tokens[i + 1];
      }
      command_buy_ticket(username, trainID, parse_date(dateText), count, from, to, queueMode == "true");
      return;
    }
    if (cmd == "query_order") {
      std::string username;
      for (int i = 1; i + 1 < tokenCount; i += 2) if (tokens[i] == "-u") username = tokens[i + 1];
      command_query_order(username);
      return;
    }
    if (cmd == "refund_ticket") {
      std::string username;
      int nth = 1;
      for (int i = 1; i + 1 < tokenCount; i += 2) {
        if (tokens[i] == "-u") username = tokens[i + 1];
        else if (tokens[i] == "-n") nth = parse_int(tokens[i + 1]);
      }
      command_refund_ticket(username, nth);
      return;
    }
    if (cmd == "clean") {
      command_clean();
      return;
    }
    if (cmd == "exit") {
      save_meta();
      std::cout << "bye\n";
      std::cout.flush();
      std::exit(0);
    }
  }
};

}  // namespace

int main() {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  TicketSystem system;
  system.init();

  std::string line;
  while (std::getline(std::cin, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    system.process_command(line);
  }
  system.save_meta();
  return 0;
}
