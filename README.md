# raft-kv

Fault-tolerant distributed key-value store in C++17 built on the Raft consensus algorithm.

**Status:** work in progress

## Done so far
- TCP server handling multiple clients concurrently (one thread per connection)
- Length-prefixed message framing (4-byte header, 1 MB size cap)
- RPC layer with configurable timeouts; dead and slow nodes fail cleanly

## Planned
- 3-node cluster, leader election, log replication
- Write-ahead log with fsync and crash recovery
- GET/PUT/DEL state machine and retrying client
- Fault-injection tests (node crashes, partitions)

## Build
    mkdir build && cd build
    cmake .. && make
