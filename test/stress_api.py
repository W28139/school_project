from flask import Flask, jsonify, request
from flask_cors import CORS

import socket
import random
import threading
import time

app = Flask(__name__)
CORS(app)

# =====================================================
# 配置
# =====================================================

SERVER_IP = "127.0.0.1"
SERVER_PORT = 8000 

USER_COUNT = 100000
VIDEO_COUNT = 10000

def random_video():

    r = random.random()

    # 80%流量打到前20%视频 (保持原有长尾分布逻辑)
    if r < 0.80:
        return f"video_{random.randint(1, 2000)}"

    # 剩余20%流量打到剩余视频 (长尾)
    return f"video_{random.randint(2001, VIDEO_COUNT)}"


# =====================================================
# 行为分布
# =====================================================

def random_action():

    r = random.random()

    # 播放
    if r < 0.75:
        return 1

    # 点赞
    elif r < 0.90:
        return 2

    # 分享
    elif r < 0.95:
        return 3

    # 完播率
    return 4


# =====================================================
# 单线程Worker
# =====================================================

def worker(msg_count):

    try:

        s = socket.socket(
            socket.AF_INET,
            socket.SOCK_STREAM
        )

        s.settimeout(10)

        s.connect(
            (
                SERVER_IP,
                SERVER_PORT
            )
        )

        buffer = []

        for _ in range(msg_count):

            uid = random.randint(
                1,
                USER_COUNT
            )

            vid = random_video()

            action = random_action()

            if action == 4:

                value = round(
                    random.uniform(0.3, 1.0),
                    2
                )

            else:

                value = 1

            # 当前兼容你现有C++
            msg = (
                f"{vid} "
                f"{action} "
                f"{value}\n"
            )

            buffer.append(msg)

            # 批量发送
            if len(buffer) >= 100:

                s.sendall(
                    "".join(buffer).encode()
                )

                buffer.clear()

        if buffer:

            s.sendall(
                "".join(buffer).encode()
            )

        s.close()

    except Exception as e:

        print(
            "Worker Error:",
            e
        )


# =====================================================
# 压测主函数
# =====================================================

def run_stress(
    thread_count,
    total_messages
):

    start = time.time()

    per_thread = (
        total_messages //
        thread_count
    )

    threads = []

    print(
        "\n"
        "====================================\n"
        f"Stress Start\n"
        f"Threads  : {thread_count}\n"
        f"Messages : {total_messages}\n"
        f"Videos   : {VIDEO_COUNT}\n"
        f"Users    : {USER_COUNT}\n"
        "===================================="
    )

    for _ in range(thread_count):

        t = threading.Thread(
            target=worker,
            args=(per_thread,)
        )

        t.start()

        threads.append(t)

    for t in threads:

        t.join()

    cost = time.time() - start

    qps = int(
        total_messages /
        cost
    )

    print(
        "\n"
        "====================================\n"
        "Stress Finished\n"
        f"Cost : {cost:.2f}s\n"
        f"QPS  : {qps}\n"
        "====================================\n"
    )

# =====================================================
# 路由接口 (保持不变)
# =====================================================

@app.route('/stress/10k')
def stress_10k():
    threading.Thread(target=run_stress, args=(10, 10000)).start()
    return jsonify({"status":"success","threads":10,"messages":10000})

@app.route('/stress/100k')
def stress_100k():
    threading.Thread(target=run_stress, args=(20, 100000)).start()
    return jsonify({"status":"success","threads":20,"messages":100000})

@app.route('/stress/1m')
def stress_1m():
    threading.Thread(target=run_stress, args=(50, 1000000)).start()
    return jsonify({"status":"success","threads":50,"messages":1000000})

@app.route('/stress/5m')
def stress_5m():
    threading.Thread(target=run_stress, args=(100, 5000000)).start()
    return jsonify({"status":"success","threads":100,"messages":5000000})

@app.route('/stress/custom', methods=['POST'])
def custom():
    data = request.json
    thread_count = int(data.get("threads", 20))
    total_messages = int(data.get("messages", 100000))
    threading.Thread(target=run_stress, args=(thread_count, total_messages)).start()
    return jsonify({"status":"success","threads":thread_count,"messages":total_messages})

@app.route('/')
def index():
    return jsonify({"status":"ok","service":"stress_api","videos":VIDEO_COUNT,"users":USER_COUNT})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, threaded=True)