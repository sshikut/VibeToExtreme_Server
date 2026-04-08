#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <iostream>

// 절대 전역 변수로 Winsock을 초기화하지 마세요. 
// 클래스의 생성자와 소멸자를 이용한 RAII 패턴으로 래핑(Wrapping)합니다.
class NetworkCore {
public:
    NetworkCore() {
        WSADATA wsaData;
        // TODO: WSAStartup을 호출하고, 실패 시 서버를 어떻게 안전하게 종료(Crash)시킬지 설계하세요.
        // 단순 std::cout 출력은 허용하지 않습니다.
    }

    ~NetworkCore() {
        // TODO: WSACleanup 호출 보장
    }

    void InitializeIOCP() {
        // TODO: CreateIoCompletionPort를 호출하여 핸들을 생성하세요.
        // 여기서 반환된 HANDLE은 서버 전체의 생명줄입니다.
    }
};

int main() {
    // 1. 메모리 누수 탐지기(CRT Debug) 활성화 (디버그 모드 전용)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    std::cout << "Vibe to Extreme - IOCP Server Booting..." << std::endl;

    try {
        NetworkCore serverCore;
        serverCore.InitializeIOCP();

        // TODO: Listener 소켓 생성 및 Accept 스레드 분리
    }
    catch (const std::exception& e) {
        // 치명적 예외 발생 시 미니덤프를 남기거나 안전하게 로그를 남기고 종료해야 합니다.
    }

    return 0;
}