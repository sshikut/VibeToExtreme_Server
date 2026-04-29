# 🚀 VibeToExtreme_Server
**C++ IOCP 기반 고성능 비동기 게임 서버 엔진**

본 리포지토리는 상용 MMORPG 급의 대규모 동시 접속자(10,000+ CCU)를 수용하기 위해 Windows 커널 레벨의 비동기 I/O 모델인 **IOCP(Input/Output Completion Port)**를 활용하여 설계된 고성능 서버 코어입니다.

## 🛠 Tech Stack
- **Language:** C++17 (ISO Standard)
- **Networking:** Windows IOCP (Non-Blocking I/O)
- **Concurrency:** Multi-threaded Worker Pool (N*2 Threads)
- **Memory Management:** Custom Session Pooling

## ✨ Key Features
- [cite_start]**IOCP Core:** `GetQueuedCompletionStatus`를 통한 효율적인 패킷 처리 및 컨텍스트 스위칭 최소화 [cite: 91-93].
- [cite_start]**10,000 Session Pooling:** 런타임 중 `new/delete` 부하를 제거하기 위해 서버 기동 시 10,000개의 세션 객체를 사전 할당[cite: 147].
- [cite_start]**Shared-Lock Concurrency:** 브로드캐스팅 시의 병목을 해결하기 위해 `std::shared_mutex` 기반의 읽기-쓰기 락 아키텍처 적용[cite: 94].
- [cite_start]**Memory Alignment:** `#pragma pack(1)` 지시어를 통한 이기종 언어(C#) 간 메모리 오프셋 정렬 최적화[cite: 95, 129].
- [cite_start]**Python Monitor:** 서버 로그 및 시스템 상태를 실시간으로 모니터링하는 통합 대시보드 연동[cite: 126, 187].

## 📈 Performance Goals
- **CCU:** 10,000명 이상의 동시 접속 유지 및 패킷 처리.
- [cite_start]**Resource Limit:** 8GB RAM 환경에서의 안정적인 구동 및 메모리 누수 제로 지향[cite: 87].

## 📂 Related Repository
- [VibeToExtreme_Client (Unity)](github.com/sshikut/VibeToExtreme_Client)
