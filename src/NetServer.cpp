#include "NetServer.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <iterator>

std::string convertToJSON(const std::vector<std::shared_ptr<Video>>& topList);

NetServer::NetServer(std::string ip, uint16_t port, int threadNum)
    : _server(ip,port,threadNum),_rm(100) { // 默认 Top 100
    
    _server.setnewconnectioncb(std::bind(&NetServer::onConnection, this, std::placeholders::_1));
    _server.setonmessagecb(std::bind(&NetServer::onMessage, this, std::placeholders::_1, std::placeholders::_2));
}

void NetServer::start() 
{
    // 在启动网络服务器之前，启动定时同步任务
    std::thread timerThread(&NetServer::runTimerTasks, this);
    timerThread.detach(); // 分离线程，让它在后台自主运行

    std::cout << "------------------------------------------------" << std::endl;
    std::cout << "服务器已启动!" << std::endl;
    std::cout << "1. 监听端口: 8000" << std::endl;
    std::cout << "2. 数据库同步: 每 5s 一次" << std::endl;
    std::cout << "3. 磁盘备份: 每 60s 一次" << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    _server.start(); // 这里会调用 mainloop_->run()，是阻塞的
}

void NetServer::onConnection(TcpConnectionPtr conn) 
{
    if(conn->disconnect_)
    {
        std::cout << "连接断开, fd: " << conn->fd() << std::endl;
    }
    else
    {
        std::cout << "新连接建立, fd: " << conn->fd() << std::endl;
    }
}

void NetServer::onMessage(TcpConnectionPtr conn, std::string& message)
{
    try 
    {
        if (message.empty()) return;

        // ==========================================
        // 1. HTTP 请求处理 (逻辑保持不变)
        // ==========================================
        if (message.rfind("GET ", 0) == 0) {

            if (message.find("/api/rank") != std::string::npos) {
                std::string jsonBody = convertToJSON(_rm.getTopList());
                std::string response =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json; charset=UTF-8\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Connection: close\r\n"
                    "Content-Length: " + std::to_string(jsonBody.size()) + "\r\n\r\n" +
                    jsonBody;

                conn->send(response); 
                // 注意：如果你的库没有 shutdown()，直接调用你的关闭方法
                // 或者等客户端关闭
                return;
            }

            if (message.find("GET /") != std::string::npos) {
                std::ifstream file("web_ui/index.html");
                if (!file.is_open()) {
                    conn->send("HTTP/1.1 404 Not Found\r\n\r\n");
                    return;
                }

                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                std::string response =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: close\r\n"
                    "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n" +
                    content;

                conn->send(response);
                return;
            }
        }

        // ==========================================
        // 2. 压测/常规 TCP 数据处理 (逻辑保持不变)
        // ==========================================
        std::stringstream ss(message);
        std::string line;
        int count = 0;

        while (std::getline(ss, line)) {
            if (line.empty()) continue;
            std::stringstream ls(line);
            std::string id;
            int type;
            double value;
            if (ls >> id >> type >> value) {
                _rm.updateVideo(id, type, value);
                count++;
            }
        }

        if (count > 0) {
            // std::cout << "Successfully processed " << count << " updates." << std::endl;
        }

    }
    catch (const std::exception& e) 
    {
        std::cerr << "CRITICAL ERROR in onMessage: " << e.what() << std::endl;
    }
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

void NetServer::runTimerTasks() 
{
    // 记录上一次全量备份的时间
    auto lastBackupTime = std::chrono::steady_clock::now();

    while (true) 
    {
        // 1. 每 5 秒执行一次数据库同步
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // std::cout << "[Timer] 正在同步 Top100 数据到 MySQL..." << std::endl;
        _rm.syncToDb();

        // 2. 检查是否到了 60 秒（全量备份）
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastBackupTime).count();
        
        if (elapsed >= 60) 
        {
            // std::cout << "[Timer] 正在执行全量数据磁盘备份..." << std::endl;
            _rm.saveToFile("rank_data.txt");
            lastBackupTime = currentTime;
        }
    }
}