// main.cpp
#include <iostream>
#include "NetworkCore.h"

int main() {

    try {
        NetworkCore server;
        server.InitializeIOCP();
        server.StartServer(7777);

        std::cout << "[System] Server is running. Press any key to terminate..." << std::endl;
        std::cin.get();

    }
    catch (const std::exception& e) {
        std::cerr << "[FATAL] Exception caught: " << e.what() << std::endl;
    }

    return 0;
}