#include <iostream>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

void handle_client(int client_fd) {
    char buf[1024];
    ssize_t n;
    while ((n = read(client_fd, buf, sizeof(buf))) > 0) {
        write(client_fd, buf, n);
    }
    close(client_fd);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9000);

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);
    std::cout << "listening on port 9000\n";

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        std::thread(handle_client, client_fd).detach();
    }
}
