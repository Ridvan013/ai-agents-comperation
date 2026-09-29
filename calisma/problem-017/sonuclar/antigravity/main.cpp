#include "system.hpp"
#include <iostream>
#include <string>

sjtu::vector<std::string> split_args(const std::string& s) {
    sjtu::vector<std::string> res;
    size_t start = 0;
    bool in_space = true;
    for (size_t i = 0; i < s.length(); ++i) {
        if (s[i] == ' ' || s[i] == '\r' || s[i] == '\n' || s[i] == '\t') {
            if (!in_space) {
                res.push_back(s.substr(start, i - start));
                in_space = true;
            }
        } else {
            if (in_space) {
                start = i;
                in_space = false;
            }
        }
    }
    if (!in_space) {
        res.push_back(s.substr(start));
    }
    return res;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    sjtu::TicketSystem* sys = new sjtu::TicketSystem();
    std::string line;
    
    while (std::getline(std::cin, line)) {
        sjtu::vector<std::string> args = split_args(line);
        if (args.empty()) continue;
        
        std::string cmd = args[0];
        
        std::string params[26];
        for (size_t i = 1; i < args.size(); i += 2) {
            if (args[i][0] == '-' && args[i].length() == 2) {
                if (i + 1 < args.size()) {
                    params[args[i][1] - 'a'] = args[i + 1];
                }
            }
        }
        
        // Print timestamp token if it's the OJ's output requirement:
        // Wait, the OJ specifies command prompts? The problem says:
        // "After receiving a complete command, it should immediately output the result and return to the state of waiting for input."
        // Wait, the examples show:
        // `> add_user ...`
        // Actually we don't output `>` or anything. Wait, looking at example:
        // Example:
        // `> add_train ...`
        // `0`
        // The `> ` might be part of the test data (in some OJs). But standard practice is to just parse the command.
        // I will assume input has timestamps if it starts with `[`.
        if (cmd[0] == '[') {
            std::cout << cmd << " ";
            if (args.size() > 1) {
                cmd = args[1];
                for (size_t i = 2; i < args.size(); i += 2) {
                    if (args[i][0] == '-' && args[i].length() == 2) {
                        if (i + 1 < args.size()) {
                            params[args[i][1] - 'a'] = args[i + 1];
                        }
                    }
                }
            } else {
                continue;
            }
        }

        if (cmd == "add_user") {
            sys->add_user(params['c' - 'a'], params['u' - 'a'], params['p' - 'a'], params['n' - 'a'], params['m' - 'a'], std::stoi(params['g' - 'a']));
        } else if (cmd == "login") {
            sys->login(params['u' - 'a'], params['p' - 'a']);
        } else if (cmd == "logout") {
            sys->logout(params['u' - 'a']);
        } else if (cmd == "query_profile") {
            sys->query_profile(params['c' - 'a'], params['u' - 'a']);
        } else if (cmd == "modify_profile") {
            int g = params['g' - 'a'].empty() ? -1 : std::stoi(params['g' - 'a']);
            sys->modify_profile(params['c' - 'a'], params['u' - 'a'], params['p' - 'a'], params['n' - 'a'], params['m' - 'a'], g);
        } else if (cmd == "add_train") {
            sys->add_train(params['i' - 'a'], std::stoi(params['n' - 'a']), std::stoi(params['m' - 'a']), params['s' - 'a'], params['p' - 'a'], params['x' - 'a'], params['t' - 'a'], params['o' - 'a'], params['d' - 'a'], params['y' - 'a'][0]);
        } else if (cmd == "release_train") {
            sys->release_train(params['i' - 'a']);
        } else if (cmd == "query_train") {
            sys->query_train(params['i' - 'a'], params['d' - 'a']);
        } else if (cmd == "delete_train") {
            sys->delete_train(params['i' - 'a']);
        } else if (cmd == "query_ticket") {
            sys->query_ticket(params['s' - 'a'], params['t' - 'a'], params['d' - 'a'], params['p' - 'a'].empty() ? "time" : params['p' - 'a']);
        } else if (cmd == "query_transfer") {
            sys->query_transfer(params['s' - 'a'], params['t' - 'a'], params['d' - 'a'], params['p' - 'a'].empty() ? "time" : params['p' - 'a']);
        } else if (cmd == "buy_ticket") {
            bool q = params['q' - 'a'] == "true";
            sys->buy_ticket(params['u' - 'a'], params['i' - 'a'], params['d' - 'a'], std::stoi(params['n' - 'a']), params['f' - 'a'], params['t' - 'a'], q);
        } else if (cmd == "query_order") {
            sys->query_order(params['u' - 'a']);
        } else if (cmd == "refund_ticket") {
            int n = params['n' - 'a'].empty() ? 1 : std::stoi(params['n' - 'a']);
            sys->refund_ticket(params['u' - 'a'], n);
        } else if (cmd == "clean") {
            sys->clear();
        } else if (cmd == "exit") {
            sys->exit();
            break;
        }
    }
    return 0;
}
