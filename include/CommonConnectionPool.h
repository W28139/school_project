#pragma once
#include<string>
#include<queue>
#include"Connection.h"
#include<mutex>
#include<atomic>
#include<condition_variable>
#include<memory>
#include<functional>
/*
实现连接池的功能模块
*/

class ConnectionPool
{
public:
    static ConnectionPool* getConnectionPool();
    
    // 给外部提供接口，从连接池中获取一个可用的空闲连接
    std::shared_ptr<Connection> getConnection();


private:
    ConnectionPool();   // 单例，构造函数私有化
    bool loadConfigFile();   // 从配置文件中加载配置项

    // 运行在独立线程中，专门负责生产连接线程
    void produceConnectionTask();

    // 运行再独立线程中，专门负责删除超时的连接线程
    void scannerConnectionTask();

    // MySQL登录的ip,port 和用户名密码 和连接数据库的名称
    std::string _ip;
    unsigned short _port;
    std::string _username;
    std::string _password;
    std::string _dbname;

    // 连接池的初始连接量
    int _initSize;
    // 连接池的最大连接量
    int _maxSize;
    // 连接池的最大空闲时间
    int _maxIdleTime;
    // 连接池获取连接的超时时间
    int _connectionTimeout;

    // 存储MySQL连接的队列(保证线程安全)
    std::queue<Connection*> _connectionQue;
    std::mutex _queueMutex;     // 委会连接队列的线程安全互斥锁
    std::atomic_int _connectionCnt;     // 记录所创建的连接的Connection的总数量

    condition_variable cv;      // 条件变量，用于连接生产线程连接和消费线程的通信

};