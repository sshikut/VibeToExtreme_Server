#include "NetworkCore.h"
#include <iostream>
#include <stdexcept>

NetworkCore::NetworkCore()
    : m_iocpHandle(NULL), m_listenSocket(INVALID_SOCKET), m_running(false) {
    WSADATA wsaData;
    if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup 실패");
    }
}

NetworkCore::~NetworkCore() {
    m_running = false;
    if (m_listenSocket != INVALID_SOCKET) ::closesocket(m_listenSocket);
    if (m_iocpHandle != NULL) ::CloseHandle(m_iocpHandle);
    ::WSACleanup();
}

void NetworkCore::InitializeIOCP() {
    m_iocpHandle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (m_iocpHandle == NULL) throw std::runtime_error("IOCP 생성 실패");
}

void NetworkCore::StartServer(uint16_t port) {
    // 1. 소켓 생성 및 Bind/Listen (복구 완료)
    m_listenSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET) throw std::runtime_error("Listen 소켓 생성 실패!");

    SOCKADDR_IN serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = ::htons(port);
    serverAddr.sin_addr.s_addr = ::htonl(INADDR_ANY);

    if (::bind(m_listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        throw std::runtime_error("소켓 Bind 실패!");
    }

    if (::listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        throw std::runtime_error("소켓 Listen 실패!");
    }

    // 2. 세션 매니저 초기화 (1만 개)
    m_sessionManager = std::make_unique<SessionManager>(10000);
    m_running = true;

    // 3. Accept 스레드 기동
    m_acceptThread = std::thread(&NetworkCore::AcceptThreadMain, this);
    m_acceptThread.detach();

    // 4. 워커 스레드 풀 생성
    unsigned int workerCount = std::thread::hardware_concurrency() * 2;
    for (unsigned int i = 0; i < workerCount; ++i) {
        m_workerThreads.emplace_back(&NetworkCore::WorkerThreadMain, this);
    }

    // 워커 스레드들은 메인 흐름과 분리
    for (auto& t : m_workerThreads) {
        t.detach();
    }
}

void NetworkCore::AcceptThreadMain() {
    std::cout << "[System] Accept 스레드 가동. 클라이언트의 접속을 기다립니다..." << std::endl;

    while (m_running) {
        SOCKET clientSocket = ::accept(m_listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) continue;

        Session* session = m_sessionManager->Acquire();
        if (!session) {
            std::cout << "🚨 [경고] 서버 인원 초과! 접속을 거절합니다." << std::endl;
            ::closesocket(clientSocket);
            continue;
        }

        session->SetSocket(clientSocket);
        std::cout << "🎉 접속 성공! 세션 인덱스: " << session->GetSessionId() << std::endl;

        // TODO: 여기서 CreateIoCompletionPort로 IOCP 연결
    }
}

void NetworkCore::WorkerThreadMain() {
    while (m_running) {
        // TODO: GetQueuedCompletionStatus() 로직
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}