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
#include "log.h"
#include "append_entries.h"
#include "leader_state.h"

using Clock = std::chrono::steady_clock;
const int BASE_PORT = 9300;
const int NUM_NODES = 3;
const int HEARTBEAT_MS = 100;

enum class Role { Follower, Candidate, Leader };

int my_id;
std::mutex mu;
Role role = Role::Follower;
int current_term = 0;
int voted_for = -1;
int commit_index = 0;
int last_applied = 0;
Log raft_log;
LeaderState* leader = nullptr;
Clock::time_point election_deadline;
std::mt19937 rng;

// ---- helpers (call with mu held) ----
void reset_election_timer() {
    std::uniform_int_distribution<int> d(300, 600);
    election_deadline = Clock::now() + std::chrono::milliseconds(d(rng));
}

void step_down_if_newer(int term) {
    if (term > current_term) {
        current_term = term;
        voted_for = -1;
        if (role != Role::Follower)
            std::cout << "[node " << my_id << "] term " << term << ": stepping down\n";
        role = Role::Follower;
    }
}

void apply_committed() {
    while (last_applied < commit_index) {
        last_applied++;
        std::cout << "[node " << my_id << "] APPLIED index " << last_applied
                  << ": " << raft_log.at(last_applied).command << "\n";
    }
}

// ---- serialization ----
// request:  "AE term leader prevIdx prevTerm commit n  (term cmd) x n"
// commands may not contain newlines; each entry is sent on its own line
std::string encode_ae(const AppendEntriesArgs& a) {
    std::ostringstream o;
    o << "AE " << a.term << " " << a.leader_id << " " << a.prev_log_index << " "
      << a.prev_log_term << " " << a.leader_commit << " " << a.entries.size() << "\n";
    for (auto& e : a.entries) o << e.term << " " << e.command << "\n";
    return o.str();
}

AppendEntriesArgs decode_ae(std::istringstream& in) {
    AppendEntriesArgs a;
    size_t n;
    in >> a.term >> a.leader_id >> a.prev_log_index >> a.prev_log_term
       >> a.leader_commit >> n;
    in.ignore();  // newline
    for (size_t i = 0; i < n; i++) {
        std::string line;
        std::getline(in, line);
        std::istringstream ls(line);
        LogEntry e;
        ls >> e.term;
        ls.ignore();
        std::getline(ls, e.command);
        a.entries.push_back(e);
    }
    return a;
}

// ---- request handling ----
std::string handle_request(const std::string& msg) {
    std::istringstream in(msg);
    std::string type;
    in >> type;
    std::lock_guard<std::mutex> lock(mu);

    if (type == "RV") {
        int term, cand, last_idx, last_term;
        in >> term >> cand >> last_idx >> last_term;
        step_down_if_newer(term);
        bool ok = false;
        // vote rule: candidate's log must be at least as up to date as ours
        bool up_to_date = last_term > raft_log.last_term() ||
            (last_term == raft_log.last_term() && last_idx >= raft_log.last_index());
        if (term == current_term && up_to_date &&
            (voted_for == -1 || voted_for == cand)) {
            voted_for = cand;
            ok = true;
            reset_election_timer();
        }
        return std::to_string(current_term) + " " + (ok ? "1" : "0");
    }

    if (type == "AE") {
        AppendEntriesArgs a = decode_ae(in);
        step_down_if_newer(a.term);
        if (a.term == current_term) {
            role = Role::Follower;
            reset_election_timer();
        }
        auto r = handle_append_entries(raft_log, current_term, commit_index, a);
        apply_committed();
        return std::to_string(r.term) + " " + (r.success ? "1" : "0");
    }

    if (type == "CMD") {
        std::string cmd;
        in.ignore();
        std::getline(in, cmd);
        if (role != Role::Leader) return "NOT_LEADER";
        raft_log.append(current_term, cmd);
        std::cout << "[node " << my_id << "] leader appended index "
                  << raft_log.last_index() << ": " << cmd << "\n";
        return "ACCEPTED " + std::to_string(raft_log.last_index());
    }

    return "ERR";
}

void handle_client(int fd) {
    std::string msg;
    while (recv_message(fd, msg)) send_message(fd, handle_request(msg));
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

// ---- election ----
void start_election() {
    int term, last_idx, last_term;
    int votes = 1;
    {
        std::lock_guard<std::mutex> lock(mu);
        role = Role::Candidate;
        current_term++;
        voted_for = my_id;
        term = current_term;
        last_idx = raft_log.last_index();
        last_term = raft_log.last_term();
        reset_election_timer();
        std::cout << "[node " << my_id << "] term " << term << ": starting election\n";
    }
    std::vector<std::thread> threads;
    for (int peer = 1; peer <= NUM_NODES; peer++) {
        if (peer == my_id) continue;
        threads.emplace_back([&, peer] {
            std::string resp;
            std::string req = "RV " + std::to_string(term) + " " + std::to_string(my_id) +
                              " " + std::to_string(last_idx) + " " + std::to_string(last_term);
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
        delete leader;
        leader = new LeaderState(NUM_NODES, my_id, raft_log);
        std::cout << "[node " << my_id << "] term " << term << ": became LEADER with "
                  << votes << " votes\n";
    }
}

// ---- replication ----
void replicate_round() {
    int term;
    {
        std::lock_guard<std::mutex> lock(mu);
        if (role != Role::Leader) return;
        term = current_term;
    }
    std::vector<std::thread> threads;
    for (int peer = 1; peer <= NUM_NODES; peer++) {
        if (peer == my_id) continue;
        threads.emplace_back([&, peer] {
            AppendEntriesArgs args;
            {
                std::lock_guard<std::mutex> lock(mu);
                if (role != Role::Leader || current_term != term) return;
                args = leader->make_args(raft_log, term, peer, commit_index);
            }
            std::string resp;
            if (!rpc_call("127.0.0.1", BASE_PORT + peer, encode_ae(args), resp, 150)) return;
            std::istringstream in(resp);
            int rterm, ok;
            in >> rterm >> ok;
            std::lock_guard<std::mutex> lock(mu);
            if (rterm > current_term) { step_down_if_newer(rterm); return; }
            if (role != Role::Leader || current_term != term) return;
            leader->on_reply(peer, args, {rterm, ok == 1});
        });
    }
    for (auto& t : threads) t.join();

    std::lock_guard<std::mutex> lock(mu);
    if (role == Role::Leader && current_term == term) {
        int before = commit_index;
        commit_index = leader->update_commit(raft_log, current_term, commit_index);
        if (commit_index != before)
            std::cout << "[node " << my_id << "] commit index -> " << commit_index << "\n";
        apply_committed();
    }
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: raft-node <id 1-3>\n"; return 1; }
    my_id = std::atoi(argv[1]);
    signal(SIGPIPE, SIG_IGN);
    rng.seed(std::random_device{}() ^ (my_id * 7919));
    { std::lock_guard<std::mutex> lock(mu); reset_election_timer(); }
    std::thread(serve).detach();
    std::cout << "[node " << my_id << "] started as follower\n";

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Role r;
        bool timed_out;
        {
            std::lock_guard<std::mutex> lock(mu);
            r = role;
            timed_out = Clock::now() >= election_deadline;
        }
        if (r == Role::Leader) {
            replicate_round();
            std::this_thread::sleep_for(std::chrono::milliseconds(HEARTBEAT_MS));
        } else if (timed_out) {
            start_election();
        }
    }
}

