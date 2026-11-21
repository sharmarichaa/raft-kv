# raft-kv

Fault-tolerant distributed key-value store in C++17 built on the Raft consensus algorithm.

**Status:** work in progress (oct 2025 - present)

## Done so far
- TCP server handling multiple clients concurrently (one thread per connection)
- Length-prefixed message framing (4-byte header, 1 MB size cap)
- RPC layer with configurable timeouts; dead and slow nodes fail cleanly
- 3-node cluster with failure detection
- Raft leader election: randomized timeouts, one vote per term, automatic failover when the leader crashes
- Raft log replication: AppendEntries with consistency check, majority commit (current-term rule), follower catch-up, and the "log must be up to date" vote rule
- Unit tests for the AppendEntries handler and leader commit logic

## Planned
- Write-ahead log with fsync and crash recovery
- GET/PUT/DEL state machine and retrying client
- Fault-injection tests (node crashes, partitions)
- Benchmarks (throughput, leader failover time)

## Architecture
    client --> any node --> leader
                              |
                  AppendEntries (replicate log)
                              |
                      follower 2, follower 3
    An entry is committed once a majority (2 of 3) has it, then applied in order.

## Project layout
    src/net/    TCP framing, RPC with timeouts
    src/raft/   Raft log, AppendEntries, leader state, cluster node
    tests/      unit tests

## Build
    mkdir build && cd build
    cmake .. && make

## Run a 3-node cluster
Open three terminals:

    ./raft-cluster-node 1
    ./raft-cluster-node 2
    ./raft-cluster-node 3

Send a command to the leader (use its node id):

    ./send-cmd 1 "PUT name rahul"

A non-leader replies `NOT_LEADER`. Every node prints `APPLIED` once the entry is committed.

## Run tests
    ./test-append-entries
    ./test-leader-state

## Known limitations
- State is not yet persisted to disk; a restarted node is caught up by the leader (persistence is next).
- No snapshots or cluster membership changes.

## References
- Ongaro and Ousterhout, "In Search of an Understandable Consensus Algorithm (Extended Version)"
