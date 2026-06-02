#pragma once
#pragma comment(lib, "ws2_32.lib")
#include <cstdint>
#include <WinSock2.h>
#include <iostream>
#include <vector>
#include <mutex>
#include <atomic>

#pragma pack(push, 1)
struct PacketHeader {
    uint16_t size;
    uint16_t id;
};
#pragma pack(pop)

#pragma pack(push, 1)
// 이동 패킷 본문 (Body)
struct C2S_MovePacket {
    int32_t sessionId; // 누가 움직이는가? (서버가 클라를 식별하기 위함, 4바이트)
    float posX;        // 현재 X 좌표 (4바이트)
    float posY;        // 현재 Y 좌표 (4바이트)
    float dirX;        // X축 바라보는 방향 (-1, 0, 1) (4바이트)
    float dirY;        // Y축 바라보는 방향 (-1, 0, 1) (4바이트)
};
#pragma pack(pop)

#pragma pack(push, 1)
struct S2C_SpawnPacket {
    uint16_t size;
    uint16_t id;        // 4 (예: 입장 패킷 ID)
    int32_t sessionId;  // 새로 들어온 놈의 번호
    float spawnX;       // 스폰 X 위치
    float spawnY;       // 스폰 Y 위치
};
#pragma pack(pop)

#pragma pack(push, 1)
struct S2C_LeavePacket {
    uint16_t size;      // 8바이트 (size + id + sessionId)
    uint16_t id;        // 3 (예: 퇴장 패킷 ID)
    int32_t sessionId;  // 방금 나간 놈의 번호
};
#pragma pack(pop)

#pragma pack(push, 1)
struct S2C_LoginPacket {
    uint16_t size;
    uint16_t id;       // 5번: 로그인 (ID 부여)
    int32_t mySessionId;
};
#pragma pack(pop)

enum class PacketType : uint16_t {
    NONE = 0,
    C2S_MOVE = 1,       // 클라이언트 -> 서버 이동
    S2C_MOVE_BROAD = 2, // 서버 -> 클라이언트 이동 브로드캐스팅
    CRASH_BOMB = 999,
};

enum class IOType { RECV, SEND };

// buffer 변수를 삭제하고 가볍게 만듭니다.
struct OverlappedContext {
    WSAOVERLAPPED overlapped;
    WSABUF wsaBuf;
    IOType type; // ★ 이 Context가 수신용인지 송신용인지 구분하는 꼬리표
};

class SessionManager;

class Session {
public:
    Session();
    ~Session();
    void Reset();

    void OnReceive(int bytesTransferred, SessionManager* manager);
    void OnSendCompleted(int bytesTransferred); // ★ 송신 완료 콜백

    void SetSessionId(int id) { m_sessionId = id; }
    int GetSessionId() const { return m_sessionId; }

    SOCKET GetSocket() const { return m_socket; }
    void SetSocket(SOCKET s) { m_socket = s; m_inUse = true; }

    bool IsFree() const { return !m_inUse; }

    bool PostRecv();
    void PostSend(); // ★ 비동기 송신 위임 함수

    bool Send(char* packet, int size);

    void SetPosition(float x, float y) { m_posX = x; m_posY = y; }
    float GetPosX() const { return m_posX; }
    float GetPosY() const { return m_posY; }

    Session* prevSectorNode = nullptr;
    Session* nextSectorNode = nullptr;

private:
    OverlappedContext m_recvContext;
    OverlappedContext m_sendContext; // ★ 송신 전용 컨텍스트 추가

    int m_sessionId;
    SOCKET m_socket;
    bool m_inUse;

    static const int BUFFER_SIZE = 65535;
    char m_recvBuffer[BUFFER_SIZE];
    int m_readPos;
    int m_writePos;

    // ★ 비동기 Send Queue 자원들
    std::mutex m_sendLock;
    std::vector<char> m_sendQueue;     // 스레드들이 패킷을 쌓아두는 큐
    std::vector<char> m_sendingBuffer; // 현재 OS가 네트워크 카드로 쏘고 있는 버퍼
    std::atomic<bool> m_isSending;     // 현재 송신 중인지 여부

    float m_posX = 0.0f;
    float m_posY = 0.0f;
};

