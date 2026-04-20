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

private:
    std::vector<Session*> m_sessions;
    std::vector<int> m_freeIndices;
    std::mutex m_mutex;
};