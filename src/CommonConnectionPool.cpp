#include"CommonConnectionPool.h"
#include"public.h"
#include <iostream>
#include <fstream>
#include<thread>
#include <chrono>
ConnectionPool::ConnectionPool()
{
    // 之前写的加载配置文件的函数在这里调用
    if (!loadConfigFile())
    {
        return;
    }
    for(int i=0;i<_initSize;i++)
    {
        Connection *p =new Connection();
        if(p->connect(_ip,_port,_username,_password,_dbname))
        {
            p->refreshAliveTime();  // 刷新一下开始空闲的起始时间
            _connectionQue.push(p);
            _connectionCnt++;
        }
    }

    // 启动一个新的线程，作为连接的生产者
    std::thread produce(std::bind(&ConnectionPool::produceConnectionTask,this));
    // 启动生产者线程并分离,主线程结束后，这个线程自动结束
    produce.detach();
    
    // 启动一个新的线程，扫描多余的空闲连接，超过maxIdleTime时间的空闲连接，进行对于超时连接的回收
    std::thread scanner(std::bind(&ConnectionPool::scannerConnectionTask,this));
    // 启动生产者线程并分离
    scanner.detach();

}
 // 线程安全的懒汉单例函数接口
ConnectionPool* ConnectionPool::getConnectionPool()
{
    static ConnectionPool pool;
    return &pool;
}

bool ConnectionPool::loadConfigFile()
{
    std::ifstream ifs("mysql.cnf");
    if (!ifs.is_open())
    {
        LOG("mysql.cnf file is not exist!");
        return false;
    }

    std::string line;
    while (std::getline(ifs, line))
    {
        // 1. 去掉空行和注释行
        if (line.empty() || line[0] == '#') 
        {
            continue;
        }

        // 2. 查找等号
        size_t idx = line.find('=');
        if (idx == std::string::npos) 
        {
            continue;
        }

        // 3. 分离 key 和 value
        std::string key = line.substr(0, idx);
        std::string value = line.substr(idx + 1);

        // 4. 【关键】去除 key 和 value 前后的空格
        auto trim = [](std::string &s) {
            if (s.empty()) return;
            s.erase(0, s.find_first_not_of(" "));
            s.erase(s.find_last_not_of(" ") + 1);
            // 顺便处理掉 Windows 换行符 \r
            if(!s.empty() && s.back() == '\r') s.pop_back();
        };
        
        trim(key);
        trim(value);

        // 5. 填充变量
        if (key == "ip") _ip = value;
        else if (key == "port") _port = atoi(value.c_str());
        else if (key == "username") _username = value;
        else if (key == "password") _password = value;
        else if (key == "dbname") _dbname = value;
        else if (key == "initSize") _initSize = atoi(value.c_str());
        else if (key == "maxSize") _maxSize = atoi(value.c_str());
        else if (key == "maxIdleTime") _maxIdleTime = atoi(value.c_str());
        else if (key == "ConnectionTimeOut") _connectionTimeout = atoi(value.c_str());
    }
    return true;
}

void ConnectionPool::produceConnectionTask()
{
    for(;;)
    {
        std::unique_lock<std::mutex>lock(_queueMutex);
        while(!_connectionQue.empty())
        {
            cv.wait(lock);  // 队列不空的话，此处生产线程进入等待状态 
        }

        // 连接数量没有到达上限，继续创建新的连接
        if(_connectionCnt < _maxSize)
        {
            Connection *p =new Connection();
            if(p->connect(_ip,_port,_username,_password,_dbname))
            {
                p->refreshAliveTime();
                _connectionQue.push(p);
                _connectionCnt++;
            }
            else
            {
                // 连接失败的话，释放p
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                delete p; 
            }
        }
        // 通知消费者线程,可以消费连接了
        cv.notify_all();
    }
}

void ConnectionPool::scannerConnectionTask()
{
    for(;;)
    {
        this_thread::sleep_for(chrono::seconds(_maxIdleTime));

        std::unique_lock<std::mutex>lock(_queueMutex);
        while(_connectionCnt>_initSize)
        {
            Connection *p = _connectionQue.front();
            if(p->getAliveTime()>=(_maxIdleTime*1000))
            {
                _connectionQue.pop();
                _connectionCnt--;
                delete p;
            }
            else
            {
                // 由于进出queue是按时间顺序，如果队头都没超过，那剩下的一定没超过
                break;
            }
        }
    }
}


// 给用户调用的，用户用它获取连接执行mysql指令
std::shared_ptr<Connection> ConnectionPool::getConnection()
{
    std::unique_lock<std::mutex>lock(_queueMutex);
    while(_connectionQue.empty())
    {
        // 等待，但不同于sleep
        if(std::cv_status::timeout == cv.wait_for(lock,
        std::chrono::milliseconds(_connectionTimeout)))
        {
            if(_connectionQue.empty())
            {
                LOG("获取连接超时失败...");
                return nullptr;
            }
        }
    }
    // 到这一步代表有了可用连接，拿出来返回给服务器使用
    std::shared_ptr<Connection>sp(_connectionQue.front(),
        [this](Connection* pcon){
            // 用户执行完，释放智能指针时，执行以下代码
            std::unique_lock<std::mutex>lock(this->_queueMutex);
            pcon->refreshAliveTime();
            this->_connectionQue.push(pcon);
            this->cv.notify_all();
        });
    _connectionQue.pop();

    // 通知生产者队列有空位了(如果它在等的话)
    cv.notify_all();    
    return sp;
}