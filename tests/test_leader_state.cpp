#include <cassert>
#include <iostream>
#include "../src/raft/leader_state.h"

int main() {
    // 1. new leader starts with nextIndex = last + 1, matchIndex = 0
    {
        Log log; log.append(1, "a"); log.append(1, "a2");
        LeaderState ls(3, 1, log);
        assert(ls.next_index(2) == 3 && ls.match_index(2) == 0);
    }
    // 2. empty-log leader sends all entries once they exist
    {
        Log log; LeaderState ls(3, 1, log);
        log.append(1, "a"); log.append(1, "b");
        auto a = ls.make_args(log, 1, 2, 0);
        assert(a.prev_log_index == 0 && a.entries.size() == 2);
    }
    // 3. success moves matchIndex and nextIndex
    {
        Log log; LeaderState ls(3, 1, log);
        log.append(1, "a"); log.append(1, "b");
        auto a = ls.make_args(log, 1, 2, 0);
        ls.on_reply(2, a, {1, true});
        assert(ls.match_index(2) == 2 && ls.next_index(2) == 3);
    }
    // 4. failure walks nextIndex back one at a time
    {
        Log log; log.append(1, "a"); log.append(1, "b"); log.append(1, "c");
        LeaderState ls(3, 1, log);
        auto a = ls.make_args(log, 1, 2, 0);
        assert(a.prev_log_index == 3);
        ls.on_reply(2, a, {1, false});
        assert(ls.next_index(2) == 3);
        a = ls.make_args(log, 1, 2, 0);
        ls.on_reply(2, a, {1, false});
        assert(ls.next_index(2) == 2);
    }
    // 5. commit needs a majority (2 of 3)
    {
        Log log; LeaderState ls(3, 1, log);
        log.append(1, "a"); log.append(1, "b");
        assert(ls.update_commit(log, 1, 0) == 0);   // only the leader has it
        auto a = ls.make_args(log, 1, 2, 0);
        ls.on_reply(2, a, {1, true});
        assert(ls.update_commit(log, 1, 0) == 2);   // leader + one follower
    }
    // 6. old-term entries are not committed by counting replicas
    {
        Log log; LeaderState ls(3, 1, log);
        log.append(1, "a"); log.append(1, "b");
        auto a = ls.make_args(log, 2, 2, 0);
        ls.on_reply(2, a, {2, true});
        assert(ls.update_commit(log, 2, 0) == 0);   // entries are from term 1, leader is term 2
        log.append(2, "c");
        a = ls.make_args(log, 2, 2, 0);
        ls.on_reply(2, a, {2, true});
        assert(ls.update_commit(log, 2, 0) == 3);   // term-2 entry commits, and the old ones with it
    }
    std::cout << "all tests passed\n";
}

