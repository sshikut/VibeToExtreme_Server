#include "Session.h"

Session::Session() : m_sessionId(-1), m_socket(INVALID_SOCKET), m_inUse(false) {}

Session::~Session() {
    if (m_socket != INVALID_SOCKET) {
        ::closesocket(m_socket);
    }
}

void Session::Reset() {
    m_inUse = false;
    m_socket = INVALID_SOCKET;
    m_readPos = 0;
    m_writePos = 0;
}

void Session::OnReceive(int bytesTransferred) {
    m_writePos += bytesTransferred; // OS가 데이터를 넣어줬으니 쓰기 커서 전진!

    while (true) {
        // ... (아까 알려준 패킷 조립 및 while 루프 로직) ...
        // 패킷이 완성되면 ProcessPacket(id, body) 호출!
    }

    // 다 처리하고 남은 짜투리 데이터가 있다면 버퍼 맨 앞으로 복사 (매우 중요)
    int dataSize = m_writePos - m_readPos;
    if (dataSize > 0 && m_readPos > 0) {
        memmove(m_recvBuffer, &m_recvBuffer[m_readPos], dataSize);
    }
    m_writePos = dataSize;
    m_readPos = 0;

    // 파싱이 끝났으니, 다음 데이터를 받기 위해 다시 WSARecv 대기열에 올립니다!
    PostRecv();
}