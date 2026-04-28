#include "Session.h"
#include "SessionManager.h"

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

void Session::OnReceive(int bytesTransferred, SessionManager* manager) {
    m_writePos += bytesTransferred;

    while (true) {
        int dataSize = m_writePos - m_readPos;
        if (dataSize < sizeof(PacketHeader)) break;

        PacketHeader* header = reinterpret_cast<PacketHeader*>(&m_recvBuffer[m_readPos]);

        // 방어: 크기가 0이거나 버퍼보다 크면 악의적 공격으로 간주하고 쓰레기 데이터 폐기!
        if (header->size <= 0 || header->size > BUFFER_SIZE) {
            m_readPos = m_writePos;
            break;
        }

        if (dataSize < header->size) break;

        // ----------------------------------------------------
        // [서버 라우팅] 수신된 패킷을 뜯어보고 역할 수행!
        // ----------------------------------------------------
        if (header->id == 1) // C2S_MOVE 수신!
        {
            std::cout << "[서버] " << m_sessionId << "번 유저의 이동 패킷 수신! 브로드캐스팅 합니다." << std::endl;

            C2S_MovePacket* movePkt = reinterpret_cast<C2S_MovePacket*>(&m_recvBuffer[m_readPos + sizeof(PacketHeader)]);

            movePkt->sessionId = m_sessionId;
            header->id = 2;

            // ★ 추가: 1만 명에게 뿌리기 전에, 서버의 내 정보에도 이 최신 좌표를 갱신합니다!
            this->SetPosition(movePkt->posX, movePkt->posY);

            manager->Broadcast(&m_recvBuffer[m_readPos], header->size, m_sessionId);
        }

        // 이 한 줄이 없어서 무한루프에 빠졌던 것입니다! 읽은 만큼 커서 전진!
        m_readPos += header->size;
    }

    // 다 처리하고 남은 짜투리 데이터 앞으로 밀기
    int dataSize = m_writePos - m_readPos;
    if (dataSize > 0 && m_readPos > 0) {
        memmove(m_recvBuffer, &m_recvBuffer[m_readPos], dataSize);
    }
    m_writePos = dataSize;
    m_readPos = 0;

    // 버그 방어막: PostRecv 실패 시 소켓 강제 종료
    bool success = PostRecv();
    if (!success) {
        // 클라이언트가 패킷을 보내자마자 랜선을 뽑았다면? 강제로 방을 빼버립니다!
        manager->Release(this);
        ::closesocket(m_socket);
    }
}

bool Session::Send(char* packet, int size) {
    // 빈 방이거나 소켓이 끊겼으면 안 보냄
    if (!m_inUse || m_socket == INVALID_SOCKET) return false;

    // 블로킹 동기 방식으로 일단 쏩니다 (현재 수준에서 가장 안전함)
    int sent = ::send(m_socket, packet, size, 0);
    return sent != SOCKET_ERROR;
}