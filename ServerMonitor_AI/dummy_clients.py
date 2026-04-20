import socket
import threading
import time

SERVER_IP = '127.0.0.1'
SERVER_PORT = 7777 
CCU_COUNT = 2000  # 헤네시스에 잠수 태울 유저 수 2,000명!

def idle_bot_behavior(bot_id):
    try:
        # 1. 서버에 접속
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect((SERVER_IP, SERVER_PORT))
        
        # 2. 메이플스토리 유저처럼 접속을 끊지 않고 무한 대기!
        while True:
            # 서버가 "응답 없는 유저"로 판단해서 자르지 않도록 
            # 5초마다 의미 없는 생존 신고(Keep-Alive) 패킷을 보냅니다.
            client.sendall(b"Ping\n")
            time.sleep(5) 
            
    except Exception as e:
        # 서버가 닫히거나 연결이 끊기면 스레드 조용히 종료
        pass

print(f"🧍‍♂️ 마을 잠수 유저 {CCU_COUNT}명 접속을 시작합니다...")
threads = []

# 멀티스레드로 2000명을 순차적으로 접속시킵니다.
for i in range(CCU_COUNT):
    t = threading.Thread(target=idle_bot_behavior, args=(i,))
    threads.append(t)
    t.start()
    
    # 2000명이 0.001초 만에 들이닥치면 OS 포트가 고갈될 수 있으므로
    # 약간의 텀을 두고 자연스럽게 접속시킵니다.
    if i % 100 == 0:
        time.sleep(0.1) 

print("✅ 2000명 상시 접속 완료! (Ctrl+C를 누르기 전까지 유지됩니다)")

# 메인 스레드가 죽지 않도록 무한 대기
while True:
    time.sleep(1)