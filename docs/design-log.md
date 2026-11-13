# Design Log

## Message framing
- TCP is a byte stream, so read() can return half a message or two glued together.
- recv_all loops until exactly N bytes arrive.
- htonl fixes byte order so different machines agree on the length.
- 1 MB max size check stops a bad length from allocating huge memory.
