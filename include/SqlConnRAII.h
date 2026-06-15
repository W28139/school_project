#ifndef SQL_CONN_RAII_H
#define SQL_CONN_RAII_H
#include "SqlConnPool.h"

// 自动获取和释放连接的助手
class SqlConnRAII{
public:
    SqlConnRAII(MYSQL** sql, SqlConnPool* connpool) 
    {
        *sql = connpool->GetConn();
        _sql = *sql;
        _connpool = connpool;
    }
    
    ~SqlConnRAII() 
    {
        if(_sql) _connpool->FreeConn(_sql);
    }

private:
    MYSQL* _sql;
    SqlConnPool* _connpool;
};
#endif