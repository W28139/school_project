import socket
import random
import threading
import time

# 配置
SERVER_IP = "192.144.188.86"
SERVER_PORT = 8000
THREAD_COUNT = 10  # 10个线程同时发
MSG_PER_THREAD = 10000  # 每个线程发2000条数据

def send_data():
    try:
        # 创建长连接
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((SERVER_IP, SERVER_PORT))
        
        for _ in range(MSG_PER_THREAD):
            vid = f"video_{random.randint(1, 50)}" # 模拟50个视频
            action = random.randint(1, 3) # 1:播, 2:赞, 3:享
            value = 1
            msg = f"{vid} {action} {value}\n"
            s.sendall(msg.encode())
            # 模拟极小的延迟，或者不延迟直接冲垮服务器
            # time.sleep(0.001) 
        
        s.close()
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    start_time = time.time()
    threads = []
    print(f"开始压测，启动 {THREAD_COUNT} 个线程...")
    
    for i in range(THREAD_COUNT):
        t = threading.Thread(target=send_data)
        threads.append(t)
        t.start()

    for t in threads:
        t.join()
        
    end_time = time.time()
    print(f"压测结束！总计发送 {THREAD_COUNT * MSG_PER_THREAD} 条指令")
    print(f"耗时: {end_time - start_time:.2f} 秒")