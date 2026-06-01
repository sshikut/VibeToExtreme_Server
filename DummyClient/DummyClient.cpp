#include <iostream>
#include <vector>
#include <thread>
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// 주의: 실제로는 서버 프로젝트의 Session.h를 include 하거나 구조체를 복사해와야 합니다.
const char* SERVER_IP = "127.0.0.1";
const uint16_t SERVER_PORT = 7777;

void BotBehaviorThread(int botId) {
    SOCKET sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return;

    SOCKADDR_IN serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = ::htons(SERVER_PORT);
    ::inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    // 1. 서버에 연결 시도
    if (::connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "[Bot " << botId << "] 접속 실패\n";
        ::closesocket(sock);
        return;
    }

    std::cout << "[Bot " << botId << "] 서버 접속 성공!\n";

    // 2. FSM 기반 메인 루프 (추후 여기에 사람처럼 움직이는 로직 추가)
    while (true) {
        // TODO: 난수 좌표 생성 및 C2S_MovePacket 전송 로직

        // 봇 하나가 너무 미친듯이 패킷을 쏘지 않게 1초 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    ::closesocket(sock);
}

int main() {
    // 한글 깨짐 방지
    SetConsoleOutputCP(CP_UTF8);

    WSADATA wsaData;
    if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return -1;

    std::cout << "더미 봇 부대를 투하합니다...\n";

    const int BOT_COUNT = 100; // 일단 100개로 테스트!
    std::vector<std::thread> botThreads;

    for (int i = 0; i < BOT_COUNT; ++i) {
        botThreads.emplace_back(BotBehaviorThread, i);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 메인 스레드 종료 방지
    for (auto& t : botThreads) {
        if (t.joinable()) t.join();
    }

    ::WSACleanup();
    return 0;
}