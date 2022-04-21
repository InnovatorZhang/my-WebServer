//
// Created by zzh on 2022/4/19.
//

#ifndef MY_WEBSERVER_SQLCONNPOLL_H
#define MY_WEBSERVER_SQLCONNPOLL_H

#include<queue>
#include<mutex>
#include<string>
#include<thread>
#include<semaphore.h>
#include<mysql/mysql.h>

#include"log.h"

/*
 * 单例模式
 */
class SqlConnPool {
public:
    static SqlConnPool *instance();

    MYSQL *getConn();

    void freeConn(MYSQL *sql);

    int getFreeConnCount();

    void init(const char *host, int port,
              const char *user, const char *pwd,
              const char *dbName, int connSize);

    void closePool();

private:
    SqlConnPool();

    ~SqlConnPool();

    int MAX_CONN_;  /*最大连接数量*/
    int useCount_;  /*被使用的连接数量*/
    int freeCount_;  /*空闲连接数量*/

    std::mutex mtx_;  /*互斥量*/
    sem_t semId_;  /*信号量*/
    std::queue<MYSQL *> connQue_;  /*连接队列*/
};

#endif //MY_WEBSERVER_SQLCONNPOLL_H
