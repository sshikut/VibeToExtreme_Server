// NetworkCore.h
#pragma once
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <vector>
#include <thread>
#include <memory>
#include <atomic>
#include "SessionManager.h"

class NetworkCore {
public:
    NetworkCore();
    ~NetworkCore();

    void InitializeIOCP();
    void StartServer(uint16_t port);

private:
    void WorkerThreadMain();
    void AcceptThreadMain();

private:
    HANDLE m_iocpHandle;
    SOCKET m_listenSocket;

    std::atomic<bool> m_running;
    std::unique_ptr<SessionManager> m_sessionManager;

    std::vector<std::thread> m_workerThreads;
    std::thread m_acceptThread;
};