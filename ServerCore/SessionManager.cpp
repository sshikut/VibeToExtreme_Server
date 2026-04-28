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