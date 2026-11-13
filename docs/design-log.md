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
