#pragma once
#pragma comment(lib, "ws2_32.lib")
#include <cstdint>
#include <WinSock2.h>
#include <iostream>

#pragma pack(push, 1)
struct PacketHeader {
    uint16_t size;
    uint16_t id;
};
#pragma pack(pop)

enum class PacketType : uint16_t {
    NONE = 0,
    C2S_MOVE = 1,       // 클라이언트 -> 서버 이동
    S2C_MOVE_BROAD = 2, // 서버 -> 클라이언트 이동 브로드캐스팅
    CRASH_BOMB = 999,
};

// buffer 변수를 삭제하고 가볍게 만듭니다.
struct OverlappedContext {
    WSAOVERLAPPED overlapped;
    WSABUF wsaBuf;
};

class Session {
public:
    Session() { Reset(); }
    ~Session() {}

    void Reset() {
        m_inUse = false;
        m_socket = INVALID_SOCKET;
        m_readPos = 0;
        m_writePos = 0;
    }

    void SetSessionId(int id) { m_sessionId = id; }
    int GetSessionId() const { return m_sessionId; }

    SOCKET GetSocket() const { return m_socket; }
    void SetSocket(SOCKET s) { m_socket = s; m_inUse = true; }

    bool IsFree() const { return !m_inUse; }
    OverlappedContext& GetReceiveContext() { return m_recvContext; }

    // 비동기 수신 대기
    bool PostRecv() {
        DWORD recvBytes = 0;
        DWORD flags = 0;

        ZeroMemory(&m_recvContext.overlapped, sizeof(m_recvContext.overlapped));

        // ★ 핵심: OS에게 "내 recvBuffer의 writePos 위치부터 빈 공간만큼 데이터를 채워줘!" 라고 부탁합니다.
        m_recvContext.wsaBuf.buf = &m_recvBuffer[m_writePos];
        m_recvContext.wsaBuf.len = BUFFER_SIZE - m_writePos;

        if (::WSARecv(m_socket, &m_recvContext.wsaBuf, 1, &recvBytes, &flags, &m_recvContext.overlapped, nullptr) == SOCKET_ERROR) {
            if (::WSAGetLastError() != WSA_IO_PENDING) {
                std::cout << "🚨 WSARecv 에러 발생: " << ::WSAGetLastError() << std::endl;
                return false;
            }
        }
        return true;
    }

    // ★ 질문 3에 대한 답변: 파싱 루틴은 이 함수 안에 들어갑니다! (Session.cpp에 구현)
    void OnReceive(int bytesTransferred);

private:
    OverlappedContext m_recvContext;
    int m_sessionId;
    SOCKET m_socket;
    bool m_inUse;

    // ★ 질문 1에 대한 답변: 각 세션이 자기만의 버퍼와 커서를 가져야 합니다.
    static const int BUFFER_SIZE = 65535;
    char m_recvBuffer[BUFFER_SIZE];
    int m_readPos;
    int m_writePos;
};