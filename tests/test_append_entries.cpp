#include <cassert>
#include <iostream>
#include "../src/raft/append_entries.h"

int main() {
    // 1. append to an empty log
    {
        Log log; int term = 1, commit = 0;
        auto r = handle_append_entries(log, term, commit,
            AppendEntriesArgs{1, 1, 0, 0, {{1, "a"}, {1, "b"}}, 0});
        assert(r.success && log.last_index() == 2);
    }
    // 2. prevLogIndex beyond our log is rejected
    {
        Log log; int term = 1, commit = 0;
        auto r = handle_append_entries(log, term, commit,
            AppendEntriesArgs{1, 1, 5, 1, {{1, "x"}}, 0});
        assert(!r.success && log.last_index() == 0);
    }
    // 3. prevLogTerm mismatch is rejected
    {
        Log log; log.append(1, "a");
        int term = 2, commit = 0;
        auto r = handle_append_entries(log, term, commit,
            AppendEntriesArgs{2, 1, 1, 2, {{2, "x"}}, 0});
        assert(!r.success);
    }
    // 4. conflicting entries are deleted and replaced
    {
        Log log; log.append(1, "a"); log.append(1, "b"); log.append(1, "c");
        int term = 2, commit = 0;
        auto r = handle_append_entries(log, term, commit,
            AppendEntriesArgs{2, 1, 1, 1, {{2, "x"}}, 0});
        assert(r.success && log.last_index() == 2);
        assert(log.term_at(2) == 2 && log.at(2).command == "x");
    }
    // 5. resending the same entries does not duplicate them
    {
        Log log; int term = 1, commit = 0;
        AppendEntriesArgs a{1, 1, 0, 0, {{1, "a"}, {1, "b"}}, 0};
        handle_append_entries(log, term, commit, a);
        handle_append_entries(log, term, commit, a);
        assert(log.last_index() == 2);
    }
    // 6. stale leader term is rejected
    {
        Log log; int term = 5, commit = 0;
        auto r = handle_append_entries(log, term, commit,
            AppendEntriesArgs{3, 1, 0, 0, {{3, "x"}}, 0});
        assert(!r.success && r.term == 5 && log.last_index() == 0);
    }
    // 7. commit index = min(leaderCommit, last new entry)
    {
        Log log; int term = 1, commit = 0;
        handle_append_entries(log, term, commit,
            AppendEntriesArgs{1, 1, 0, 0, {{1, "a"}, {1, "b"}}, 10});
        assert(commit == 2);
    }
    std::cout << "all tests passed\n";
}

