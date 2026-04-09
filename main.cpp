#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <Windows.h> // IOCP, HANDLE 등을 위해 필수
#include <iostream>
#include <stdexcept> // 에러 처리를 위한 예외(Exception) 라이브러리
#include <vector>
#include <thread>
#include <mutex>

std::mutex g_logMutex;

// [네트워크 코어 클래스]
class NetworkCore {
private:
    HANDLE m_iocpHandle; // 클래스가 남몰래 가지고 있는 데이터 (멤버 변수)
    SOCKET m_listenSocket; // 전화를 받을 대기용 소켓

    std::vector<std::thread> m_workerThreads; // 일꾼들을 관리할 명부(리스트)

    // 일꾼들이 하루 종일 일할 작업장 (함수)
    void WorkerThreadMain() {
        {
            // 이 중괄호 구역에 들어올 때 자물쇠를 잠그고, 나갈 때 자동으로 풉니다 (RAII 패턴)
            std::lock_guard<std::mutex> lock(g_logMutex);
            std::cout << "[System] 워커 스레드 ID: " << std::this_thread::get_id() << " 출근 완료!" << std::endl;
        }

        while (true) {
            // TODO: 나중에 여기서 GetQueuedCompletionStatus()를 호출하여
            // 우체국(IOCP)에서 날아온 우편물을 무한히 꺼내서 처리할 것입니다.

            // 당장 무한 루프가 CPU를 100% 잡아먹는 것을 막기 위해 1초 대기
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

public:
    // 1. 생성자 (Constructor): NetworkCore 객체가 태어날 때 자동으로 딱 1번 실행됨
    NetworkCore() : m_iocpHandle(NULL), m_listenSocket(INVALID_SOCKET) {
        WSADATA wsaData;

        // WinSock 2.2 버전 사용을 운영체제에 신고합니다.
        int result = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            // [기초] 에러가 났다고 단순히 printf 치고 넘어가는 건 하수입니다.
            // "나 일 못해!" 하고 폭탄(예외)을 던져버립니다.
            throw std::runtime_error("WSAStartup 실패! 네트워크를 켤 수 없습니다.");
        }
        std::cout << "[System] Winsock 초기화 성공." << std::endl;
    }

    // 2. 소멸자 (Destructor): NetworkCore 객체가 죽을 때(메모리에서 사라질 때) 자동으로 실행됨
    ~NetworkCore() {
        if (m_iocpHandle != NULL) {
            ::CloseHandle(m_iocpHandle); // IOCP 핸들도 깔끔하게 닫아줌
        }
        ::WSACleanup(); // 윈도우 OS에 빌렸던 네트워크 자원 반납
        std::cout << "[System] Winsock 자원 반환 완료. 서버가 안전하게 종료됩니다." << std::endl;
    }

    // 3. IOCP 핸들(우체국) 생성 함수
    void InitializeIOCP() {
        // 운영체제에게 "나 비동기 입출력(IOCP) 쓸 거야, 관리할 우체국 하나 지어줘!" 라고 요청
        m_iocpHandle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

        if (m_iocpHandle == NULL) {
            throw std::runtime_error("IOCP 핸들 생성 실패!"); // 실패하면 얄짤없이 폭탄 투척
        }
        std::cout << "[System] IOCP 코어(우체국) 핸들 생성 성공." << std::endl;
    }

    // [서버 시작 함수]
    void StartServer(uint16_t port) {
        // 1. 소켓(전화기) 만들기
        // AF_INET(IPv4), SOCK_STREAM(TCP) 방식을 사용하는 전화기를 개통합니다.
        m_listenSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listenSocket == INVALID_SOCKET) {
            throw std::runtime_error("Listen 소켓 생성 실패!");
        }

        // 2. 전화기에 번호(IP와 Port) 부여하기 (Bind)
        SOCKADDR_IN serverAddr = {}; // 구조체를 0으로 깔끔하게 초기화
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = ::htons(port); // 포트 번호를 네트워크 바이트 순서(Big-Endian)로 변환
        serverAddr.sin_addr.s_addr = ::htonl(INADDR_ANY); // 어떤 랜선(IP)으로 들어오든 다 받겠다

        if (::bind(m_listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            throw std::runtime_error("소켓 Bind 실패! (포트가 이미 사용 중일 수 있습니다)");
        }

        // 3. 전화기 벨소리 켜기 (Listen)
        // SOMAXCONN: 동시에 대기할 수 있는 최대 연결 수를 OS에 맡김
        if (::listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
            throw std::runtime_error("소켓 Listen 실패!");
        }

        std::cout << "[System] 서버가 포트 " << port << "에서 성공적으로 열렸습니다! 클라이언트를 기다립니다..." << std::endl;
        
        // 4. 일꾼(스레드) 고용하기
        // 실무에서는 보통 (CPU 코어 개수 * 2) 명의 일꾼을 고용합니다.
        unsigned int coreCount = std::thread::hardware_concurrency();
        unsigned int workerCount = coreCount * 2;

        std::cout << "[System] CPU 코어 개수: " << coreCount << "개. " << workerCount << "명의 일꾼을 고용합니다." << std::endl;

        for (unsigned int i = 0; i < workerCount; ++i) {
            // 스레드를 생성하여 m_workerThreads 명부에 차곡차곡 넣습니다.
            m_workerThreads.emplace_back(&NetworkCore::WorkerThreadMain, this);
        }
    }
};

int main() {
    // 1. 메모리 누수 탐지
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    // 2. 콘솔 출력을 UTF-8 모드로 강제 고정 (파일이 UTF-8로 저장되어 있으므로 이제 완벽히 호환됨)
    SetConsoleOutputCP(CP_UTF8);

    std::cout << "=== Vibe to Extreme - IOCP Server Booting ===" << std::endl;

    try {
        NetworkCore serverCore;
        serverCore.InitializeIOCP();

        // 7777번 포트로 서버 귀 열기 및 일꾼 스레드 고용
        serverCore.StartServer(7777);

        // 메인 스레드가 바로 종료되지 않도록 무한 대기 (추후 Accept 로직이 들어갈 자리)
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

    }
    catch (const std::exception& e) {
        std::cerr << "🚨 에러: " << e.what() << std::endl;
    }

    return 0;
}