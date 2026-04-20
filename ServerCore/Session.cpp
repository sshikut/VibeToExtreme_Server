#include "Session.h"

Session::Session() : m_sessionId(-1), m_socket(INVALID_SOCKET), m_inUse(false) {}

Session::~Session() {
    if (m_socket != INVALID_SOCKET) {
        ::closesocket(m_socket);
    }
}

void Session::Reset() {
    m_inUse = false;
    m_socket = INVALID_SOCKET;
}