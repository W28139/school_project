#include "NetServer.h"
#include <iostream>
#include <sstream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <iterator>

using namespace muduo;
using namespace muduo::net;

std::string convertToJSON(const std::vector<std::shared_ptr<Video>>& topList);

NetServer::NetServer(EventLoop* loop, const InetAddress& listenAddr)
    : _server(loop, listenAddr, "VideoRankServer"), _rm(100) { // 默认 Top 100
    
    _server.setConnectionCallback(std::bind(&NetServer::onConnection, this, _1));
    _server.setMessageCallback(std::bind(&NetServer::onMessage, this, _1, _2, _3));
}

void NetServer::start() 
{
    _server.start();
}

void NetServer::onConnection(const TcpConnectionPtr& conn) 
{
    if (conn->connected()) 
    {
        std::cout << "新连接建立: " << conn->peerAddress().toIpPort() << std::endl;
    } 
    else 
    {
        std::cout << "连接断开: " << conn->peerAddress().toIpPort() << std::endl;
    }
}

void NetServer::onMessage(
    const muduo::net::TcpConnectionPtr& conn,
    muduo::net::Buffer* buf,
    muduo::Timestamp time)
{
    try 
    {
        // 从缓冲区中取出所有数据并转换成字符串
        std::string msg(buf->retrieveAllAsString());
        if (msg.empty()) return;

        // ==========================================
        // 1. HTTP 请求处理 (通过判断字符串开头是否为 "GET ")
        // ==========================================
        if (msg.rfind("GET ", 0) == 0) {

            // 场景 A: 获取排行榜 JSON 数据接口
            if (msg.find("/api/rank") != std::string::npos) {
                
                // 理器从排名管 _rm 获取顶尖列表，并转换为 JSON 字符串
                std::string jsonBody = convertToJSON(_rm.getTopList());

                // 构造 HTTP 响应报文
                std::string response =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json; charset=UTF-8\r\n"
                    "Access-Control-Allow-Origin: *\r\n" // 允许跨域，方便前端调用
                    "Connection: close\r\n"             // 处理完即关闭连接（短连接）
                    "Content-Length: " + std::to_string(jsonBody.size()) + "\r\n\r\n" +
                    jsonBody;

                conn->send(response);   // 发送响应
                conn->shutdown();       // 主动关闭连接（遵循 HTTP/1.0 习惯）
                return;
            }

            // 场景 B: 访问主页 (根目录)
            if (msg.find("GET /") != std::string::npos) {

                // 读取本地 HTML 文件
                std::ifstream file("web_ui/index.html");
                if (!file.is_open()) {
                    conn->send("HTTP/1.1 404 Not Found\r\n\r\n");
                    conn->shutdown();
                    return;
                }

                // 将文件内容一次性读入字符串
                std::string content(
                    (std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>()
                );

                // 构造 HTML 响应报文
                std::string response =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: close\r\n"
                    "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n" +
                    content;

                conn->send(response);
                conn->shutdown();
                return;
            }

            // 场景 C: 其他未定义的路径，返回 404
            conn->send("HTTP/1.1 404 Not Found\r\n\r\n");
            conn->shutdown();
            return;
        }

        // ==========================================
        // 2. 压测/常规 TCP 数据处理 (非 HTTP 协议)
        // 期望格式: "视频ID 类型(int) 分值(double)\n"
        // ==========================================

        std::stringstream ss(msg);
        std::string line;
        int count = 0;

        // 按行解析数据，支持批量发送多行数据
        while (std::getline(ss, line)) {

            if (line.empty()) continue;

            std::stringstream ls(line);
            std::string id;
            int type;
            double value;

            // 尝试解析: id type value (例如: video123 1 95.5)
            if (ls >> id >> type >> value) 
            {
                // 更新排名管理器的内存数据结构
                _rm.updateVideo(id, type, value);
                count++;
            }
        }

        // 如果成功处理了数据，在控制台打印日志
        if (count > 0) 
        {
            std::cout << "Successfully processed " << count << " updates." << std::endl;
        }

    }
    catch (const std::exception& e) 
    {
        // 异常处理，防止单个请求崩溃导致整个服务退出
        std::cerr << "CRITICAL ERROR in onMessage: " << e.what() << std::endl;
    }
}

void NetServer::setThreadNum(int numThreads) {
    _server.setThreadNum(numThreads);
}

// 辅助函数：将榜单转换为 JSON 字符串
std::string convertToJSON(const std::vector<std::shared_ptr<Video>>& topList) {
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < topList.size(); ++i) {
        ss << "{"
           << "\"rank\":" << i + 1 << ","
           << "\"id\":\"" << topList[i]->videoId << "\","
           << "\"heat\":" << std::fixed << std::setprecision(2) << topList[i]->heat << ","
           << "\"play\":" << topList[i]->playCount << ","
           << "\"like\":" << topList[i]->likeCount
           << "}";
        if (i < topList.size() - 1) ss << ",";
    }
    ss << "]";
    return ss.str();
}

RankManager& NetServer::getRankManager()
{
    return _rm;
}