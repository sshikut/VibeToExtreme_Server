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

    session->Reset();
    int returnedIndex = session->GetSessionId();
    m_freeIndices.push_back(returnedIndex);
}