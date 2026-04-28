#pragma once
#include "Session.h"
#include <vector>
#include <mutex>

class SessionManager {
public:
    explicit SessionManager(size_t maxSession);
    ~SessionManager();

    Session* Acquire();
    void Release(Session* session);

    // 현재 남은 빈방의 갯수를 안전하게(Lock) 가져옵니다.
    size_t GetAvailableSessionCount() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_freeIndices.size();
    }

    // 맵에 있는 모두에게 패킷을 뿌립니다. (excludeSessionId: 나 자신은 제외)
    void Broadcast(char* packet, int size, int excludeSessionId = -1);

    // 새로 접속한 뉴비에게 기존 유저들의 목록을 쏴주는 함수
    void SyncExistingSessions(Session* newSession);

private:
    std::vector<Session*> m_sessions;
    std::vector<int> m_freeIndices;
    std::mutex m_mutex;
};