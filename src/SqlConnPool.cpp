#include "SqlConnPool.h"
#include <iostream>

SqlConnPool::SqlConnPool() {
    _maxConn = 0;
}

SqlConnPool::~SqlConnPool() {
    ClosePool();
}

// 单例模式：懒汉式
SqlConnPool* SqlConnPool::Instance() {
    static SqlConnPool pool;
    return &pool;
}

// 初始化连接池
void SqlConnPool::Init(std::string host, int port, std::string user, std::string pwd, std::string dbName, int connSize) {
    _maxConn = connSize;

    for (int i = 0; i < _maxConn; i++) {
        MYSQL* sql = nullptr;
        sql = mysql_init(sql);
        if (!sql) {
            std::cerr << "MySQL init error!" << std::endl;
            exit(1);
        }
        sql = mysql_real_connect(sql, host.c_str(), user.c_str(), pwd.c_str(), dbName.c_str(), port, nullptr, 0);
        if (!sql) {
            std::cerr << "MySQL connect error: " << mysql_error(sql) << std::endl;
            exit(1);
        }
        _connQue.push(sql);
    }

    // 初始化信号量，初始值为连接池大小
    sem_init(&_sem, 0, _maxConn);
}

// 从池中取出一个连接
MYSQL* SqlConnPool::GetConn() {
    MYSQL* sql = nullptr;
    if (_connQue.empty()) {
        std::cerr << "SqlConnPool is busy!" << std::endl;
        return nullptr;
    }

    // 等待信号量（若值为0则阻塞，直到有连接归还）
    sem_wait(&_sem);

    {
        std::lock_guard<std::mutex> lock(_mtx);
        sql = _connQue.front();
        _connQue.pop();
    }
    return sql;
}

// 归还连接到池中
void SqlConnPool::FreeConn(MYSQL* sql) {
    if (!sql) return;

    {
        std::lock_guard<std::mutex> lock(_mtx);
        _connQue.push(sql);
    }

    // 释放信号量，通知等待的线程
    sem_post(&_sem);
}

// 关闭并清空连接池
void SqlConnPool::ClosePool() {
    std::lock_guard<std::mutex> lock(_mtx);
    while (!_connQue.empty()) {
        MYSQL* sql = _connQue.front();
        _connQue.pop();
        mysql_close(sql);
    }
    mysql_library_end();
}

int SqlConnPool::GetFreeConnCount() {
    std::lock_guard<std::mutex> lock(_mtx);
    return _connQue.size();
}