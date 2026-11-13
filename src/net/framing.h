#pragma once
#include <string>
#include <cstdint>
#include <unistd.h>
#include <arpa/inet.h>

inline bool send_all(int fd, const void* data, size_t len) {
    const char* p = static_cast<const char*>(data);
    while (len > 0) {
        ssize_t n = write(fd, p, len);
        if (n <= 0) return false;
        p += n;
        len -= n;
    }
    return true;
}

inline bool recv_all(int fd, void* data, size_t len) {
    char* p = static_cast<char*>(data);
    while (len > 0) {
        ssize_t n = read(fd, p, len);
        if (n <= 0) return false;
        p += n;
        len -= n;
    }
    return true;
}

inline bool send_message(int fd, const std::string& msg) {
    uint32_t len = htonl(static_cast<uint32_t>(msg.size()));
    return send_all(fd, &len, 4) && send_all(fd, msg.data(), msg.size());
}

inline bool recv_message(int fd, std::string& out) {
    uint32_t len;
    if (!recv_all(fd, &len, 4)) return false;
    len = ntohl(len);
    if (len > 1024 * 1024) return false;
    out.resize(len);
    return recv_all(fd, out.data(), len);
}

