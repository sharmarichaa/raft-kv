#pragma once
#include <string>
#include <vector>

struct LogEntry {
    int term;
    std::string command;   // e.g. "PUT name rahul"
};

// Log is 1-indexed like the Raft paper; index 0 is a dummy entry (term 0)
class Log {
public:
    Log() { entries_.push_back({0, ""}); }

    int last_index() const { return (int)entries_.size() - 1; }
    int last_term() const { return entries_.back().term; }

    int term_at(int index) const {
        if (index < 0 || index > last_index()) return -1;
        return entries_[index].term;
    }

    const LogEntry& at(int index) const { return entries_[index]; }

    int append(int term, const std::string& command) {
        entries_.push_back({term, command});
        return last_index();
    }

    // delete entry at index and everything after it
    void truncate_from(int index) {
        if (index >= 1 && index <= last_index())
            entries_.resize(index);
    }

private:
    std::vector<LogEntry> entries_;
};

