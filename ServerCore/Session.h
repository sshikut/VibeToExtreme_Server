#pragma once
#pragma comment(lib, "ws2_32.lib")
#include <cstdint>
#include <WinSock2.h>
#include <iostream>

enum class PacketType : uint16_t {
    NONE = 0,
    CRASH_BOMB = 999, // 이 번호가 오면 서버를 터뜨립니다.
};

struct OverlappedContext {
    WSAOVERLAPPED overlapped;
    WSABUF wsaBuf;
    char buffer[1024];
};

class Session {
public:
    Session();
    ~Session();

    void Reset();

    // 내 방 번호를 설정하고 가져오는 함수
    void SetSessionId(int id) { m_sessionId = id; }
    int GetSessionId() const { return m_sessionId; }

    SOCKET GetSocket() const { return m_socket; }
    void SetSocket(SOCKET s) { m_socket = s; m_inUse = true; }

    void OnReceive(int bytesTransferred);
    OverlappedContext& GetReceiveContext() { return m_recvContext; }

    bool IsFree() const { return !m_inUse; }

    bool PostRecv() {
        DWORD recvBytes = 0;
        DWORD flags = 0;

        // 매번 받을 때마다 OVERLAPPED 구조체를 깨끗하게 초기화해야 합니다. (매우 중요)
        ZeroMemory(&m_recvContext.overlapped, sizeof(m_recvContext.overlapped));
        m_recvContext.wsaBuf.buf = m_recvContext.buffer;
        m_recvContext.wsaBuf.len = sizeof(m_recvContext.buffer);

        // 비동기 수신(WSARecv) 요청!
        if (::WSARecv(m_socket, &m_recvContext.wsaBuf, 1, &recvBytes, &flags, &m_recvContext.overlapped, nullptr) == SOCKET_ERROR) {
            if (::WSAGetLastError() != WSA_IO_PENDING) {
                std::cout << "🚨 WSARecv 에러 발생: " << ::WSAGetLastError() << std::endl;
                return false; // ★ 핵심: 즉시 에러가 났으니 Accept 스레드에게 실패했다고 알림!
            }
        }
        return true; // ★ 무사히 대기열에 들어갔거나, 즉시 완료됨
    }

private:
    OverlappedContext m_recvContext;
    int m_sessionId;
    SOCKET m_socket;
    bool m_inUse;
};