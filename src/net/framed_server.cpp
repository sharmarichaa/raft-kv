#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include "framing.h"

void handle_client(int fd) {
    std::string msg;
    while (recv_message(fd, msg)) {
        std::cout << "got: " << msg << "\n";
        send_message(fd, "echo: " + msg);
    }
    close(fd);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9001);

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);
    std::cout << "framed server listening on port 9001\n";

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        std::thread(handle_client, client_fd).detach();
    }
}
