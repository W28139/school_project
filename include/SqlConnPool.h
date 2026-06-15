#ifndef SQL_CONN_POOL_H
#define SQL_CONN_POOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>

class SqlConnPool {
public:
    static SqlConnPool* Instance(); // 单例模式

    MYSQL* GetConn();                // 取出一个连接
    void FreeConn(MYSQL* conn);      // 归还一个连接
    int GetFreeConnCount();          // 获取空闲连接数

    void Init(std::string host, int port, std::string user, std::string pwd, std::string dbName, int connSize);
    void ClosePool();

private:
    SqlConnPool();
    ~SqlConnPool();

    int _maxConn;
    std::queue<MYSQL*> _connQue; // 连接队列
    std::mutex _mtx;
    sem_t _sem;                  // 信号量用于同步
};

#endif