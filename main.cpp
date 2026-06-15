#include <csignal>
#include <iostream>
#include "NetServer.h"
#include<iostream>
#include <mysql/mysql.h> 
#include"Connection.h"
#include<string>
#include"CommonConnectionPool.h"

// 为了能在信号处理函数中访问到服务器对象，定义一个全局指针（或者使用单例）
NetServer* g_server = nullptr;

// 信号处理函数：当你按下 Ctrl+C 时触发
void handleSignal(int sig) {
    if (g_server) {
        std::cout << "\n[System] 捕获到退出信号 (" << sig << ")，正在进行紧急存档..." << std::endl;
        // 1. 立即同步一次数据库
        g_server->getRankManager().syncToDb();
        // 2. 立即备份一次本地文件
        g_server->getRankManager().saveToFile("rank_data.txt");
        std::cout << "[System] 存档完成，程序安全退出。" << std::endl;
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    // 1. 初始化数据库连接池
    ConnectionPool *cp = ConnectionPool::getConnectionPool();

    // 2. 创建muduo
    NetServer server("0.0.0.0", 8000, 4);

    // 3. 启动时：从本地文件恢复数据
    // server.getRankManager().loadFromFile("rank_data.txt");

    // 4. 注册信号处理：处理 Ctrl+C (SIGINT)
    std::signal(SIGINT, handleSignal);

    // 5. 启动服务器（内部会调用 mainloop_->run()）
    server.start();

    return 0;
}