#include <iostream>
#include <csignal>
#include "rpc.h"

int main() {
    signal(SIGPIPE, SIG_IGN);
    std::string resp;

    // 1. normal call
    if (rpc_call("127.0.0.1", 9002, "hello", resp, 1000))
        std::cout << "1. ok: " << resp << "\n";
    else
        std::cout << "1. failed\n";

    // 2. server takes 3s, we only wait 1s
    if (rpc_call("127.0.0.1", 9002, "slow", resp, 1000))
        std::cout << "2. ok: " << resp << "\n";
    else
        std::cout << "2. timed out\n";

    // 3. nobody listening on this port
    if (rpc_call("127.0.0.1", 9999, "hello", resp, 1000))
        std::cout << "3. ok: " << resp << "\n";
    else
        std::cout << "3. node down\n";
}

