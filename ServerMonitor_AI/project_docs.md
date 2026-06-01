# 🚀 VibeToExtreme 서버 아키텍처 명세서

### 📄 `main.cpp`
- `main.cpp`는 게임 서버의 진입점입니다.
- `NetworkCore` 클래스를 사용하여 네트워크 서버를 초기화하고 시작합니다.
- IOCP(I/O Completion Port)를 사용하여 비동기 통신을 구현합니다.
- 예외 처리를 통해 오류 발생 시 메시지를 출력합니다.

---

### 📄 `NetworkCore.cpp`
- **NetworkCore.cpp**: C++ 게임 서버의 네트워크 코어를 관리하는 클래스입니다. 메인 역할은 Windows 소켓과 I/O 컐링 포트(IoCP)를 사용해 비동기적 통신을 처리합니다.

- **GetWindowsErrorMessage**: OS의 에러 코드를 사람이 읽을 수 있는 문자열로 번역하는 함수입니다.

- **NetworkCore::InitializeIOCP**: IoCP 핸들을 생성하고 초기화하는 메서드입니다. 이는 비동기 I/O 작업을 효율적으로 처리하는 데 중요합니다.

- **NetworkCore::StartServer**: 서버를 시작하는 메서드로, 소켓을 생성하고 바인딩하며, 세션 매니저를 초기화하고 워커 스레드와 수용 스레드를 생성합니다. 이 메서드는 비동기적으로 클라이언트 연결을 처리하도록 설계되어 있습니다.

- **NetworkCore::AcceptThreadMain**: 클라이언트 연결을 수락하는 스레드입니다. 새 클라이언트가 접속하면 IOCP에 추가되고, 세션 매니저에서 세션을 할당하여 통신이 시작됩니다.

- **NetworkCore::WorkerThreadMain**: 비동기 I/O 작업의 결과를 처리하는 워커 스레드입니다. 클라이언트로부터 데이터를 수신하고 처리한 후, 다시 다음 편지를 대기열에 넣습니다.

---

### 📄 `Session.cpp`
- **클래스 및 함수 요약**:
  - `Session`: 게임 서버에서 클라이언트 세션을 관리하는 클래스입니다.
  - `Session()` (생성자): 새로운 세션 객체를 생성하고 초기화합니다. 세션 ID는 `-1`, 소켓은 `INVALID_SOCKET`, 사용 중 상태는 `false`로 설정됩니다.
  - `~Session()` (소멸자): 소멸 시 소켓이 유효하면 소켓을 닫습니다.
  - `Reset()`: 세션을 초기화합니다. 사용 중 상태를 `false`로, 소켓을 `INVALID_SOCKET`으로 설정합니다.

---

### 📄 `SessionManager.cpp`
```markdown
- **SessionManager 클래스**: 세션 관리 메커니즘을 제공, 최대 세션 수를 제한하고 세션의 생성과 해제를 관리합니다.
- **생성자 (`SessionManager::SessionManager`)**: 지정된 최대 세션 수만큼 세션 객체를 생성하여 세션 리스트와 비어있는 인덱스 리스트에 저장합니다.
- **소멸자 (`SessionManager::~SessionManager`)**: 모든 세션 객체를 해제하여 메모리 누수를 방지합니다.
- **세션 할당 (`SessionManager::Acquire`)**: 비어있는 세션을 반환하며, 해당 세션이 이미 사용 중이라면 `nullptr`을 반환합니다.
- **세션 반납 (`SessionManager::Release`)**: 사용한 세션을 비워두고, 비어있는 인덱스 리스트에 추가하여 재사용할 수 있도록 합니다.
```

---

### 📄 `NetworkCore.h`
```markdown
- `NetworkCore` 클래스는 네트워크 서버의 핵심 역할을 담당합니다.
- `InitializeIOCP` 함수: I/O 쓰일기(I/O Completion Port)를 초기화합니다.
- `StartServer` 함수: 서버를 시작하고 지정된 포트에 바인딩합니다.
- `WorkerThreadMain` 및 `AcceptThreadMain` 함수: 각각 worker 스레드와 accept 스레드의 메인 로직을 실행합니다.
```

---

### 📄 `Session.h`
- **Session.h** 파일은 C++에서 게임 서버의 세션 관리를 위한 헤더 파일입니다.
- `Session` 클래스는 각 클라이언트 연결에 대한 세션 정보를 관리하며, 비동기 통신을 위해 오버랩된 I/O를 사용합니다.
- `PostRecv` 함수는 데이터 수신을 비동기적으로 시작하고, 오버랩된 컨텍스트를 초기화하여 다음 수신 준비합니다.

---

### 📄 `SessionManager.h`
- **핵심 역할**: `SessionManager`는 게임 서버에서 세션 관리를 담당하며, 새로운 세션을 생성하고 사용 중인 세션을 해제하는 역할을 합니다.

- **주요 클래스/함수**:
  - `Acquire()`: 사용 가능한 세션을 가져옵니다.
  - `Release(Session* session)`: 사용한 세션을 해제합니다.
  - `GetAvailableSessionCount()`: 현재 남은 빈 방의 수를 안전하게 가져옵니다.

---

