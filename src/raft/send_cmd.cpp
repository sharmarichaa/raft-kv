#include <iostream>
#include <string>
#include <cstdlib>
#include "../net/rpc.h"

// usage: send-cmd <node id 1-3> "PUT name rahul"
int main(int argc, char** argv) {
    if (argc < 3) { std::cerr << "usage: send-cmd <id> <command>\n"; return 1; }
    int id = std::atoi(argv[1]);
    std::string resp;
    if (!rpc_call("127.0.0.1", 9300 + id, "CMD " + std::string(argv[2]), resp, 1000))
        std::cout << "node " << id << " unreachable\n";
    else
        std::cout << resp << "\n";
}

