#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include "log.h"

struct AppendEntriesArgs {
    int term;
    int leader_id;
    int prev_log_index;
    int prev_log_term;
    std::vector<LogEntry> entries;
    int leader_commit;
};

struct AppendEntriesReply {
    int term;
    bool success;
};

// Follower-side AppendEntries, written from Figure 2
inline AppendEntriesReply handle_append_entries(Log& log, int& current_term,
                                                int& commit_index,
                                                const AppendEntriesArgs& a) {
    // 1. reject a stale leader
    if (a.term < current_term) return {current_term, false};
    if (a.term > current_term) current_term = a.term;

    // 2. consistency check: do we have the entry just before the new ones?
    if (a.prev_log_index > log.last_index() ||
        log.term_at(a.prev_log_index) != a.prev_log_term)
        return {current_term, false};

    // 3. append new entries, deleting any that conflict
    int idx = a.prev_log_index;
    for (const auto& e : a.entries) {
        idx++;
        if (idx <= log.last_index()) {
            if (log.term_at(idx) == e.term) continue;  // already have it
            log.truncate_from(idx);                    // conflict: drop it and everything after
        }
        log.append(e.term, e.command);
    }

    // 4. advance commit index
    if (a.leader_commit > commit_index) {
        int last_new = a.prev_log_index + (int)a.entries.size();
        commit_index = std::min(a.leader_commit, last_new);
    }
    return {current_term, true};
}

