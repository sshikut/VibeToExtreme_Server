#include "Session.h"
#include "SessionManager.h"

Session::Session() : m_sessionId(-1), m_socket(INVALID_SOCKET), m_inUse(false), m_isSending(false) {}

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

    // ★ 빈 방이 될 때 Send 큐도 깔끔하게 비웁니다.
    std::lock_guard<std::mutex> lock(m_sendLock);
    m_sendQueue.clear();
    m_sendingBuffer.clear();
    m_isSending = false;
}

bool Session::PostRecv() {
    DWORD recvBytes = 0;
    DWORD flags = 0;

    ZeroMemory(&m_recvContext.overlapped, sizeof(m_recvContext.overlapped));
    m_recvContext.type = IOType::RECV; // ★ "나는 수신용 편지봉투다!" 명시

    m_recvContext.wsaBuf.buf = &m_recvBuffer[m_writePos];
    m_recvContext.wsaBuf.len = BUFFER_SIZE - m_writePos;

    if (::WSARecv(m_socket, &m_recvContext.wsaBuf, 1, &recvBytes, &flags, &m_recvContext.overlapped, nullptr) == SOCKET_ERROR) {
        if (::WSAGetLastError() != WSA_IO_PENDING) {
            std::cout << "[ERROR] WSARecv failed. Error Code: " << ::WSAGetLastError() << std::endl;
            return false;
        }
    }
    return true;
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
            C2S_MovePacket* movePkt = reinterpret_cast<C2S_MovePacket*>(&m_recvBuffer[m_readPos + sizeof(PacketHeader)]);

            if (movePkt->sessionId != -999)
            {
                std::cout << "[INFO] User " << m_sessionId
                    << " moved to (" << movePkt->posX << ", " << movePkt->posY << ")" << std::endl;
            }

            movePkt->sessionId = m_sessionId;
            header->id = 2;

            // ★ 1. 유저의 좌표를 갱신하면서 동시에 Sector(격자) 이중 연결 리스트도 O(1)로 이동시킵니다!
            manager->UpdateSessionSector(this, movePkt->posX, movePkt->posY);

            // ★ 2. 1만 명 전체가 아니라, 내가 속한 해당 '격자'에만 패킷을 쏩니다! (CPU 오버헤드 99% 감소)
            auto [gridX, gridY] = manager->GetSectorIndex(movePkt->posX, movePkt->posY);
            manager->BroadcastToSurroundingSectors(gridX, gridY, &m_recvBuffer[m_readPos], header->size, m_sessionId);
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
    if (!m_inUse || m_socket == INVALID_SOCKET) return false;

    bool expected = false;
    {
        // 1. 패킷을 무작정 쏘지 않고 내 큐(Queue)에 차곡차곡 쌓아둡니다.
        std::lock_guard<std::mutex> lock(m_sendLock);
        m_sendQueue.insert(m_sendQueue.end(), packet, packet + size);
    }

    // 2. 만약 지금 아무도 송신을 하고 있지 않다면, 내가 총대를 메고 송신을 시작(PostSend)합니다!
    if (m_isSending.compare_exchange_strong(expected, true)) {
        PostSend();
    }
    return true;
}

void Session::PostSend() {
    // 큐에 쌓인 모든 패킷을 '실제 송신 버퍼'로 한 번에 옮깁니다. (Gather I/O)
    {
        std::lock_guard<std::mutex> lock(m_sendLock);
        m_sendingBuffer = std::move(m_sendQueue);
        m_sendQueue.clear();
    }

    ZeroMemory(&m_sendContext.overlapped, sizeof(m_sendContext.overlapped));
    m_sendContext.type = IOType::SEND; // ★ "나는 송신용 편지봉투다!" 명시

    m_sendContext.wsaBuf.buf = m_sendingBuffer.data();
    m_sendContext.wsaBuf.len = static_cast<ULONG>(m_sendingBuffer.size());

    DWORD sendBytes = 0;

    // OS에게 "이 커다란 덩어리 한 번에 쏴줘!" 라고 비동기로 위임합니다.
    if (::WSASend(m_socket, &m_sendContext.wsaBuf, 1, &sendBytes, 0, &m_sendContext.overlapped, nullptr) == SOCKET_ERROR) {
        if (::WSAGetLastError() != WSA_IO_PENDING) {
            // 에러가 나면 락을 풀고 송신 상태를 해제합니다.
            m_isSending = false;
        }
    }
}

void Session::OnSendCompleted(int bytesTransferred) {
    // 1. 수동으로 자물쇠를 잠급니다. (lock_guard는 블록 끝까지 안 풀리므로 직접 제어)
    m_sendLock.lock();

    // 2. 큐에 보낼 패킷이 더 남아있는지 확인합니다.
    if (!m_sendQueue.empty()) {

        // ★ 핵심: PostSend() 내부에서 다시 m_sendLock을 요구하므로, 
        // 부르기 직전에 반드시 내가 쥐고 있던 자물쇠를 풀어줘야 데드락(Crash)이 발생하지 않습니다!
        m_sendLock.unlock();

        // 이제 안전하게 다음 송신을 위임합니다.
        PostSend();

    }
    else {
        // 더 이상 보낼 게 없다면 스위치를 끄고 자물쇠를 풉니다.
        m_isSending = false;
        m_sendLock.unlock();
    }
}