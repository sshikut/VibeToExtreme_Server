// main.cpp
#include <iostream>
#include "NetworkCore.h"

int main() {
    // 한글 깨짐 방지
    SetConsoleOutputCP(CP_UTF8);

    try {
        NetworkCore server;
        server.InitializeIOCP();
        server.StartServer(7777);

        std::cout << "서버가 가동 중입니다. 종료하려면 아무 키나 누르세요..." << std::endl;
        std::cin.get();

    }
    catch (const std::exception& e) {
        std::cerr << "🚨 에러 발생: " << e.what() << std::endl;
    }

    return 0;
}