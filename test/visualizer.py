import requests
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# 设置中文字体（防止乱码）
plt.rcParams['font.sans-serif'] = ['Arial'] 
plt.rcParams['axes.unicode_minus'] = False

fig, ax = plt.subplots(figsize=(10, 6))

def animate(i):
    try:
        # 获取后端 JSON 数据
        response = requests.get("http://127.0.0.1:8000/api/rank", timeout=1)
        data = response.json()
        
        if not data: return

        # 解析数据
        ids = [item['id'] for item in data][::-1] # 逆序方便画横向条形图
        heats = [item['heat'] for item in data][::-1]
        
        ax.clear()
        # 画条形图
        colors = plt.cm.get_cmap('viridis', len(ids))
        bars = ax.barh(ids, heats, color=colors(range(len(ids))))
        
        # 添加标签
        ax.set_xlabel('实时热度值')
        ax.set_title('短视频热度 TOP 10 实时监控')
        
        # 在条形图末尾标注热度数值
        for bar in bars:
            width = bar.get_width()
            ax.text(width + 1, bar.get_y() + bar.get_height()/2, 
                    f'{width:.1f}', va='center')

    except Exception as e:
        print(f"Waiting for server... {e}")

# 每 500ms 刷新一次图表
ani = FuncAnimation(fig, animate, interval=500)
plt.tight_layout()
plt.show()