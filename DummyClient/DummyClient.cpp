#include <iostream>
#include <vector>
#include <thread>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <random>

#pragma comment(lib, "ws2_32.lib")

const char* SERVER_IP = "127.0.0.1";
const uint16_t SERVER_PORT = 7777;

#pragma pack(push, 1)
struct C2S_MovePacket {
    uint16_t size;
    uint16_t id;
    int32_t sessionId;
    float posX;
    float posY;
    float dirX;
    float dirY;
};
#pragma pack(pop)

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

    // ★ 모던 C++의 스레드 독립적 난수 생성기 세팅 (스레드마다 다른 Seed 부여!)
    std::mt19937 rng(std::random_device{}() ^ (groupId + 12345));
    std::uniform_real_distribution<float> distPos(0.0f, 240.0f); // 0~30 좌표
    std::uniform_int_distribution<int> distDir(0, 1);           // 방향 0 또는 1

    // 2. 1:N 멀티플렉싱 흉내내기 (Polling 루프)
    char dumpBuffer[65535]; // 배수구 뚫기용 버퍼

    while (true) {
        for (SOCKET s : sockets) {
            C2S_MovePacket movePkt;
            movePkt.size = sizeof(C2S_MovePacket);
            movePkt.id = 1;
            movePkt.sessionId = 0;

            // ★ 더 이상 겹치지 않는 완벽한 독립 난수 좌표 생성!
            movePkt.posX = distPos(rng);
            movePkt.posY = distPos(rng);
            movePkt.dirX = (distDir(rng) == 0) ? -1.0f : 1.0f;
            movePkt.dirY = 0.0f;

            ::send(s, (char*)&movePkt, sizeof(movePkt), 0);

            while (::recv(s, dumpBuffer, sizeof(dumpBuffer), 0) > 0) {}
        }
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

    const int TOTAL_BOTS = 1000;
    const int BOTS_PER_THREAD = 100;  // 스레드 1개당 1000마리 관리!
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