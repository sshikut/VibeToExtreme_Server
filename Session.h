#pragma once
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>

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

private:
    int m_sessionId;
    SOCKET m_socket;
    bool m_inUse;
};