import streamlit as st
import psutil 
import requests
import os
import glob
import pandas as pd
import time
import altair as alt
import datetime
import threading # ★ 추가: AI를 백그라운드에서 돌리기 위한 스레드
from streamlit.runtime.scriptrunner import add_script_run_ctx # ★ 추가: 스레드 경고 방지

# ==========================================
# ⚙️ [초기 설정] 경로 및 환경 변수
# ==========================================
SERVER_PROJECT_PATH = r"D:\Projects\VibeToExtreme\VibeToExtreme_Server" 
LOG_FILE_PATH = os.path.join(SERVER_PROJECT_PATH, "server_log.txt")
NETWORK_CODE_PATH = os.path.join(SERVER_PROJECT_PATH, "NetworkCore.cpp")

st.set_page_config(page_title="VibeToExtreme 프로젝트 관제소", page_icon="🚀", layout="wide")

# ==========================================
# 🧠 세션 상태(State) 초기화
# ==========================================
if "ai_report" not in st.session_state: st.session_state.ai_report = None
if "last_analyzed_log" not in st.session_state: st.session_state.last_analyzed_log = ""
if "chat_history" not in st.session_state: st.session_state.chat_history = []
if "is_monitoring" not in st.session_state: st.session_state.is_monitoring = False
if "cpu_history" not in st.session_state: st.session_state.cpu_history = []
if "mem_history" not in st.session_state: st.session_state.mem_history = []
if "chart_style" not in st.session_state: st.session_state.chart_style = "Line Chart (추세)"
if "ai_is_analyzing" not in st.session_state: st.session_state.ai_is_analyzing = False # ★ AI 분석 상태 추가

st.title("🚀 VibeToExtreme 통합 관리 대시보드")

tab1, tab2, tab3 = st.tabs(["📊 통합 관제 센터", "💬 AI 코드 어시스턴트", "📚 AI 자동 산출물 (구조도)"])

