#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include "rpc.h"

const int BASE_PORT = 9100;
const int NUM_NODES = 3;

void handle_client(int fd, int id) {
    std::string msg;
    while (recv_message(fd, msg)) {
        send_message(fd, "pong from node " + std::to_string(id));
    }
    close(fd);
}

void serve(int id) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(BASE_PORT + id);
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);
    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        std::thread(handle_client, client_fd, id).detach();
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: ping-node <id 1-3>\n";
        return 1;
    }
    int id = std::atoi(argv[1]);
    signal(SIGPIPE, SIG_IGN);
    std::thread(serve, id).detach();

    while (true) {
        for (int peer = 1; peer <= NUM_NODES; peer++) {
            if (peer == id) continue;
            std::string resp;
            bool ok = rpc_call("127.0.0.1", BASE_PORT + peer,
                               "ping from node " + std::to_string(id), resp, 500);
            std::cout << "node " << id << " -> " << peer << ": "
                      << (ok ? resp : "DOWN") << "\n";
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

