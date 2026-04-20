#include "NetworkCore.h"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <string>

// OS의 에러 코드를 사람이 읽을 수 있는 문자열로 번역해 주는 마법의 함수
std::string GetWindowsErrorMessage(int errorCode) {
    if (errorCode == 0) return "에러 없음";

    char* errorText = nullptr;
    ::FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        (LPSTR)&errorText,
        0,
        NULL
    );

    if (errorText) {
        std::string result(errorText);
        ::LocalFree(errorText); // 메모리 누수 방지 (매우 중요!)

        // 문자열 끝의 줄바꿈 문자 제거
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
        return result;
    }
    return "알 수 없는 에러 (코드: " + std::to_string(errorCode) + ")";
}

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

        // 세 번째 인자((ULONG_PTR)session)는 워커 스레드에게 "이 편지는 몇 번 방 손님이 보낸 거야"라고 알려주는 이름표(Key)입니다.
        ::CreateIoCompletionPort((HANDLE)clientSocket, m_iocpHandle, (ULONG_PTR)session, 0);

        // 세션이 보내는 편지 받기
        session->PostRecv();
    }
}

void NetworkCore::WorkerThreadMain() {
    DWORD bytesTransferred = 0;
    ULONG_PTR completionKey = 0;
    LPOVERLAPPED overlapped = nullptr;

    while (m_running) {
        // 우체국(IOCP)에서 편지가 오길 기다립니다.
        BOOL result = ::GetQueuedCompletionStatus(
            m_iocpHandle,
            &bytesTransferred,
            &completionKey,
            &overlapped,
            INFINITE
        );

        if (!result && overlapped == nullptr) {
            // 우체국이 닫혔습니다. 스레드를 조용히 퇴근시킵니다.
            break;
        }

        // 이름표를 보고 어떤 세션인지 확인합니다.
        Session* session = reinterpret_cast<Session*>(completionKey);
        OverlappedContext* context = reinterpret_cast<OverlappedContext*>(overlapped);

        // 클라이언트가 강제로 랜선을 뽑거나 접속을 종료했을 때!
        if (!result || bytesTransferred == 0) {
            if (session) {
                int sessionId = session->GetSessionId();
                std::ofstream logFile("server_log.txt", std::ios::app);

                if (result && bytesTransferred == 0) {
                    // [상황 A] OS가 대신 닫아주거나, 정상 로그아웃한 경우
                    std::cout << "[정상 종료] 세션 " << sessionId << " 연결 해제." << std::endl;
                    if (logFile.is_open()) logFile << "[INFO] Session " << sessionId << " Gracefully Disconnected.\n";
                }
                else {
                    // [상황 B] 진짜 비정상 끊김 (랜선 뽑힘 등)
                    // ★ WSAGetLastError가 아니라 GetLastError를 써야 합니다!
                    int errCode = ::GetLastError();
                    std::string errMsg = GetWindowsErrorMessage(errCode);

                    std::cout << "[비정상 종료] 세션 " << sessionId << " | 사유: " << errMsg << std::endl;
                    if (logFile.is_open()) logFile << "-> FATAL ERROR: " << errMsg << " (Code: " << errCode << ")\n";
                }
                if (logFile.is_open()) logFile.close();

                m_sessionManager->Release(session);
                ::closesocket(session->GetSocket());
            }
            continue;
        }

        // [핵심] 유니티에서 보낸 패킷 타입 확인
        PacketType* type = reinterpret_cast<PacketType*>(context->buffer);

        // 다시 다음 편지를 받을 준비 (WSARecv 재호출 로직 생략)
    }
}

