#pragma once
#include "muduo/TcpServer.h"  
#include "RankManager.h"
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

// 关键：把你库里的 spConnection 定义为业务层用的 TcpConnectionPtr
using TcpConnectionPtr = muduo::spConnection; 

class NetServer {
public:
    // 构造函数参数改为你的库支持的格式
    NetServer(std::string ip, uint16_t port, int threadNum);
    void start();
    RankManager& getRankManager();

private:
    muduo::TcpServer _server;
    RankManager _rm;

    // 回调函数参数对应你库里的定义
    void onConnection(TcpConnectionPtr conn);
    void onMessage(TcpConnectionPtr conn, std::string& message); // 你的库直接给 string

    // 增加定时任务的启动函数
    void runTimerTasks(); 
};