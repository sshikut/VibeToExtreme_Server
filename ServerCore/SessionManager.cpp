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
    auto [gridX, gridY] = GetSectorIndex(newSession->GetPosX(), newSession->GetPosY());
    Sector& mySector = m_grid[gridX][gridY];

    // 내가 속한 격자의 락을 걸고, 같은 격자 사람들의 위치만 뉴비에게 알려줍니다!
    std::shared_lock<std::shared_mutex> readLock(mySector.srwLock);

    Session* current = mySector.head;
    while (current != nullptr) {
        if (current->GetSessionId() != newSession->GetSessionId()) {
            S2C_SpawnPacket spawnPkt;
            spawnPkt.size = sizeof(S2C_SpawnPacket);
            spawnPkt.id = 4;
            spawnPkt.sessionId = current->GetSessionId();
            spawnPkt.spawnX = current->GetPosX();
            spawnPkt.spawnY = current->GetPosY();

            newSession->Send((char*)&spawnPkt, sizeof(spawnPkt));
        }
        current = current->nextSectorNode;
    }
}

void SessionManager::UpdateSessionSector(Session* session, float newX, float newY) {
    // ★ 1. 클램핑 되기 전의 순수한 과거 좌표를 확인하여 첫 소환(뉴비)인지 판별합니다.
    bool isFirstSpawn = (session->GetPosX() <= -9000.0f);

    auto [oldX, oldY] = GetSectorIndex(session->GetPosX(), session->GetPosY());
    auto [newGridX, newGridY] = GetSectorIndex(newX, newY);

    // 첫 소환이 아니고, 격자가 변하지 않았다면 일찌감치 리턴!
    if (!isFirstSpawn && oldX == newGridX && oldY == newGridY) {
        session->SetPosition(newX, newY);
        return;
    }

    Sector& oldSector = m_grid[oldX][oldY];
    Sector& newSector = m_grid[newGridX][newGridY];

    // ★ 2. 데드락 완벽 방어: 같은 자물쇠(Mutex)를 두 번 잠그면 영원히 멈춥니다!
    {
        if (oldX == newGridX && oldY == newGridY) {
            // 첫 스폰인데 우연히 0번 격자에 떨어진 경우 (자물쇠 1개만 필요)
            std::scoped_lock writeLock(newSector.srwLock);
            session->SetPosition(newX, newY);
            AddSessionToSector(newSector, session);
        }
        else {
            // 격자를 넘어가는 정상 이동의 경우 (자물쇠 2개 필요)
            std::scoped_lock writeLock(oldSector.srwLock, newSector.srwLock);
            if (!isFirstSpawn) {
                RemoveSessionFromSector(oldSector, session);
            }
            session->SetPosition(newX, newY);
            AddSessionToSector(newSector, session);
        }
    }

    // =========================================================================
    // 2. ★ 상용 MMORPG 코어: AOI 차집합(Set Difference) 동기화 로직
    // =========================================================================

    auto IsInAOI = [](int targetX, int targetY, int centerX, int centerY) {
        return std::abs(targetX - centerX) <= 1 && std::abs(targetY - centerY) <= 1;
        };

    // 윈도우 min/max 매크로의 악랄한 공격을 괄호 () 로 회피합니다.
    int minX = (std::max)(0, (std::min)(oldX, newGridX) - 1);
    int maxX = (std::min)(MAX_SECTOR_X - 1, (std::max)(oldX, newGridX) + 1);
    int minY = (std::max)(0, (std::min)(oldY, newGridY) - 1);
    int maxY = (std::min)(MAX_SECTOR_Y - 1, (std::max)(oldY, newGridY) + 1);

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            bool wasInAOI = IsInAOI(x, y, oldX, oldY);
            bool isInAOI = IsInAOI(x, y, newGridX, newGridY);

            // 계속 시야에 있거나, 아예 시야 밖인 격자는 무시합니다.
            if ((wasInAOI && isInAOI) || (!wasInAOI && !isInAOI)) continue;

            Sector& targetSector = m_grid[x][y];

            // 아까 위에서 쓰기 락을 풀었기 때문에, 이제 데드락 없이 안전하게 읽기 락을 걸 수 있습니다!
            std::shared_lock<std::shared_mutex> readLock(targetSector.srwLock);
            Session* current = targetSector.head;

            if (wasInAOI && !isInAOI)
            {
                // [시야 이탈] 차집합 1
                S2C_LeavePacket leaveMe;
                leaveMe.size = sizeof(S2C_LeavePacket);
                leaveMe.id = 3;
                leaveMe.sessionId = session->GetSessionId();

                while (current != nullptr) {
                    if (current->GetSessionId() != session->GetSessionId()) {
                        current->Send((char*)&leaveMe, sizeof(leaveMe)); // 상대방에게 나를 지우라고 통보

                        S2C_LeavePacket leaveOther;
                        leaveOther.size = sizeof(S2C_LeavePacket);
                        leaveOther.id = 3;
                        leaveOther.sessionId = current->GetSessionId();
                        session->Send((char*)&leaveOther, sizeof(leaveOther)); // 나에게 상대방을 지우라고 통보
                    }
                    current = current->nextSectorNode;
                }
            }
            else if (!wasInAOI && isInAOI)
            {
                // [시야 진입] 차집합 2
                S2C_SpawnPacket spawnMe;
                spawnMe.size = sizeof(S2C_SpawnPacket);
                spawnMe.id = 4;
                spawnMe.sessionId = session->GetSessionId();
                spawnMe.spawnX = session->GetPosX(); // 최신 좌표 송신
                spawnMe.spawnY = session->GetPosY();

                while (current != nullptr) {
                    if (current->GetSessionId() != session->GetSessionId()) {
                        current->Send((char*)&spawnMe, sizeof(spawnMe)); // 상대방에게 나를 생성하라고 통보

                        S2C_SpawnPacket spawnOther;
                        spawnOther.size = sizeof(S2C_SpawnPacket);
                        spawnOther.id = 4;
                        spawnOther.sessionId = current->GetSessionId();
                        spawnOther.spawnX = current->GetPosX();
                        spawnOther.spawnY = current->GetPosY();
                        session->Send((char*)&spawnOther, sizeof(spawnOther)); // 나에게 상대방을 생성하라고 통보
                    }
                    current = current->nextSectorNode;
                }
            }
        }
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

void SessionManager::BroadcastToSurroundingSectors(int centerGridX, int centerGridY, char* packet, int size, int excludeSessionId) {

    // 나를 중심으로 X, Y를 -1, 0, +1 씩 탐색하여 총 9개의 인접 격자를 순회합니다.
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            int targetX = centerGridX + dx;
            int targetY = centerGridY + dy;

            // ★ 핵심 방어막: 맵의 가장자리(Edge)에 있어서 음수(-1)나 최대치를 벗어나는 격자는 무시!
            if (targetX < 0 || targetX >= MAX_SECTOR_X || targetY < 0 || targetY >= MAX_SECTOR_Y) {
                continue;
            }

            Sector& targetSector = m_grid[targetX][targetY];

            // 각 격자별로 안전하게 읽기 락(Shared Lock)을 걸고 패킷 전송
            std::shared_lock<std::shared_mutex> readLock(targetSector.srwLock);

            Session* current = targetSector.head;
            while (current != nullptr) {
                if (current->GetSessionId() != excludeSessionId) {
                    current->Send(packet, size);
                }
                current = current->nextSectorNode;
            }
        }
    }
}