#pragma once
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "framing.h"

// Send one request, wait for one reply. Returns false on error or timeout.
inline bool rpc_call(const std::string& host, int port,
                     const std::string& request, std::string& response,
                     int timeout_ms) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    bool ok = connect(fd, (sockaddr*)&addr, sizeof(addr)) == 0
           && send_message(fd, request)
           && recv_message(fd, response);
    close(fd);
    return ok;
}

