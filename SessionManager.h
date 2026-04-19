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

private:
    std::vector<Session*> m_sessions;
    std::vector<int> m_freeIndices;
    std::mutex m_mutex;
};