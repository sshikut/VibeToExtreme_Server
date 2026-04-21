# 🚀 VibeToExtreme 서버 아키텍처 명세서

### 📄 `main.cpp`
- `NetworkCore` 클래스를 통해 네트워크 서버의 핵심 기능을 관리합니다.
- `InitializeIOCP()` 함수로 I/O 컨텍스트 포인터를 초기화합니다.
- `StartServer(7777)` 함수로 서버를 7777번 포트에서 시작합니다.
- 예외 처리를 위해 try-catch 블록을 사용하여 에러 메시지를 출력합니다.

---

### 📄 `NetworkCore.cpp`
- **네트워크 커어 초기화:** `NetworkCore` 클래스는 네트워크 서버의 핵심 로직을 담당합니다. 생성자에서 Windows 소켓 API를 초기화하고, 소멸자에서는 리소스를 정리합니다.
- **IOCP 설정:** `InitializeIOCP` 함수는 I/O 컬러프로킹(Io Completion Port)을 설정하여 비동기 입출력을 효율적으로 관리합니다.
- **서버 시작:** `StartServer` 메서드는 서버를 시작하고 클라이언트 연결을 수신하는 스레드와 워커 스레드를 생성합니다. 또한, 상태 모니터링 스레드도 추가하여 내부 상태를 추적합니다.
- **네트워크 이벤트 처리:** `AcceptThreadMain`과 `WorkerThreadMain` 메서드는 비동기 네트워크 이벤트를 처리하며, 클라이언트 연결 및 데이터 수신에 대한 로깅과 오류 처리를 담당합니다.

---

### 📄 `Session.cpp`
- `Session` 클래스는 게임 서버와 클라이언트 사이의 세션을 관리합니다.
- 생성자 `Session()` initializes member variables: `m_sessionId`, `m_socket`, and `m_inUse`.
- 소멸자 `~Session()` closes the socket if it is still open.
- `Reset()` 함수는 세션 상태를 초기화하며, `m_inUse`을 `false`로 설정하고, 소켓을 닫습니다.

---

### 📄 `SessionManager.cpp`
- `SessionManager` 클래스는 게임 서버의 세션 관리를 담당하며, 최대 세션 수를 설정할 수 있습니다.
- 생성자에서 세션을 미리 생성하고, 초기화 상태로 유지합니다. 비어있는 세션 인덱스를 저장하는 리스트도 준비합니다.
- `Acquire` 함수는 사용 가능한 세션을 가져옵니다. 만약 모든 세션이 사용 중이라면 nullptr를 반환합니다.
- `Release` 함수는 사용된 세션을 반납하고, 다시 비어있는 세션 목록에 추가합니다. 이미 반납된 상태의 세션은 무시됩니다.

---

### 📄 `NetworkCore.h`
- **NetworkCore**: 게임 서버의 네트워크 핵심을 담당하는 클래스. IOCP를 초기화하고, 서버를 시작하며, 워커 스레드와 수용 스레드를 관리합니다.
  - `InitializeIOCP`: IOCP를 초기화하는 함수.
  - `StartServer(uint16_t port)`: 지정된 포트에서 서버를 시작하는 함수.
  - `WorkerThreadMain`: 클라이언트 연결을 처리하는 워커 스레드의 메인 함수.
  - `AcceptThreadMain`: 클라이언트 연결 수용을 처리하는 수용 스레드의 메인 함수.

---

### 📄 `Session.h`
- **Session.h**: C++ 게임 서버에서 클라이언트 세션을 관리하는 헤더 파일입니다.
- **PacketType**: 데이터 패킷의 유형을 정의한 열거체입니다. 특히 `CRASH_BOMB`는 특별히 지정되어 있습니다.
- **Session 클래스**:
  - **Reset()**: 세션을 초기화합니다.
  - **SetSessionId()와 GetSessionId()**: 클라이언트의 세션 ID를 설정하고 가져옵니다.
  - **GetSocket()과 SetSocket()**: 소켓을 가져오고 설정하며, 사용 중 상태를 관리합니다.
  - **OnReceive(int bytesTransferred)**: 수신된 데이터를 처리하는 함수입니다.
  - **PostRecv()**: 비동기로 데이터를 받는 작업을 시작하고, 오버플로우 방지를 위해 매번 초기화합니다.

---

### 📄 `SessionManager.h`
- `SessionManager` 클래스는 게임 서버의 세션 관리를 담당합니다.
- 생성자는 최대 세션 수를 받아 초기화하고, 소멸자에서는 모든 세션을 해제합니다.
- `Acquire()` 함수는 사용 가능한 세션을 가져오고, `Release()` 함수는 사용한 세션을 반납합니다.
- `GetAvailableSessionCount()` 함수는 현재 남은 비어있는 세션 수를 안전하게 반환합니다.

---

