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
    return "Unknown error (Code:" + std::to_string(errorCode) + ")";
}

NetworkCore::NetworkCore()
    : m_iocpHandle(NULL), m_listenSocket(INVALID_SOCKET), m_running(false) {
    WSADATA wsaData;
    if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed");
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
    if (m_iocpHandle == NULL) throw std::runtime_error("Failed to create IOCP");
}

void NetworkCore::StartServer(uint16_t port) {
    // 1. 소켓 생성 및 Bind/Listen (복구 완료)
    m_listenSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET) throw std::runtime_error("Failed to create listen socket");

    SOCKADDR_IN serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = ::htons(port);
    serverAddr.sin_addr.s_addr = ::htonl(INADDR_ANY);

    if (::bind(m_listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        throw std::runtime_error("Socket bind failed");
    }

    if (::listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        throw std::runtime_error("Socket listen failed");
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

    // 5. 내부 상태 모니터링 (하트비트) 스레드 추가 ★
    std::thread([this]() {
        while (m_running) {
            // 3초마다 현재 남은 빈방 갯수를 콘솔에 출력합니다.
            size_t freeCount = m_sessionManager->GetAvailableSessionCount();
            std::cout << "\n=======================================" << std::endl;
            std::cout << "[Server Status] Available Sessions: " << freeCount << " / 10000" << std::endl;
            std::cout << "=======================================\n" << std::endl;

            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
        }).detach();
}

void NetworkCore::AcceptThreadMain() {
    std::cout << "[System] Accept thread started. Waiting for clients..." << std::endl;

    while (m_running) {
        SOCKET clientSocket = ::accept(m_listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) continue;

        Session* session = m_sessionManager->Acquire();
        if (!session) {
            std::cout << "[WARNING] Server full! Connection rejected." << std::endl;
            ::closesocket(clientSocket);
            continue;
        }

        session->SetSocket(clientSocket);

        ::CreateIoCompletionPort((HANDLE)clientSocket, m_iocpHandle, (ULONG_PTR)session, 0);

        // ★ 핵심 방어막: PostRecv가 즉시 실패했는지 확인!
        bool isSuccess = session->PostRecv();

        if (!isSuccess) {
            // 악성 봇이 접속하자마자 끊고 도망간 최악의 상황!
            // 워커 스레드로 못 넘어가니, 여기서 직접 방 열쇠를 뺏어서 반납해야 합니다.
            std::cout << "[WARNING] Connection dropped immediately after accept (Ghost session prevented). Session released." << std::endl;
            m_sessionManager->Release(session);
            ::closesocket(clientSocket);
        }
        else {
            // 정상적으로 대기열에 들어간 경우에만 성공으로 취급
            std::cout << "[INFO] Client connected and listening. Session ID: " << session->GetSessionId() << std::endl;

            // ==============================================================
            // 🎯 초기 동기화 (Initial Synchronization) 시작!
            // ==============================================================

            // 뉴비가 0번 격자에 몰리지 않도록 초기 좌표를 맵 전체에 랜덤하게 흩뿌립니다!
            float randomX = static_cast<float>(rand() % (MAX_SECTOR_X * SECTOR_SIZE));
            float randomY = static_cast<float>(rand() % (MAX_SECTOR_Y * SECTOR_SIZE));

            // 처음 접속 시 해당 랜덤 격자의 연결 리스트에 나를 밀어 넣습니다.
            m_sessionManager->UpdateSessionSector(session, randomX, randomY);

            // 뉴비에게 "너의 고유 ID는 이거야!" 라고 알려줍니다.
            S2C_LoginPacket loginPkt;
            loginPkt.size = sizeof(S2C_LoginPacket);
            loginPkt.id = 5;
            loginPkt.mySessionId = session->GetSessionId();
            session->Send((char*)&loginPkt, sizeof(loginPkt));

            // 1. 기존 유저들에게 "뉴비가 왔어!" 라고 브로드캐스팅
            S2C_SpawnPacket spawnPkt;
            spawnPkt.size = sizeof(S2C_SpawnPacket);
            spawnPkt.id = 4;
            spawnPkt.sessionId = session->GetSessionId();
            spawnPkt.spawnX = 0.0f;
            spawnPkt.spawnY = 0.0f;
            m_sessionManager->Broadcast((char*)&spawnPkt, sizeof(spawnPkt), session->GetSessionId());

            // 2. 뉴비에게 "기존에 있던 고인물 명단이야!" 라고 1:1 동기화
            m_sessionManager->SyncExistingSessions(session);

            // ==============================================================
        }
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

            // ★ 에러 코드가 다른 함수(ofstream 등)에 의해 오염되기 전에 즉시 구출합니다!
            int errCode = (!result) ? ::WSAGetLastError() : 0;

            if (session) {
                int sessionId = session->GetSessionId();
                std::cout << "[INFO] Session disconnected:" << sessionId << std::endl;

                // ★ 1. 퇴장 패킷 브로드캐스트 (이제 O(N^2)가 아니라 내 주변 격자에게만 쏘면 됨!)
                S2C_LeavePacket leavePkt;
                leavePkt.size = sizeof(S2C_LeavePacket);
                leavePkt.id = 3;
                leavePkt.sessionId = sessionId;

                auto [gridX, gridY] = m_sessionManager->GetSectorIndex(session->GetPosX(), session->GetPosY());
                // (구현 생략) 내 주변 3x3 격자에만 leavePkt 쏘기

                // ★ 2. 작성자님이 짚어낸 핵심: 연결 리스트에서 앞뒤 사람 손 이어주고 빠지기!
                m_sessionManager->RemoveSessionFromItsSector(session);

                // ★ 3. 그제야 안전하게 세션을 빈방으로 반납합니다.
                m_sessionManager->Release(session);
                ::closesocket(session->GetSocket());
            }
            continue;
        }

        // 워커 스레드는 그저 "데이터가 n바이트 도착했어!" 라고 세션에게 알려주기만 하면 됩니다.
        if (session != nullptr) {
            // ★ 편지봉투(Context)의 꼬리표를 보고 분기 처리!
            if (context->type == IOType::RECV) {
                // 수신 완료 이벤트: 패킷을 뜯어보고 로직(이동, 브로드캐스팅) 처리
                session->OnReceive(bytesTransferred, m_sessionManager.get());
            }
            else if (context->type == IOType::SEND) {
                // 송신 완료 이벤트: 다음 패킷이 큐에 남아있는지 확인하고 이어서 쏘기
                session->OnSendCompleted(bytesTransferred);
            }
        }
    }
}

