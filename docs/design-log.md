# Design Log

## Message framing
- TCP is a byte stream, so read() can return half a message or two glued together.
- recv_all loops until exactly N bytes arrive.
- htonl fixes byte order so different machines agree on the length.
- 1 MB max size check stops a bad length from allocating huge memory.

## RPC timeouts
- Without a timeout, one dead node would freeze the leader.
- SO_RCVTIMEO and SO_SNDTIMEO make read and write give up after N ms.
- A timeout and a refused connection both look like “call failed” to the caller.

## Leader election
- Randomized timeouts stop all nodes from becoming candidates at once and splitting the vote.
- One vote per term (voted_for) means at most one leader per term.
- A higher term always makes a node step down.
- A lone node keeps incrementing its term because it can never reach a majority.

## Log structure
- Index starts at 1 like the paper; the dummy entry at 0 makes the prevLogIndex = 0 check simple.
- Each entry stores the term it was created in, which is how nodes detect conflicts.
  
## AppendEntries
- The prevLogIndex/prevLogTerm check makes sure the follower’s log matches the leader’s before anything is added.
- On a conflict, the follower deletes that entry and everything after it, because the leader’s log wins.
- Entries we already have are skipped, so a retried RPC doesn’t duplicate anything.
- The commit index is min(leaderCommit, last new entry), so a follower never commits entries it doesn’t have.

## Leader replication
- nextIndex is a guess; matchIndex is confirmed. A failed AppendEntries moves nextIndex back until the logs match.
- Commit needs a majority, and the entry must be from the current term.
- Test 6 shows why: old-term entries only commit indirectly, when a current-term entry after them commits.


  
