#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <vector>
#include <sstream>
#include <csignal>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../net/rpc.h"

using Clock = std::chrono::steady_clock;
const int BASE_PORT = 9200;
const int NUM_NODES = 3;
const int HEARTBEAT_MS = 100;

enum class Role { Follower, Candidate, Leader };

int my_id;
std::mutex mu;
Role role = Role::Follower;
int current_term = 0;
int voted_for = -1;
Clock::time_point election_deadline;
std::mt19937 rng;

// call with mu held
void reset_election_timer() {
    std::uniform_int_distribution<int> d(300, 600);
    election_deadline = Clock::now() + std::chrono::milliseconds(d(rng));
}

// call with mu held
void step_down_if_newer(int term) {
    if (term > current_term) {
        current_term = term;
        voted_for = -1;
        if (role != Role::Follower)
            std::cout << "[node " << my_id << "] term " << term << ": stepping down to follower\n";
        role = Role::Follower;
    }
}

// Handles both RequestVote ("RV term from") and heartbeat ("AE term from")
std::string handle_request(const std::string& msg) {
    std::istringstream in(msg);
    std::string type;
    int term, from;
    in >> type >> term >> from;

    std::lock_guard<std::mutex> lock(mu);
    step_down_if_newer(term);

    bool ok = false;
    if (type == "RV") {
        if (term == current_term && (voted_for == -1 || voted_for == from)) {
            voted_for = from;
            ok = true;
            reset_election_timer();
        }
    } else if (type == "AE") {
        if (term == current_term) {
            role = Role::Follower;
            ok = true;
            reset_election_timer();
        }
    }
    return std::to_string(current_term) + " " + (ok ? "1" : "0");
}

void handle_client(int fd) {
    std::string msg;
    while (recv_message(fd, msg)) {
        send_message(fd, handle_request(msg));
    }
    close(fd);
}

void serve() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(BASE_PORT + my_id);
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 16);
    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        std::thread(handle_client, client_fd).detach();
    }
}

void start_election() {
    int term;
    int votes = 1;  // vote for myself
    {
        std::lock_guard<std::mutex> lock(mu);
        role = Role::Candidate;
        current_term++;
        voted_for = my_id;
        term = current_term;
        reset_election_timer();
        std::cout << "[node " << my_id << "] term " << term << ": starting election\n";
    }

    std::vector<std::thread> threads;
    for (int peer = 1; peer <= NUM_NODES; peer++) {
        if (peer == my_id) continue;
        threads.emplace_back([&, peer] {
            std::string resp;
            std::string req = "RV " + std::to_string(term) + " " + std::to_string(my_id);
            if (!rpc_call("127.0.0.1", BASE_PORT + peer, req, resp, 200)) return;
            std::istringstream in(resp);
            int rterm, granted;
            in >> rterm >> granted;
            std::lock_guard<std::mutex> lock(mu);
            step_down_if_newer(rterm);
            if (granted && role == Role::Candidate && current_term == term) votes++;
        });
    }
    for (auto& t : threads) t.join();

    std::lock_guard<std::mutex> lock(mu);
    if (role == Role::Candidate && current_term == term && votes * 2 > NUM_NODES) {
        role = Role::Leader;
        std::cout << "[node " << my_id << "] term " << term << ": became LEADER with "
                  << votes << " votes\n";
    }
}

void send_heartbeats(int term) {
    std::vector<std::thread> threads;
    for (int peer = 1; peer <= NUM_NODES; peer++) {
        if (peer == my_id) continue;
        threads.emplace_back([&, peer] {
            std::string resp;
            std::string req = "AE " + std::to_string(term) + " " + std::to_string(my_id);
            if (!rpc_call("127.0.0.1", BASE_PORT + peer, req, resp, 100)) return;
            std::istringstream in(resp);
            int rterm, ok;
            in >> rterm >> ok;
            std::lock_guard<std::mutex> lock(mu);
            step_down_if_newer(rterm);
        });
    }
    for (auto& t : threads) t.join();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: election-node <id 1-3>\n";
        return 1;
    }
    my_id = std::atoi(argv[1]);
    signal(SIGPIPE, SIG_IGN);
    rng.seed(std::random_device{}() ^ (my_id * 7919));

    {
        std::lock_guard<std::mutex> lock(mu);
        reset_election_timer();
    }
    std::thread(serve).detach();
    std::cout << "[node " << my_id << "] started as follower\n";

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Role r;
        int term;
        bool timed_out;
        {
            std::lock_guard<std::mutex> lock(mu);
            r = role;
            term = current_term;
            timed_out = Clock::now() >= election_deadline;
        }
        if (r == Role::Leader) {
            send_heartbeats(term);
            std::this_thread::sleep_for(std::chrono::milliseconds(HEARTBEAT_MS));
        } else if (timed_out) {
            start_election();
        }
    }
}
