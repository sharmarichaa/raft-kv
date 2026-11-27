
#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include "../raft/log.h"

inline bool write_fully(int fd, const std::string& s) {
    const char* p = s.data();
    size_t len = s.size();
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n <= 0) return false;
        p += n;
        len -= n;
    }
    return true;
}

// Write state to dir/state.tmp, fsync, atomically rename to dir/state.dat
inline bool save_state(const std::string& dir, int term, int voted_for, const Log& log) {
    std::ostringstream o;
    o << term << " " << voted_for << " " << log.last_index() << "\n";
    for (int i = 1; i <= log.last_index(); i++)
        o << log.at(i).term << " " << log.at(i).command << "\n";
    std::string data = o.str();

    std::string tmp = dir + "/state.tmp";
    std::string fin = dir + "/state.dat";
    int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return false;
    bool ok = write_fully(fd, data) && fsync(fd) == 0;
    close(fd);
    if (!ok) return false;
    if (rename(tmp.c_str(), fin.c_str()) != 0) return false;

    int dfd = open(dir.c_str(), O_RDONLY);   // make the rename itself durable
    if (dfd >= 0) { fsync(dfd); close(dfd); }
    return true;
}

// Returns false if there is no saved state (fresh node)
inline bool load_state(const std::string& dir, int& term, int& voted_for, Log& log) {
    std::ifstream in(dir + "/state.dat");
    if (!in) return false;
    int n;
    if (!(in >> term >> voted_for >> n)) return false;
    in.ignore();
    for (int i = 0; i < n; i++) {
        std::string line;
        std::getline(in, line);
        std::istringstream ls(line);
        int t;
        ls >> t;
        ls.ignore();
        std::string cmd;
        std::getline(ls, cmd);
        log.append(t, cmd);
    }
    return true;
}

