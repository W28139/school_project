#include <muduo/net/EventLoop.h>
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
    g_server->getRankManager().syncToDb();

    muduo::net::EventLoop loop;
    muduo::net::InetAddress listenAddr(8000);
    
    // 2. 创建服务器对象
    NetServer server(&loop, listenAddr);
    g_server = &server; // 赋值给全局指针，供信号处理函数使用

    // 3. 【核心逻辑 A】启动时：从本地文件恢复数据
    // server.getRankManager().loadFromFile("rank_data.txt");

    // 4. 注册信号处理：处理 Ctrl+C (SIGINT)
    std::signal(SIGINT, handleSignal);

    // 5. 设置服务器线程数
    server.setThreadNum(4);
    server.start();

    // 6. 【核心逻辑 B】设置定时器 (muduo 内部机制)
    
    // 定时任务 1：每 5 秒同步一次 MySQL (保证前端看到的排行榜是准实时的)
    loop.runEvery(5.0, [&](){
        std::cout << "[Timer] 正在同步 Top100 数据到 MySQL..." << std::endl;
        server.getRankManager().syncToDb();
    });

    // 定时任务 2：每 60 秒做一次全量磁盘备份 (防止硬件故障或断电丢失非榜单数据)
    loop.runEvery(60.0, [&](){
        std::cout << "[Timer] 正在执行全量数据磁盘备份..." << std::endl;
        server.getRankManager().saveToFile("rank_data.txt");
    });

    std::cout << "------------------------------------------------" << std::endl;
    std::cout << "服务器已启动!" << std::endl;
    std::cout << "1. 监听端口: 8000" << std::endl;
    std::cout << "2. 数据库同步: 每 5s 一次" << std::endl;
    std::cout << "3. 磁盘备份: 每 60s 一次" << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    loop.loop(); 
    
    return 0;
}