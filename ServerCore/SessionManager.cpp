#include "SessionManager.h"

SessionManager::SessionManager(size_t maxSession) {
    m_sessions.reserve(maxSession);
    m_freeIndices.reserve(maxSession);

    for (int i = 0; i < maxSession; ++i) {
        Session* newSession = new Session();
        newSession->SetSessionId(i);

        m_sessions.push_back(newSession);
        m_freeIndices.push_back(i);
    }
}

SessionManager::~SessionManager() {
    for (Session* session : m_sessions) {
        delete session;
    }
}

Session* SessionManager::Acquire() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_freeIndices.empty()) {
        int freeIndex = m_freeIndices.back();
        m_freeIndices.pop_back();

        Session* session = m_sessions[freeIndex];
        session->Reset();
        return session;
    }
    return nullptr;
}

void SessionManager::Release(Session* session) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // 이미 누군가가 반납해서 빈방(Free)이 된 상태라면, 두 번 반납하지 않고 무시합니다.
    if (session->IsFree()) {
        return;
    }

    session->Reset();
    int returnedIndex = session->GetSessionId();
    m_freeIndices.push_back(returnedIndex);
}

void SessionManager::Broadcast(char* packet, int size, int excludeSessionId) {
    std::vector<Session*> targets;
    targets.reserve(10000);

    // 1. 락을 걸고 빠르게 접속 중인 유저 명단만 복사합니다.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (Session* session : m_sessions) {
            // 빈 방이 아니고, 제외할 대상(나)이 아니면 타겟 리스트에 추가!
            if (!session->IsFree() && session->GetSessionId() != excludeSessionId) {
                targets.push_back(session);
            }
        }
    }

    // 2. 락이 풀린 안전한 상태에서 명단을 돌며 패킷을 쏩니다.
    for (Session* target : targets) {
        target->Send(packet, size);
    }
}

void SessionManager::SyncExistingSessions(Session* newSession) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (Session* existingSession : m_sessions) {
        if (!existingSession->IsFree() && existingSession->GetSessionId() != newSession->GetSessionId()) {

            S2C_SpawnPacket spawnPkt;
            spawnPkt.size = sizeof(S2C_SpawnPacket);
            spawnPkt.id = 4;
            spawnPkt.sessionId = existingSession->GetSessionId();

            // ★ 수정: 하드코딩 0.0f를 버리고 서버가 기억하는 진짜 최신 좌표를 넣습니다!
            spawnPkt.spawnX = existingSession->GetPosX();
            spawnPkt.spawnY = existingSession->GetPosY();

            newSession->Send((char*)&spawnPkt, sizeof(spawnPkt));
        }
    }
}

void SessionManager::UpdateSessionSector(Session* session, float newX, float newY) {
    auto [oldX, oldY] = GetSectorIndex(session->GetPosX(), session->GetPosY());
    auto [newGridX, newGridY] = GetSectorIndex(newX, newY);

    if (oldX == newGridX && oldY == newGridY) {
        session->SetPosition(newX, newY);
        return;
    }

    Sector& oldSector = m_grid[oldX][oldY];
    Sector& newSector = m_grid[newGridX][newGridY];

    // 읽기/쓰기 락(SRWLOCK) 중 강력한 '쓰기 락'을 획득합니다. (데드락 방지)
    std::scoped_lock writeLock(oldSector.srwLock, newSector.srwLock);

    // 구시대적인 oldList.erase()는 버리고 압도적으로 빠른 포인터 조작을 씁니다.
    RemoveSessionFromSector(oldSector, session);
    session->SetPosition(newX, newY);
    AddSessionToSector(newSector, session);
}

void SessionManager::BroadcastToSector(int gridX, int gridY, char* packet, int size, int excludeSessionId) {
    Sector& targetSector = m_grid[gridX][gridY];

    // 누군가 이동(Write) 중이 아니라면, 1만 개의 스레드가 락을 무시하고 동시에 읽을 수 있습니다!
    std::shared_lock<std::shared_mutex> readLock(targetSector.srwLock);

    Session* current = targetSector.head;
    while (current != nullptr) {
        if (current->GetSessionId() != excludeSessionId) {
            current->Send(packet, size);
        }
        current = current->nextSectorNode;
    }
}

void SessionManager::AddSessionToSector(Sector& sector, Session* session) {
    session->nextSectorNode = nullptr;
    session->prevSectorNode = sector.tail;

    if (sector.tail != nullptr) {
        sector.tail->nextSectorNode = session;
    }
    else {
        sector.head = session; // 방에 아무도 없었다면 내가 head
    }
    sector.tail = session;
}

// ★ $O(1)$ 삭제: 내 앞사람과 뒷사람의 손을 서로 이어주고 나는 빠집니다. (메모리 복사 0%)
void SessionManager::RemoveSessionFromSector(Sector& sector, Session* session) {
    if (session->prevSectorNode != nullptr) {
        session->prevSectorNode->nextSectorNode = session->nextSectorNode;
    }
    else {
        sector.head = session->nextSectorNode;
    }

    if (session->nextSectorNode != nullptr) {
        session->nextSectorNode->prevSectorNode = session->prevSectorNode;
    }
    else {
        sector.tail = session->prevSectorNode;
    }

    // 내 손은 깨끗하게 씻습니다.
    session->prevSectorNode = nullptr;
    session->nextSectorNode = nullptr;
}

// 유저가 강제 종료되거나 맵을 나갈 때 호출하는 안전한 퇴장 함수
void SessionManager::RemoveSessionFromItsSector(Session* session) {
    auto [gridX, gridY] = GetSectorIndex(session->GetPosX(), session->GetPosY());
    Sector& mySector = m_grid[gridX][gridY];

    // 격자의 쓰기 락을 걸고 안전하게 명단에서 삭제합니다.
    std::scoped_lock writeLock(mySector.srwLock);

    // 아까 만들어둔 O(1) 삭제 헬퍼 함수 재활용!
    RemoveSessionFromSector(mySector, session);
}