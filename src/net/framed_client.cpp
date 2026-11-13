#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "framing.h"

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9001);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "connect failed\n";
        return 1;
    }

    send_message(fd, "hello");
    send_message(fd, "world");
    send_message(fd, "raft");

    std::string reply;
    for (int i = 0; i < 3; i++) {
        if (!recv_message(fd, reply)) break;
        std::cout << reply << "\n";
    }
    close(fd);
}
