import socket
import threading
import time

SERVER_IP = '127.0.0.1'
SERVER_PORT = 7777 # 작성자님의 C++ 서버 포트 번호로 맞추세요!
BOT_COUNT = 1000    # 한 번에 투입할 가짜 AI 봇(클라이언트) 수

def ai_bot_behavior(bot_id):
    try:
        # 1. 서버에 무작정 연결
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect((SERVER_IP, SERVER_PORT))
        print(f"🤖 [Bot {bot_id}] 침투 성공!")
        
        # 2. 아무 의미 없는 가짜 패킷(쓰레기 데이터)을 마구 던짐
        for _ in range(10):
            client.sendall(b"Hello Server! I am an AI Bot testing your limits!\n")
            time.sleep(0.01)
            
        # 3. 비정상적으로 연결 뚝 끊고 도망감 (서버가 버티는지 테스트!)
        client.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, b'\x01\x00\x00\x00\x00\x00\x00\x00')
        client.close()
        
    except Exception as e:
        print(f"❌ [Bot {bot_id}] 접속 실패: {e}")

print(f"🔥 AI 부하 테스트 봇 {BOT_COUNT}기 출격 준비 완료!")
threads = []

# 멀티스레드로 봇 500개를 동시에 발사!
for i in range(BOT_COUNT):
    t = threading.Thread(target=ai_bot_behavior, args=(i,))
    threads.append(t)
    t.start()

for t in threads:
    t.join()

print("🏁 모든 봇의 공격이 종료되었습니다.")