#ifndef NET_SERVER_H
#define NET_SERVER_H

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include "RankManager.h"

class NetServer {
public:
    void setThreadNum(int numThreads); 
    NetServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listenAddr);
    void start();
    RankManager& getRankManager();

private:
    // 处理连接事件
    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    // 处理消息接收事件
    void onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp time);

    muduo::net::TcpServer _server;
    RankManager _rm; // 核心逻辑对象

};

#endif