# ------------------------------------------
# [TAB 1] 통합 실시간 관제 센터 (리소스 + 크래시)
# ------------------------------------------
with tab1:
    st.subheader("🖥️ VibeToExtreme 통합 관제 센터")
    
    col_btn1, col_btn2 = st.columns([2, 8])
    with col_btn1:
        button_label = "🛑 통합 관제 정지" if st.session_state.is_monitoring else "🔴 실시간 통합 관제 시작"
        if st.button(button_label):
            st.session_state.is_monitoring = not st.session_state.is_monitoring
            st.rerun()
            
    with col_btn2:
        # ★ 수정: 생략되었던 백업 로직 완벽 복구
        if st.button("💾 서버 로그 백업 및 초기화 (Archive)"):
            if os.path.exists(LOG_FILE_PATH):
                history_dir = os.path.join(SERVER_PROJECT_PATH, "history")
                os.makedirs(history_dir, exist_ok=True)
                now_str = datetime.datetime.now().strftime("%Y_%m%d_%H%M%S")
                backup_filepath = os.path.join(history_dir, f"server_log_{now_str}.txt")

                with open(LOG_FILE_PATH, "r", encoding="utf-8") as f_orig:
                    log_content = f_orig.read()
                
                if log_content.strip():
                    with open(backup_filepath, "w", encoding="utf-8") as f_backup:
                        f_backup.write(log_content)
                    st.toast(f"✅ 백업 완료: server_log_{now_str}.txt")

                open(LOG_FILE_PATH, 'w').close()

            st.session_state.ai_report = None
            st.session_state.last_analyzed_log = ""
            st.rerun()

    # ★ 수정: index를 강제 지정하지 않고 key로만 상태를 묶어서 리셋 방지
    st.radio("📈 차트 스타일 선택", ["Line Chart (추세)", "Bar Chart (현재)"], key="chart_style", horizontal=True)

    # ★ AI를 백그라운드에서 몰래 돌리기 위한 함수
    def background_ai_task(prompt_text, current_log_text):
        try:
            res = requests.post('http://localhost:11434/api/generate', json={"model": "qwen2.5-coder", "prompt": prompt_text, "stream": False})
            st.session_state.ai_report = res.json()['response']
        except Exception as e:
            st.session_state.ai_report = f"로컬 AI 연결 실패: {e}"
        finally:
            st.session_state.last_analyzed_log = current_log_text
            st.session_state.ai_is_analyzing = False # 분석 완료!

    @st.fragment(run_every=1)
    def live_dashboard_fragment():
        if not st.session_state.is_monitoring:
            st.info("⏸️ 관제가 정지되었습니다.")
            return

        col_left, col_right = st.columns([6, 4])
        
        with col_left:
            # CPU 측정 정확도 복구 완료
            cpu = psutil.cpu_percent(interval=0.1) 
            mem = psutil.virtual_memory().percent
            connections = psutil.net_connections()
            session_count = len([conn for conn in connections if conn.laddr.port == 7777 and conn.status == 'ESTABLISHED'])
            
            metric_col1, metric_col2, metric_col3 = st.columns(3)
            metric_col1.metric("CPU 점유율", f"{cpu}%")
            metric_col2.metric("메모리 점유율", f"{mem}%")
            metric_col3.metric("현재 접속자 수", f"{session_count} 명")

            st.session_state.cpu_history.append(cpu)
            st.session_state.mem_history.append(mem)
            if len(st.session_state.cpu_history) > 50: st.session_state.cpu_history.pop(0)
            if len(st.session_state.mem_history) > 50: st.session_state.mem_history.pop(0)

            if "Line" in st.session_state.chart_style:
                df_line = pd.DataFrame({"CPU (%)": st.session_state.cpu_history, "RAM (%)": st.session_state.mem_history})
                st.line_chart(df_line, height=300)
            else:
                df_bar = pd.DataFrame({"사용량 (%)": [cpu, mem]}, index=["🖥️ CPU", "💾 RAM"])
                chart = alt.Chart(df_bar.reset_index()).mark_bar(size=80).encode(
                    x=alt.X('index', axis=alt.Axis(labelAngle=0, title=None)),
                    y=alt.Y('사용량 (%)', scale=alt.Scale(domain=[0, 100]))
                ).properties(height=300)
                st.altair_chart(chart, use_container_width=True)
                
        with col_right:
            st.markdown("#### 🚨 서버 로그 & AI 감시 (실시간)")
            
            current_log = "서버 대기 중... 로그 파일이 아직 없습니다."
            is_crashed = False
            
            if os.path.exists(LOG_FILE_PATH):
                with open(LOG_FILE_PATH, "r", encoding="utf-8") as f:
                    lines = f.readlines()
                    if lines:
                        current_log = "".join(lines[-15:])
                        if "FATAL ERROR" in current_log:
                            is_crashed = True
                            
            st.text_area(label="C++ Server Log", value=current_log, height=180, disabled=True, label_visibility="collapsed")
                
            # ★ 핵심: 크래시 감지 시 스레드로 던져버리기 (차트 멈춤 방지!)
            if is_crashed:
                if st.session_state.last_analyzed_log != current_log:
                    if not st.session_state.ai_is_analyzing:
                        st.session_state.ai_is_analyzing = True # 분석 시작 플래그 ON
                        st.session_state.ai_report = None
                        
                        cpp_code = "코드를 찾을 수 없습니다."
                        if os.path.exists(NETWORK_CODE_PATH):
                            with open(NETWORK_CODE_PATH, "r", encoding="utf-8") as f: cpp_code = f.read()
                            
                        prompt = f"15년차 C++ 서버 전문가로서 아래 로그와 코드를 보고 에러 원인을 3줄 요약해.\n\n[로그]\n{current_log}\n\n[코드]\n{cpp_code}"
                        
                        # 하청업체(스레드)에게 일을 던지고 메인 화면은 계속 차트를 그립니다!
                        t = threading.Thread(target=background_ai_task, args=(prompt, current_log))
                        add_script_run_ctx(t) # 스레드가 Streamlit 상태를 건드릴 수 있게 허락해줌
                        t.start()

            # 분석 중일 때와 분석이 끝났을 때의 UI 분리
            if st.session_state.ai_is_analyzing:
                st.warning("🚨 크래시 감지! 로컬 AI가 백그라운드에서 분석 중입니다... ⚙️ (차트는 멈추지 않습니다!)")
            elif st.session_state.ai_report:
                st.success("✔️ AI 실시간 분석 결과")
                st.info(st.session_state.ai_report)

    live_dashboard_fragment()

