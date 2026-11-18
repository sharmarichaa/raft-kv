#pragma once
#include <algorithm>
#include <vector>
#include "log.h"
#include "append_entries.h"

// Leader-side bookkeeping. Node ids are 1..num_nodes.
class LeaderState {
public:
    LeaderState(int num_nodes, int my_id, const Log& log)
        : n_(num_nodes), me_(my_id),
          next_(num_nodes + 1, log.last_index() + 1),
          match_(num_nodes + 1, 0) {
        match_[me_] = log.last_index();
    }

    int next_index(int peer) const { return next_[peer]; }
    int match_index(int peer) const { return match_[peer]; }

    // Build the AppendEntries request for one follower
    AppendEntriesArgs make_args(const Log& log, int term, int peer,
                                int commit_index) const {
        int next = next_[peer];
        AppendEntriesArgs a;
        a.term = term;
        a.leader_id = me_;
        a.prev_log_index = next - 1;
        a.prev_log_term = log.term_at(next - 1);
        for (int i = next; i <= log.last_index(); i++) a.entries.push_back(log.at(i));
        a.leader_commit = commit_index;
        return a;
    }

    // Call only when reply.term <= our term (a higher term means step down instead)
    void on_reply(int peer, const AppendEntriesArgs& sent, const AppendEntriesReply& r) {
        if (r.success) {
            int new_match = sent.prev_log_index + (int)sent.entries.size();
            match_[peer] = std::max(match_[peer], new_match);
            next_[peer] = match_[peer] + 1;
        } else {
            next_[peer] = std::max(1, std::min(next_[peer], sent.prev_log_index));
        }
    }

    // Returns the new commit index (unchanged if nothing new can commit)
    int update_commit(const Log& log, int current_term, int commit_index) {
        match_[me_] = log.last_index();
        for (int n = log.last_index(); n > commit_index; n--) {
            if (log.term_at(n) != current_term) break;  // never commit old-term entries by counting
            int count = 0;
            for (int id = 1; id <= n_; id++)
                if (match_[id] >= n) count++;
            if (count * 2 > n_) return n;
        }
        return commit_index;
    }

private:
    int n_, me_;
    std::vector<int> next_, match_;
};

