#ifndef SJTU_MODELS_HPP
#define SJTU_MODELS_HPP

#include <cstring>
#include <string>

namespace sjtu {

struct String24 {
    char str[24];
    String24() { str[0] = '\0'; }
    String24(const std::string& s) {
        strncpy(str, s.c_str(), sizeof(str) - 1);
        str[sizeof(str) - 1] = '\0';
    }
    String24(const char* s) {
        strncpy(str, s, sizeof(str) - 1);
        str[sizeof(str) - 1] = '\0';
    }
    bool operator<(const String24& o) const {
        return strcmp(str, o.str) < 0;
    }
    bool operator==(const String24& o) const {
        return strcmp(str, o.str) == 0;
    }
};

struct UserData {
    char password[32];
    char name[24];
    char mailAddr[32];
    int privilege;
};

struct TrainData {
    int stationNum;
    int seatNum;
    char stations[100][32]; // 100 stations, each up to 30 bytes (UTF-8 10 chars)
    int prices[100];
    int startTime; // in minutes from 00:00
    int travelTimes[100];
    int stopoverTimes[100];
    int saleDateL; // day index (e.g., from June 1)
    int saleDateR;
    char type;
    bool released;
};

struct StationTrainKey {
    char station[32];
    char trainID[24];
    StationTrainKey() { station[0] = '\0'; trainID[0] = '\0'; }
    StationTrainKey(const std::string& s, const std::string& t) {
        strncpy(station, s.c_str(), sizeof(station) - 1);
        station[sizeof(station) - 1] = '\0';
        strncpy(trainID, t.c_str(), sizeof(trainID) - 1);
        trainID[sizeof(trainID) - 1] = '\0';
    }
    bool operator<(const StationTrainKey& o) const {
        int cmp = strcmp(station, o.station);
        if (cmp != 0) return cmp < 0;
        return strcmp(trainID, o.trainID) < 0;
    }
};

struct SeatKey {
    char trainID[24];
    int date; // departure date of train at station 0
    SeatKey() { trainID[0] = '\0'; date = 0; }
    SeatKey(const std::string& t, int d) {
        strncpy(trainID, t.c_str(), sizeof(trainID) - 1);
        trainID[sizeof(trainID) - 1] = '\0';
        date = d;
    }
    bool operator<(const SeatKey& o) const {
        int cmp = strcmp(trainID, o.trainID);
        if (cmp != 0) return cmp < 0;
        return date < o.date;
    }
};

struct SeatData {
    int seats[100];
};

struct OrderKey {
    char username[24];
    int orderID;
    OrderKey() { username[0] = '\0'; orderID = 0; }
    OrderKey(const std::string& u, int id) {
        strncpy(username, u.c_str(), sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
        orderID = id;
    }
    bool operator<(const OrderKey& o) const {
        int cmp = strcmp(username, o.username);
        if (cmp != 0) return cmp < 0;
        return orderID > o.orderID; // Sort by orderID descending so newest is first? 
        // Wait, `query_order` asks for newest to oldest. So sorting by ID desc is perfect.
    }
};

enum OrderStatus {
    SUCCESS,
    PENDING,
    REFUNDED
};

struct OrderData {
    OrderStatus status;
    char trainID[24];
    char from[32];
    char to[32];
    int date; // the date leaving `from`
    int price;
    int num;
    int leavingTime; // absolute time
    int arrivingTime; // absolute time
    int timestamp; // used to uniquely identify order or for queue
    int originTrainDate; // date leaving starting station
    int fromIdx;
    int toIdx;
};

struct QueueKey {
    char trainID[24];
    int date; // date leaving starting station
    int timestamp; // original timestamp of the order
    QueueKey() { trainID[0] = '\0'; date = 0; timestamp = 0; }
    QueueKey(const std::string& t, int d, int ts) {
        strncpy(trainID, t.c_str(), sizeof(trainID) - 1);
        trainID[sizeof(trainID) - 1] = '\0';
        date = d;
        timestamp = ts;
    }
    bool operator<(const QueueKey& o) const {
        int cmp = strcmp(trainID, o.trainID);
        if (cmp != 0) return cmp < 0;
        if (date != o.date) return date < o.date;
        return timestamp < o.timestamp; // Ascending order
    }
};

struct QueueData {
    char username[24];
    int orderID;
    int fromIdx;
    int toIdx;
    int num;
};

} // namespace sjtu

#endif // SJTU_MODELS_HPP
