#pragma once
#include "Session.h"
#include <vector>
#include <mutex>
#include <algorithm>
#include <shared_mutex>

constexpr int SECTOR_SIZE = 10;   // 격자 1개의 크기 (예: 10 Unit)
constexpr int MAX_SECTOR_X = 100; // X축 격자 개수
constexpr int MAX_SECTOR_Y = 100; // Y축 격자 개수

// 독립된 공간(Sector) 구조체
struct Sector {
    // 일반 mutex가 아닌 다중 읽기 허용 자물쇠!
    std::shared_mutex srwLock;

    // Intrusive List Head/Tail (vector 절대 사용 금지)
    Session* head = nullptr;
    Session* tail = nullptr;
};

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

    std::pair<int, int> GetSectorIndex(float posX, float posY) {
        int gridX = static_cast<int>(posX) / SECTOR_SIZE;
        int gridY = static_cast<int>(posY) / SECTOR_SIZE;

        // 맵 밖으로 뚫고 나가는 악의적 좌표 방어 (클램핑)
        gridX = std::clamp(gridX, 0, MAX_SECTOR_X - 1);
        gridY = std::clamp(gridY, 0, MAX_SECTOR_Y - 1);
        return { gridX, gridY };
    }

    void BroadcastToSector(int gridX, int gridY, char* packet, int size, int excludeSessionId = -1);

    void UpdateSessionSector(Session* session, float newX, float newY);

    void RemoveSessionFromItsSector(Session* session);

private:
    std::vector<Session*> m_sessions;
    std::vector<int> m_freeIndices;
    std::mutex m_mutex;

    Sector m_grid[MAX_SECTOR_X][MAX_SECTOR_Y];

    void AddSessionToSector(Sector& sector, Session* session);
    void RemoveSessionFromSector(Sector& sector, Session* session);
};