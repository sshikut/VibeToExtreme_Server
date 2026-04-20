# 🚀 VibeToExtreme 서버 아키텍처 명세서

### 📄 `main.cpp`
```markdown
- `main.cpp`는 게임 서버의 진입점입니다.
- `NetworkCore` 클래스를 사용해 네트워크 인프라를 초기화하고 서버를 시작합니다.
- IOCP(입출력 완료 포인트)를 설정해 비동기 통신을 구현합니다.
- 에러 발생 시 예외 처리로 문제를 잡아냅니다.
```

---

### 📄 `NetworkCore.cpp`
- **NetworkCore.cpp**: C++로编작된 네트워크 서버의 핵심 컴포넌트로, 비동기 I/O를 위한 IOCP를 사용하여 클라이언트와의 통신을 관리합니다.

- **InitializeIOCP()**: IOCP 객체 생성 및 초기화를 담당하는 함수입니다. 이는 서버가 클라이언트 연결을 효율적으로 처리할 수 있도록 하는 중요합니다.

- **StartServer(uint16_t port)**: 서버를 시작하는 메인 함수로, 포트 설정, 소켓 생성, Bind, Listen 등 초기화 작업을 수행하고, Accept 스레드와 워커 스레드들을 구동시킵니다. 이 함수는 서버의 전체적인 작동을 제어합니다.

- **AcceptThreadMain()**: 클라이언트의 연결 요청을 받아들이고, 세션 매니저에서 세션을 할당하여 비동기 통신을 설정하는 역할을 합니다. 이를 통해 동시에 여러 클라이언트와의 연결을 관리할 수 있습니다.

- **WorkerThreadMain()**: IOCP로부터 전달된 이벤트를 처리하는 워커 스레드의 메인 함수입니다. 여기서는 클라이언트로부터 데이터를 받고, 패킷 타입을 확인하여 적절한 처리를 수행합니다. 이 함수는 서버의 성능과 효율성을 향상시키는데 핵심적입니다.

---

### 📄 `Session.cpp`
- **생성자 (`Session()`)**: 세션을 초기화하며, `m_sessionId`, `m_socket`, `m_inUse` 변수를 설정합니다.
  
- **소멸자 (`~Session()`)**: 사용 중인 소켓이 있다면 닫아서 메모리를 해제합니다.

- **리셋 함수 (`Reset()`)**: 세션을 초기 상태로 되돌립니다. `m_inUse`을 `false`로 설정하고, 소켓을 무효하게 만듭니다.

---

### 📄 `SessionManager.cpp`
- `SessionManager` 클래스는 게임 서버의 세션 관리를 담당하며, 최대 세션 수를 설정할 수 있습니다.
- 생성자에서 세션들을 미리 생성하고, 사용 가능한 인덱스 목록을 초기화합니다.
- 소멸자는 모든 세션 객체를 해제합니다.
- `Acquire` 함수는 비어 있는 세션을 가져오며, 없으면 `nullptr`을 반환합니다.
- `Release` 함수는 사용한 세션을 다시 반납하고, 해당 세션의 인덱스를 재사용할 수 있게 합니다.

---

### 📄 `NetworkCore.h`
```markdown
- **NetworkCore.h**: 게임 서버의 네트워크 핵심 컴포넌트를 정의한 헤더 파일입니다.
- **클래스 `NetworkCore`**:
  - **함수**:
    - `InitializeIOCP()`: I/O 완료 포인트 (IOCP)를 초기화합니다.
    - `StartServer(uint16_t port)`: 서버를 지정된 포트에서 시작합니다.
  - **변수**:
    - `m_iocpHandle`: IOCP 핸들입니다.
    - `m_listenSocket`: 연결 대기 소켓입니다.
    - `m_running`: 서버가 실행 중인지 여부를 나타내는 플래그입니다.
    - `m_sessionManager`: 세션 관리자를 위한 포인터입니다.
    - `m_workerThreads`: 워커 스레드 벡터입니다.
    - `m_acceptThread`: 연결 수락을 담당하는 스레드입니다.
```

---

### 📄 `Session.h`
- **Session 클래스**: 클라이언트 세션을 관리하며, 비동기 수신을 위해 `WSARecv` 함수를 사용합니다.
- **OverlappedContext 구조체**: `WSAOVERLAPPED`, `WSABUF`, 그리고 버퍼를 포함하여 비동기 I/O 작업에 필요한 정보를 저장합니다.
- **OnReceive 함수**: 수신한 데이터를 처리하는 메서드입니다.
- **PostRecv 함수**: 비동기 수신을 요청하고, 오버라이드된 컨텍스트 초기화를 합니다.

---

### 📄 `SessionManager.h`
- **Core Role:** `SessionManager`는 게임 서버에서 세션 관리를 담당하는 클래스입니다. 클라이언트의 연결을 모니터링하고, 필요한 경우 새로운 세션을 생성하거나 기존의 세션을 해제합니다.

- **Important Classes/Functions:**
  - `explicit SessionManager(size_t maxSession);`: 최대 세션 수를 설정하여 인스턴스를 초기화합니다.
  - `~SessionManager();`: 할당된 모든 리소스를 정리하고 해제합니다.
  - `Session* Acquire();`: 사용 가능한 세션을 가져옵니다. 만약 없다면 새로운 세션을 생성하거나 기다립니다.
  - `void Release(Session* session);`: 사용이 끝난 세션을 해제합니다.

---