# ------------------------------------------
# [TAB 2] AI 코드 어시스턴트 (새로운 종합 관리 기능)
# ------------------------------------------
with tab2:
    st.subheader("🤖 내 프로젝트 로컬 AI 멘토")
    st.markdown("프로젝트 내의 C++ 파일을 선택하고, 궁금한 점이나 최적화 방안을 질문하세요.")

    # 1. 프로젝트 폴더 내의 모든 .cpp, .h 파일 검색
    cpp_files = glob.glob(os.path.join(SERVER_PROJECT_PATH, "**/*.cpp"), recursive=True)
    h_files = glob.glob(os.path.join(SERVER_PROJECT_PATH, "**/*.h"), recursive=True)
    all_source_files = cpp_files + h_files
    
    # 2. 파일 선택 드롭다운
    file_names = [os.path.basename(f) for f in all_source_files]
    selected_file_name = st.selectbox("📂 분석할 소스 코드를 선택하세요:", ["선택 안함"] + file_names)

    # 3. 채팅 UI
    for msg in st.session_state.chat_history:
        with st.chat_message(msg["role"]):
            st.markdown(msg["content"])

    # 사용자가 채팅창에 입력했을 때
    if user_input := st.chat_input("예: 이 파일의 IOCP 로직에서 메모리 누수가 발생할 수 있는 엣지 케이스를 찾아줘."):
        # 사용자 메시지 화면에 표시 및 저장
        st.session_state.chat_history.append({"role": "user", "content": user_input})
        with st.chat_message("user"):
            st.markdown(user_input)

        # 선택된 파일이 있다면 코드를 읽어서 프롬프트에 몰래 끼워 넣음 (Context Injection)
        context_code = ""
        if selected_file_name != "선택 안함":
            selected_path = all_source_files[file_names.index(selected_file_name)]
            with open(selected_path, "r", encoding="utf-8") as f:
                context_code = f"\n\n[참고 소스 코드: {selected_file_name}]\n{f.read()}"

        # AI에게 보낼 최종 프롬프트 조합
        final_prompt = f"너는 C++ IOCP 아키텍처를 가르치는 엄청나게 엄격한 10년 차 게임 서버 멘토야. 학생이 바이브 코딩으로 개발 중이니 유지보수성과 메모리 누수 관점에서 예리하게 답변해.{context_code}\n\n질문: {user_input}"

        # 로컬 AI 추론 및 답변 출력
        with st.chat_message("assistant"):
            with st.spinner("코드를 분석하며 답변을 작성 중입니다..."):
                try:
                    res = requests.post('http://localhost:11434/api/generate', json={"model": "qwen2.5-coder", "prompt": final_prompt, "stream": False})
                    bot_reply = res.json()['response']
                    st.markdown(bot_reply)
                    st.session_state.chat_history.append({"role": "assistant", "content": bot_reply})
                except Exception as e:
                    st.error("Ollama 서버에 연결할 수 없습니다.")

# ------------------------------------------
# [TAB 3] AI 프로젝트 자동 문서화 (전체 숲 보기)
# ------------------------------------------
with tab3:
    st.subheader("📚 AI 자동 생성 프로젝트 명세서")
    st.markdown("로컬 AI가 전체 C++ 소스 코드를 순회하며 아키텍처 문서를 자동으로 작성합니다.")
    
    doc_file_path = "project_docs.md"

    # [주의!] 파일이 많을수록 시간이 걸리므로 버튼으로 수동 트리거합니다.
    if st.button("🚀 전체 프로젝트 AI 문서화 시작 (약 1~2분 소요)"):
        with st.spinner("AI가 전체 소스 코드를 읽고 명세서를 작성 중입니다... (VRAM 풀가동 중)"):
            
            # 문서의 헤더 작성
            compiled_docs = "# 🚀 VibeToExtreme 서버 아키텍처 명세서\n\n"
            
            # Tab 2에서 만들어둔 all_source_files 리스트를 재활용합니다.
            for file_path in all_source_files:
                file_name = os.path.basename(file_path)
                
                with open(file_path, "r", encoding="utf-8") as f:
                    code = f.read()
                
                # 파일 1개당 프롬프트를 쏴서 요약본을 받아옵니다.
                prompt = f"""
                너는 C++ 게임 서버 아키텍트야. 
                다음 '{file_name}' 파일의 핵심 역할과 중요한 클래스/함수를 
                마크다운의 글머리 기호(-)를 사용해서 3~4줄로 아주 짧고 명확하게 요약해.
                
                [🚨절대 규칙] 
                반드시 100% 한국어(Korean)로만 대답해. 영어 문장이나 한자를 절대 섞지 마!!
                
                코드:
                {code}
                """
                
                try:
                    res = requests.post('http://localhost:11434/api/generate', json={
                        "model": "qwen2.5-coder", 
                        "prompt": prompt, 
                        "stream": False
                    })
                    summary = res.json()['response']
                    
                    # 마크다운 텍스트에 누적
                    compiled_docs += f"### 📄 `{file_name}`\n{summary}\n\n---\n\n"
                except Exception as e:
                    compiled_docs += f"### 📄 `{file_name}`\n요약 실패: {e}\n\n"
            
            # 최종 완성된 문서를 파일로 저장
            with open(doc_file_path, "w", encoding="utf-8") as f:
                f.write(compiled_docs)
                
            st.success("✔️ 전체 프로젝트 문서화가 완료되었습니다!")

    # 저장된 문서가 있다면 화면에 이쁘게 뿌려줍니다.
    if os.path.exists(doc_file_path):
        with open(doc_file_path, "r", encoding="utf-8") as f:
            st.markdown(f.read())
