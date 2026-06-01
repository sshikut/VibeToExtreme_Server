#include <iostream>
#include <vector>
#include <thread>
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

const char* SERVER_IP = "127.0.0.1";
const uint16_t SERVER_PORT = 7777;

// ★ 소대장 스레드 (1개의 스레드가 botCount만큼의 소켓을 관리)
void BotGroupThread(int groupId, int botCount) {
    std::vector<SOCKET> sockets;
    sockets.reserve(botCount);

    SOCKADDR_IN serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = ::htons(SERVER_PORT);
    ::inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    std::cout << "[소대 " << groupId << "] 봇 연결 시작...\n";

    // 1. 소켓 연결 (Connect) 단계
    for (int i = 0; i < botCount; ++i) {
        SOCKET sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        // 연결은 블로킹으로 확실하게!
        if (::connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) != SOCKET_ERROR) {

            // ★ 핵심: 연결이 끝난 소켓을 '논블로킹(Non-Blocking)' 모드로 전환!
            u_long mode = 1;
            ::ioctlsocket(sock, FIONBIO, &mode);

            sockets.push_back(sock);
        }
        // 로그인 스톰 방지용 스로틀링 (10ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[소대 " << groupId << "] " << sockets.size() << "개 봇 연결 완료. 무한 이동 시작!\n";

    // 2. 1:N 멀티플렉싱 흉내내기 (Polling 루프)
    while (true) {
        for (SOCKET s : sockets) {
            // TODO: C2S_MovePacket 구조체 만들어서 쏘기
            char dummyPacket[18] = { 0, }; // 예시용 더미 배열

            // 논블로킹 소켓이므로 1000개를 순식간에 쏘고 넘어갑니다.
            ::send(s, dummyPacket, sizeof(dummyPacket), 0);
        }

        // 소대장 스레드가 1000명에게 지시를 다 내렸으면 1초간 휴식 (CPU 100% 방지)
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // 종료 시 소켓 정리
    for (SOCKET s : sockets) {
        ::closesocket(s);
    }
}

int main() {
    WSADATA wsaData;
    ::WSAStartup(MAKEWORD(2, 2), &wsaData);

    const int TOTAL_BOTS = 10000;
    const int BOTS_PER_THREAD = 1000;  // 스레드 1개당 1000마리 관리!
    const int THREAD_COUNT = TOTAL_BOTS / BOTS_PER_THREAD;

    std::vector<std::thread> threads;

    // 단 10개의 스레드만 생성! (메모리 낭비 원천 차단)
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back(BotGroupThread, i, BOTS_PER_THREAD);
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    ::WSACleanup();
    return 0;
}