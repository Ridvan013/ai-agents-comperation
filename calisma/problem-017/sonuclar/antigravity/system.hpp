#ifndef SJTU_SYSTEM_HPP
#define SJTU_SYSTEM_HPP

#include "models.hpp"
#include "bptree.hpp"
#include "utility.hpp"
#include "vector.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>

namespace sjtu {

class TicketSystem {
private:
    BPTree<String24, UserData, 100, 200> users;
    BPTree<String24, TrainData, 10, 100> trains;
    BPTree<StationTrainKey, int, 100, 300> station_trains;
    BPTree<SeatKey, SeatData, 50, 100> seats;
    BPTree<OrderKey, OrderData, 100, 200> orders;
    BPTree<QueueKey, QueueData, 100, 100> pending;

    // In-memory data
    // std::map or vector of logged-in users.
    // The requirement says multiple clients can log in same user? 
    // "each frontend client can log in one user... their login status can coexist"
    // Wait, Q: "Will repeated login fail?" A: "Yes, return -1."
    // So we just maintain a simple vector/list of currently logged in users.
    struct Session {
        String24 username;
        int privilege;
    };
    sjtu::vector<Session> sessions;

    int order_id_counter = 0;
    int queue_timestamp_counter = 0;
    int user_count = 0;

    int date_to_int(const std::string& d) {
        if (d.length() != 5) return 0;
        int m = (d[0] - '0') * 10 + (d[1] - '0');
        int day = (d[3] - '0') * 10 + (d[4] - '0');
        // days from June 1
        int days[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        int res = day - 1;
        for (int i = 6; i < m; ++i) res += days[i];
        return res;
    }

    std::string int_to_date(int d) {
        if (d < 0) return "xx-xx";
        int days[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        int m = 6;
        while (d >= days[m]) {
            d -= days[m];
            m++;
        }
        int day = d + 1;
        char buf[6];
        sprintf(buf, "%02d-%02d", m, day);
        return std::string(buf);
    }

    int time_to_int(const std::string& t) {
        if (t.length() != 5) return 0;
        int h = (t[0] - '0') * 10 + (t[1] - '0');
        int m = (t[3] - '0') * 10 + (t[4] - '0');
        return h * 60 + m;
    }

    std::string int_to_time(int t) {
        if (t < 0) return "xx:xx";
        int h = (t / 60) % 24;
        int m = t % 60;
        char buf[6];
        sprintf(buf, "%02d:%02d", h, m);
        return std::string(buf);
    }
    
    std::string format_datetime(int date, int time) {
        if (date < 0 || time < 0) return "xx-xx xx:xx";
        int total_mins = time;
        int extra_days = total_mins / (24 * 60);
        total_mins %= (24 * 60);
        return int_to_date(date + extra_days) + " " + int_to_time(total_mins);
    }

    int get_privilege(const String24& u) {
        for (int i = 0; i < sessions.size(); ++i) {
            if (sessions[i].username == u) return sessions[i].privilege;
        }
        return -1;
    }

    bool is_logged_in(const String24& u) {
        return get_privilege(u) != -1;
    }

public:
    TicketSystem() 
        : users("users.dat"), trains("trains.dat"), station_trains("station_trains.dat"),
          seats("seats.dat"), orders("orders.dat"), pending("pending.dat") {
        // Find order counter by loading state or we can just make it unique by checking from 0
        // To be safe, order ID can just be a global counter.
        // Wait, order counter needs to be persistent across restarts!
        // We can find the max orderID by querying each user's first order (newest), but it's hard.
        // Or store global config in a file.
        std::fstream config("config.dat", std::ios::in | std::ios::binary);
        if (config.is_open()) {
            config.read(reinterpret_cast<char*>(&order_id_counter), sizeof(int));
            config.read(reinterpret_cast<char*>(&queue_timestamp_counter), sizeof(int));
            config.read(reinterpret_cast<char*>(&user_count), sizeof(int));
            config.close();
        } else {
            order_id_counter = 0;
            queue_timestamp_counter = 0;
            user_count = 0;
            save_config();
        }
    }

    ~TicketSystem() {
        save_config();
    }

    void save_config() {
        std::fstream config("config.dat", std::ios::out | std::ios::binary);
        config.write(reinterpret_cast<char*>(&order_id_counter), sizeof(int));
        config.write(reinterpret_cast<char*>(&queue_timestamp_counter), sizeof(int));
        config.write(reinterpret_cast<char*>(&user_count), sizeof(int));
        config.close();
    }

    void clear() {
        users.clear();
        trains.clear();
        station_trains.clear();
        seats.clear();
        orders.clear();
        pending.clear();
        sessions.clear();
        order_id_counter = 0;
        queue_timestamp_counter = 0;
        user_count = 0;
        save_config();
        std::cout << "0\n";
    }

    void exit() {
        std::cout << "bye\n";
    }

    void add_user(const std::string& c, const std::string& u, const std::string& p, const std::string& n, const std::string& m, int g) {
        String24 u_key(u);
        UserData dummy;
        if (users.find(u_key, dummy)) {
            std::cout << "-1\n";
            return;
        }

        UserData new_user;
        strncpy(new_user.password, p.c_str(), sizeof(new_user.password) - 1);
        new_user.password[sizeof(new_user.password) - 1] = '\0';
        strncpy(new_user.name, n.c_str(), sizeof(new_user.name) - 1);
        new_user.name[sizeof(new_user.name) - 1] = '\0';
        strncpy(new_user.mailAddr, m.c_str(), sizeof(new_user.mailAddr) - 1);
        new_user.mailAddr[sizeof(new_user.mailAddr) - 1] = '\0';
        new_user.privilege = g;

        if (user_count == 0) {
            new_user.privilege = 10;
        } else {
            String24 c_key(c);
            int cur_privilege = get_privilege(c_key);
            if (cur_privilege == -1 || g >= cur_privilege) {
                std::cout << "-1\n";
                return;
            }
        }

        users.insert(u_key, new_user);
        user_count++;
        std::cout << "0\n";
    }

    void login(const std::string& u, const std::string& p) {
        String24 u_key(u);
        if (is_logged_in(u_key)) {
            std::cout << "-1\n";
            return;
        }
        UserData ud;
        if (!users.find(u_key, ud) || strcmp(ud.password, p.c_str()) != 0) {
            std::cout << "-1\n";
            return;
        }
        sessions.push_back({u_key, ud.privilege});
        std::cout << "0\n";
    }

    void logout(const std::string& u) {
        String24 u_key(u);
        for (size_t i = 0; i < sessions.size(); ++i) {
            if (sessions[i].username == u_key) {
                sessions[i] = sessions[sessions.size() - 1];
                sessions.pop_back();
                std::cout << "0\n";
                return;
            }
        }
        std::cout << "-1\n";
    }

    void query_profile(const std::string& c, const std::string& u) {
        String24 c_key(c), u_key(u);
        int cur_privilege = get_privilege(c_key);
        if (cur_privilege == -1) {
            std::cout << "-1\n";
            return;
        }
        UserData ud;
        if (!users.find(u_key, ud)) {
            std::cout << "-1\n";
            return;
        }
        if (cur_privilege <= ud.privilege && !(c_key == u_key)) {
            std::cout << "-1\n";
            return;
        }
        std::cout << u << " " << ud.name << " " << ud.mailAddr << " " << ud.privilege << "\n";
    }

    void modify_profile(const std::string& c, const std::string& u, const std::string& p, const std::string& n, const std::string& m, int g) {
        String24 c_key(c), u_key(u);
        int cur_privilege = get_privilege(c_key);
        if (cur_privilege == -1) {
            std::cout << "-1\n";
            return;
        }
        UserData ud;
        if (!users.find(u_key, ud)) {
            std::cout << "-1\n";
            return;
        }
        if (cur_privilege <= ud.privilege && !(c_key == u_key)) {
            std::cout << "-1\n";
            return;
        }
        if (g != -1 && g >= cur_privilege) {
            std::cout << "-1\n";
            return;
        }
        
        if (!p.empty()) {
            strncpy(ud.password, p.c_str(), sizeof(ud.password) - 1);
            ud.password[sizeof(ud.password) - 1] = '\0';
        }
        if (!n.empty()) {
            strncpy(ud.name, n.c_str(), sizeof(ud.name) - 1);
            ud.name[sizeof(ud.name) - 1] = '\0';
        }
        if (!m.empty()) {
            strncpy(ud.mailAddr, m.c_str(), sizeof(ud.mailAddr) - 1);
            ud.mailAddr[sizeof(ud.mailAddr) - 1] = '\0';
        }
        if (g != -1) {
            ud.privilege = g;
            // Also update session privilege if it's the same user or someone logged in
            for (size_t i = 0; i < sessions.size(); ++i) {
                if (sessions[i].username == u_key) {
                    sessions[i].privilege = g;
                }
            }
        }
        
        users.insert(u_key, ud); // Since our B+ tree insert acts as update if key exists
        std::cout << u << " " << ud.name << " " << ud.mailAddr << " " << ud.privilege << "\n";
    }
    sjtu::vector<std::string> split(const std::string& s, char delim) {
        sjtu::vector<std::string> res;
        size_t start = 0;
        for (size_t i = 0; i <= s.length(); ++i) {
            if (i == s.length() || s[i] == delim) {
                res.push_back(s.substr(start, i - start));
                start = i + 1;
            }
        }
        return res;
    }

    void add_train(const std::string& i, int n, int m, const std::string& s, const std::string& p, const std::string& x, const std::string& t, const std::string& o, const std::string& d, char y) {
        String24 i_key(i);
        TrainData dummy;
        if (trains.find(i_key, dummy)) {
            std::cout << "-1\n";
            return;
        }

        TrainData td;
        td.stationNum = n;
        td.seatNum = m;
        sjtu::vector<std::string> st_vec = split(s, '|');
        for (int j = 0; j < n; ++j) {
            strncpy(td.stations[j], st_vec[j].c_str(), sizeof(td.stations[j]) - 1);
            td.stations[j][sizeof(td.stations[j]) - 1] = '\0';
        }
        sjtu::vector<std::string> p_vec = split(p, '|');
        for (int j = 0; j < n - 1; ++j) {
            td.prices[j] = std::stoi(p_vec[j]);
        }
        td.startTime = time_to_int(x);
        sjtu::vector<std::string> t_vec = split(t, '|');
        for (int j = 0; j < n - 1; ++j) {
            td.travelTimes[j] = std::stoi(t_vec[j]);
        }
        if (n > 2) {
            sjtu::vector<std::string> o_vec = split(o, '|');
            for (int j = 0; j < n - 2; ++j) {
                td.stopoverTimes[j] = std::stoi(o_vec[j]);
            }
        }
        sjtu::vector<std::string> d_vec = split(d, '|');
        td.saleDateL = date_to_int(d_vec[0]);
        td.saleDateR = date_to_int(d_vec[1]);
        td.type = y;
        td.released = false;

        trains.insert(i_key, td);
        std::cout << "0\n";
    }

    void release_train(const std::string& i) {
        String24 i_key(i);
        TrainData td;
        if (!trains.find(i_key, td) || td.released) {
            std::cout << "-1\n";
            return;
        }
        td.released = true;
        trains.insert(i_key, td); // Update

        for (int j = 0; j < td.stationNum; ++j) {
            StationTrainKey st_key(td.stations[j], i);
            station_trains.insert(st_key, j);
        }

        // Initialize seats for this train for all days
        SeatData init_seat;
        for (int k = 0; k < td.stationNum - 1; ++k) {
            init_seat.seats[k] = td.seatNum;
        }
        for (int day = td.saleDateL; day <= td.saleDateR; ++day) {
            SeatKey sk(i, day);
            seats.insert(sk, init_seat);
        }
        std::cout << "0\n";
    }

    void query_train(const std::string& i, const std::string& d) {
        String24 i_key(i);
        TrainData td;
        if (!trains.find(i_key, td)) {
            std::cout << "-1\n";
            return;
        }
        int query_date = date_to_int(d);
        if (query_date < td.saleDateL || query_date > td.saleDateR) {
            std::cout << "-1\n";
            return;
        }

        SeatData sd;
        bool has_seats = false;
        if (td.released) {
            has_seats = seats.find(SeatKey(i, query_date), sd);
        }

        std::cout << i << " " << td.type << "\n";
        int cur_time = td.startTime;
        int sum_price = 0;
        for (int j = 0; j < td.stationNum; ++j) {
            std::cout << td.stations[j] << " ";
            if (j == 0) {
                std::cout << "xx-xx xx:xx -> " << format_datetime(query_date, cur_time) << " ";
            } else if (j == td.stationNum - 1) {
                std::cout << format_datetime(query_date, cur_time) << " -> xx-xx xx:xx ";
            } else {
                std::cout << format_datetime(query_date, cur_time) << " -> ";
                cur_time += td.stopoverTimes[j - 1];
                std::cout << format_datetime(query_date, cur_time) << " ";
            }

            std::cout << sum_price << " ";
            if (j < td.stationNum - 1) {
                sum_price += td.prices[j];
            }

            if (j == td.stationNum - 1) {
                std::cout << "x\n";
            } else {
                if (td.released && has_seats) {
                    std::cout << sd.seats[j] << "\n";
                } else {
                    std::cout << td.seatNum << "\n";
                }
                cur_time += td.travelTimes[j];
            }
        }
    }

    void delete_train(const std::string& i) {
        String24 i_key(i);
        TrainData td;
        if (!trains.find(i_key, td) || td.released) {
            std::cout << "-1\n";
            return;
        }
        trains.erase(i_key);
        std::cout << "0\n";
    }
    struct TicketRes {
        String24 trainID;
        int cost;
        int time;
        int start_time;
        int end_time;
        int start_date;
        int end_date;
        int seat;
    };

    void query_ticket(const std::string& s, const std::string& t, const std::string& d, const std::string& p) {
        int target_date = date_to_int(d);
        auto match_s = [&](const StationTrainKey& k) { return strcmp(k.station, s.c_str()) == 0; };
        auto match_t = [&](const StationTrainKey& k) { return strcmp(k.station, t.c_str()) == 0; };
        
        sjtu::vector<sjtu::pair<StationTrainKey, int>> starts = station_trains.find_prefix(StationTrainKey(s, ""), match_s);
        sjtu::vector<sjtu::pair<StationTrainKey, int>> ends = station_trains.find_prefix(StationTrainKey(t, ""), match_t);
        
        sjtu::vector<TicketRes> results;
        
        int i = 0, j = 0;
        while (i < starts.size() && j < ends.size()) {
            int cmp = strcmp(starts[i].first.trainID, ends[j].first.trainID);
            if (cmp == 0) {
                int s_idx = starts[i].second;
                int t_idx = ends[j].second;
                if (s_idx < t_idx) {
                    String24 tID = starts[i].first.trainID;
                    TrainData td;
                    if (trains.find(tID, td) && td.released) {
                        int cur_time = td.startTime;
                        for (int k = 0; k < s_idx; ++k) {
                            cur_time += td.travelTimes[k];
                            if (k < s_idx - 1) cur_time += td.stopoverTimes[k];
                            else if (s_idx > 0 && k == s_idx - 1) cur_time += td.stopoverTimes[k]; // Wait, stopover is AT s_idx. We depart s_idx, so we ADD stopover at s_idx.
                        }
                        // Actually, departure time from s_idx:
                        int leave_s_time = td.startTime;
                        for (int k = 0; k < s_idx; ++k) {
                            leave_s_time += td.travelTimes[k];
                            leave_s_time += td.stopoverTimes[k];
                        }
                        int extra_days = leave_s_time / 1440;
                        int origin_date = target_date - extra_days;
                        
                        if (origin_date >= td.saleDateL && origin_date <= td.saleDateR) {
                            int cost = 0;
                            for (int k = s_idx; k < t_idx; ++k) cost += td.prices[k];
                            
                            int arrive_t_time = leave_s_time;
                            for (int k = s_idx; k < t_idx; ++k) {
                                arrive_t_time += td.travelTimes[k];
                                if (k < t_idx - 1) arrive_t_time += td.stopoverTimes[k];
                            }
                            
                            SeatData sd;
                            seats.find(SeatKey(tID.str, origin_date), sd);
                            int min_seat = td.seatNum;
                            for (int k = s_idx; k < t_idx; ++k) {
                                if (sd.seats[k] < min_seat) min_seat = sd.seats[k];
                            }
                            
                            TicketRes res;
                            res.trainID = tID;
                            res.cost = cost;
                            res.time = arrive_t_time - leave_s_time;
                            res.start_time = leave_s_time % 1440;
                            res.end_time = arrive_t_time % 1440;
                            res.start_date = target_date;
                            res.end_date = origin_date + (arrive_t_time / 1440);
                            res.seat = min_seat;
                            results.push_back(res);
                        }
                    }
                }
                ++i; ++j;
            } else if (cmp < 0) {
                ++i;
            } else {
                ++j;
            }
        }
        
        bool by_time = (p == "time");
        sjtu::sort(results.begin(), results.end(), [by_time](const TicketRes& a, const TicketRes& b) {
            if (by_time) {
                if (a.time != b.time) return a.time < b.time;
            } else {
                if (a.cost != b.cost) return a.cost < b.cost;
            }
            return strcmp(a.trainID.str, b.trainID.str) < 0;
        });
        
        std::cout << results.size() << "\n";
        for (int k = 0; k < results.size(); ++k) {
            std::cout << results[k].trainID.str << " " << s << " " << format_datetime(results[k].start_date, results[k].start_time)
                      << " -> " << t << " " << format_datetime(results[k].end_date, results[k].end_time)
                      << " " << results[k].cost << " " << results[k].seat << "\n";
        }
    }

    void query_transfer(const std::string& s, const std::string& t, const std::string& d, const std::string& p) {
        int target_date = date_to_int(d);
        auto match_s = [&](const StationTrainKey& k) { return strcmp(k.station, s.c_str()) == 0; };
        auto match_t = [&](const StationTrainKey& k) { return strcmp(k.station, t.c_str()) == 0; };
        
        sjtu::vector<sjtu::pair<StationTrainKey, int>> starts = station_trains.find_prefix(StationTrainKey(s, ""), match_s);
        sjtu::vector<sjtu::pair<StationTrainKey, int>> ends = station_trains.find_prefix(StationTrainKey(t, ""), match_t);
        
        bool by_time = (p == "time");
        bool found = false;
        
        TicketRes best_t1, best_t2;
        int best_cost = 2e9, best_time = 2e9;
        std::string best_mid;
        
        for (int i = 0; i < starts.size(); ++i) {
            String24 tID1 = starts[i].first.trainID;
            int s_idx = starts[i].second;
            TrainData td1;
            if (!trains.find(tID1, td1) || !td1.released) continue;
            
            int leave_s_time = td1.startTime;
            for (int k = 0; k < s_idx; ++k) {
                leave_s_time += td1.travelTimes[k] + td1.stopoverTimes[k];
            }
            int extra_days1 = leave_s_time / 1440;
            int origin_date1 = target_date - extra_days1;
            if (origin_date1 < td1.saleDateL || origin_date1 > td1.saleDateR) continue;
            
            SeatData sd1;
            seats.find(SeatKey(tID1.str, origin_date1), sd1);
            
            int cur_time1 = leave_s_time;
            int cost1 = 0;
            int min_seat1 = td1.seatNum;
            
            for (int j = s_idx + 1; j < td1.stationNum; ++j) {
                cost1 += td1.prices[j - 1];
                if (sd1.seats[j - 1] < min_seat1) min_seat1 = sd1.seats[j - 1];
                cur_time1 += td1.travelTimes[j - 1];
                int arr_mid_time = cur_time1;
                
                std::string mid_station = td1.stations[j];
                auto match_mid = [&](const StationTrainKey& k) { return strcmp(k.station, mid_station.c_str()) == 0; };
                sjtu::vector<sjtu::pair<StationTrainKey, int>> mid_starts = station_trains.find_prefix(StationTrainKey(mid_station, ""), match_mid);
                
                for (int x = 0; x < mid_starts.size(); ++x) {
                    String24 tID2 = mid_starts[x].first.trainID;
                    if (tID1 == tID2) continue;
                    
                    int mid_idx2 = mid_starts[x].second;
                    // Find if tID2 goes to t
                    int t_idx2 = -1;
                    // We can binary search in `ends` for tID2, because `ends` is sorted by trainID since B+Tree keys are sorted by (station, trainID).
                    int l = 0, r = ends.size() - 1;
                    while (l <= r) {
                        int mid = l + (r - l) / 2;
                        int cmp = strcmp(ends[mid].first.trainID, tID2.str);
                        if (cmp == 0) { t_idx2 = ends[mid].second; break; }
                        else if (cmp < 0) l = mid + 1;
                        else r = mid - 1;
                    }
                    if (t_idx2 == -1 || mid_idx2 >= t_idx2) continue;
                    
                    TrainData td2;
                    if (!trains.find(tID2, td2) || !td2.released) continue;
                    
                    int leave_mid_base = td2.startTime;
                    for (int k = 0; k < mid_idx2; ++k) {
                        leave_mid_base += td2.travelTimes[k] + td2.stopoverTimes[k];
                    }
                    
                    // We need a departure date from mid_station that is >= arrive_date at mid_station
                    int arr_mid_date = origin_date1 + (arr_mid_time / 1440);
                    int arr_mid_min = arr_mid_time % 1440;
                    
                    int leave_mid_min = leave_mid_base % 1440;
                    int base_extra_days = leave_mid_base / 1440;
                    
                    int target_leave_date = arr_mid_date;
                    if (leave_mid_min < arr_mid_min) target_leave_date++;
                    
                    int origin_date2 = target_leave_date - base_extra_days;
                    if (origin_date2 < td2.saleDateL) {
                        origin_date2 = td2.saleDateL;
                    }
                    if (origin_date2 > td2.saleDateR) continue;
                    
                    int actual_leave_mid_time = (origin_date2 + base_extra_days) * 1440 + leave_mid_min;
                    
                    SeatData sd2;
                    seats.find(SeatKey(tID2.str, origin_date2), sd2);
                    
                    int cost2 = 0;
                    int min_seat2 = td2.seatNum;
                    for (int k = mid_idx2; k < t_idx2; ++k) {
                        cost2 += td2.prices[k];
                        if (sd2.seats[k] < min_seat2) min_seat2 = sd2.seats[k];
                    }
                    
                    int arr_t_time = leave_mid_base;
                    for (int k = mid_idx2; k < t_idx2; ++k) {
                        arr_t_time += td2.travelTimes[k];
                        if (k < t_idx2 - 1) arr_t_time += td2.stopoverTimes[k];
                    }
                    int actual_arr_t_time = (origin_date2) * 1440 + arr_t_time; // total mins from day 0
                    
                    int total_time = actual_arr_t_time - (origin_date1 * 1440 + leave_s_time);
                    int total_cost = cost1 + cost2;
                    
                    bool replace = false;
                    if (!found) replace = true;
                    else if (by_time) {
                        if (total_time < best_time) replace = true;
                        else if (total_time == best_time && total_cost < best_cost) replace = true;
                        else if (total_time == best_time && total_cost == best_cost) {
                            if (arr_mid_time - leave_s_time < best_t1.time) replace = true;
                        }
                    } else {
                        if (total_cost < best_cost) replace = true;
                        else if (total_cost == best_cost && total_time < best_time) replace = true;
                        else if (total_time == best_time && total_cost == best_cost) {
                            if (arr_mid_time - leave_s_time < best_t1.time) replace = true;
                        }
                    }
                    
                    if (replace) {
                        found = true;
                        best_time = total_time;
                        best_cost = total_cost;
                        best_mid = mid_station;
                        
                        best_t1.trainID = tID1;
                        best_t1.cost = cost1;
                        best_t1.time = arr_mid_time - leave_s_time;
                        best_t1.start_time = leave_s_time % 1440;
                        best_t1.end_time = arr_mid_time % 1440;
                        best_t1.start_date = target_date;
                        best_t1.end_date = arr_mid_date;
                        best_t1.seat = min_seat1;
                        
                        best_t2.trainID = tID2;
                        best_t2.cost = cost2;
                        best_t2.time = actual_arr_t_time - actual_leave_mid_time;
                        best_t2.start_time = actual_leave_mid_time % 1440;
                        best_t2.end_time = actual_arr_t_time % 1440;
                        best_t2.start_date = origin_date2 + base_extra_days;
                        best_t2.end_date = actual_arr_t_time / 1440;
                        best_t2.seat = min_seat2;
                    }
                }
                cur_time1 += td1.stopoverTimes[j];
            }
        }
        
        if (!found) {
            std::cout << "0\n";
        } else {
            std::cout << best_t1.trainID.str << " " << s << " " << format_datetime(best_t1.start_date, best_t1.start_time)
                      << " -> " << best_mid << " " << format_datetime(best_t1.end_date, best_t1.end_time)
                      << " " << best_t1.cost << " " << best_t1.seat << "\n";
            std::cout << best_t2.trainID.str << " " << best_mid << " " << format_datetime(best_t2.start_date, best_t2.start_time)
                      << " -> " << t << " " << format_datetime(best_t2.end_date, best_t2.end_time)
                      << " " << best_t2.cost << " " << best_t2.seat << "\n";
        }
    }
    void buy_ticket(const std::string& u, const std::string& i, const std::string& d, int n, const std::string& f, const std::string& t, bool q) {
        String24 u_key(u);
        if (!is_logged_in(u_key)) {
            std::cout << "-1\n";
            return;
        }
        
        String24 i_key(i);
        TrainData td;
        if (!trains.find(i_key, td) || !td.released || n > td.seatNum || n == 0) {
            std::cout << "-1\n";
            return;
        }
        
        int target_date = date_to_int(d);
        int f_idx = -1, t_idx = -1;
        for (int j = 0; j < td.stationNum; ++j) {
            if (strcmp(td.stations[j], f.c_str()) == 0) f_idx = j;
            if (strcmp(td.stations[j], t.c_str()) == 0) t_idx = j;
        }
        
        if (f_idx == -1 || t_idx == -1 || f_idx >= t_idx) {
            std::cout << "-1\n";
            return;
        }
        
        int leave_s_time = td.startTime;
        for (int k = 0; k < f_idx; ++k) {
            leave_s_time += td.travelTimes[k] + td.stopoverTimes[k];
        }
        
        int extra_days = leave_s_time / 1440;
        int origin_date = target_date - extra_days;
        if (origin_date < td.saleDateL || origin_date > td.saleDateR) {
            std::cout << "-1\n";
            return;
        }
        
        int cost = 0;
        for (int k = f_idx; k < t_idx; ++k) cost += td.prices[k];
        
        int arrive_t_time = leave_s_time;
        for (int k = f_idx; k < t_idx; ++k) {
            arrive_t_time += td.travelTimes[k];
            if (k < t_idx - 1) arrive_t_time += td.stopoverTimes[k];
        }
        
        SeatKey sk(i, origin_date);
        SeatData sd;
        seats.find(sk, sd);
        
        bool enough_seats = true;
        for (int k = f_idx; k < t_idx; ++k) {
            if (sd.seats[k] < n) { enough_seats = false; break; }
        }
        
        if (enough_seats) {
            for (int k = f_idx; k < t_idx; ++k) sd.seats[k] -= n;
            seats.insert(sk, sd);
            
            OrderData od;
            od.status = SUCCESS;
            strncpy(od.trainID, i.c_str(), sizeof(od.trainID) - 1);
            od.trainID[sizeof(od.trainID) - 1] = '\0';
            strncpy(od.from, f.c_str(), sizeof(od.from) - 1);
            od.from[sizeof(od.from) - 1] = '\0';
            strncpy(od.to, t.c_str(), sizeof(od.to) - 1);
            od.to[sizeof(od.to) - 1] = '\0';
            od.date = target_date;
            od.price = cost;
            od.num = n;
            od.leavingTime = leave_s_time;
            od.arrivingTime = arrive_t_time;
            od.timestamp = ++queue_timestamp_counter;
            od.originTrainDate = origin_date;
            od.fromIdx = f_idx;
            od.toIdx = t_idx;
            
            OrderKey ok(u, ++order_id_counter);
            orders.insert(ok, od);
            
            std::cout << (long long)cost * n << "\n";
        } else if (q) {
            OrderData od;
            od.status = PENDING;
            strncpy(od.trainID, i.c_str(), sizeof(od.trainID) - 1);
            od.trainID[sizeof(od.trainID) - 1] = '\0';
            strncpy(od.from, f.c_str(), sizeof(od.from) - 1);
            od.from[sizeof(od.from) - 1] = '\0';
            strncpy(od.to, t.c_str(), sizeof(od.to) - 1);
            od.to[sizeof(od.to) - 1] = '\0';
            od.date = target_date;
            od.price = cost;
            od.num = n;
            od.leavingTime = leave_s_time;
            od.arrivingTime = arrive_t_time;
            od.timestamp = ++queue_timestamp_counter;
            od.originTrainDate = origin_date;
            od.fromIdx = f_idx;
            od.toIdx = t_idx;
            
            int ordID = ++order_id_counter;
            OrderKey ok(u, ordID);
            orders.insert(ok, od);
            
            QueueData qd;
            strncpy(qd.username, u.c_str(), sizeof(qd.username) - 1);
            qd.username[sizeof(qd.username) - 1] = '\0';
            qd.orderID = ordID;
            qd.fromIdx = f_idx;
            qd.toIdx = t_idx;
            qd.num = n;
            
            pending.insert(QueueKey(i, origin_date, od.timestamp), qd);
            
            std::cout << "queue\n";
        } else {
            std::cout << "-1\n";
        }
    }

    void query_order(const std::string& u) {
        String24 u_key(u);
        if (!is_logged_in(u_key)) {
            std::cout << "-1\n";
            return;
        }
        
        auto match_u = [&](const OrderKey& k) { return strcmp(k.username, u.c_str()) == 0; };
        sjtu::vector<sjtu::pair<OrderKey, OrderData>> res = orders.find_prefix(OrderKey(u, 2147483647), match_u);
        
        std::cout << res.size() << "\n";
        for (int i = 0; i < res.size(); ++i) {
            const OrderData& od = res[i].second;
            std::cout << "[";
            if (od.status == SUCCESS) std::cout << "success";
            else if (od.status == PENDING) std::cout << "pending";
            else std::cout << "refunded";
            std::cout << "] " << od.trainID << " " << od.from << " " << format_datetime(od.date, od.leavingTime % 1440)
                      << " -> " << od.to << " " << format_datetime(od.date + (od.arrivingTime - od.leavingTime) / 1440, od.arrivingTime % 1440)
                      << " " << od.price << " " << od.num << "\n";
        }
    }

    void refund_ticket(const std::string& u, int n = 1) {
        String24 u_key(u);
        if (!is_logged_in(u_key)) {
            std::cout << "-1\n";
            return;
        }
        
        auto match_u = [&](const OrderKey& k) { return strcmp(k.username, u.c_str()) == 0; };
        sjtu::vector<sjtu::pair<OrderKey, OrderData>> res = orders.find_prefix(OrderKey(u, 2147483647), match_u);
        
        if (n <= 0 || n > res.size()) {
            std::cout << "-1\n";
            return;
        }
        
        OrderKey ok = res[n - 1].first;
        OrderData od = res[n - 1].second;
        
        if (od.status == REFUNDED) {
            std::cout << "-1\n";
            return;
        }
        
        if (od.status == PENDING) {
            pending.erase(QueueKey(od.trainID, od.originTrainDate, od.timestamp));
        } else {
            SeatKey sk(od.trainID, od.originTrainDate);
            SeatData sd;
            seats.find(sk, sd);
            for (int k = od.fromIdx; k < od.toIdx; ++k) sd.seats[k] += od.num;
            
            // Check pending queue for this train/date
            auto match_q = [&](const QueueKey& k) {
                return strcmp(k.trainID, od.trainID) == 0 && k.date == od.originTrainDate;
            };
            sjtu::vector<sjtu::pair<QueueKey, QueueData>> q_res = pending.find_prefix(QueueKey(od.trainID, od.originTrainDate, 0), match_q);
            
            for (int i = 0; i < q_res.size(); ++i) {
                const QueueData& qd = q_res[i].second;
                bool enough = true;
                for (int k = qd.fromIdx; k < qd.toIdx; ++k) {
                    if (sd.seats[k] < qd.num) { enough = false; break; }
                }
                if (enough) {
                    for (int k = qd.fromIdx; k < qd.toIdx; ++k) sd.seats[k] -= qd.num;
                    
                    OrderKey ok_q(qd.username, qd.orderID);
                    OrderData od_q;
                    orders.find(ok_q, od_q);
                    od_q.status = SUCCESS;
                    orders.insert(ok_q, od_q);
                    
                    pending.erase(q_res[i].first);
                }
            }
            seats.insert(sk, sd);
        }
        
        od.status = REFUNDED;
        orders.insert(ok, od);
        std::cout << "0\n";
    }
};

}

#endif
