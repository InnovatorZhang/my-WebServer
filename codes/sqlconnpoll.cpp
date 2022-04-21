//
// Created by zzh on 2022/4/20.
//
#include"sqlconnpoll.h"

/*
 * 私有的构造函数，单例模式防止类外创建SqlConnPool实例
 */
SqlConnPool::SqlConnPool() : useCount_(0), freeCount_(0) {

}

/*
 * 析构时释放所有资源
 */
SqlConnPool::~SqlConnPool() {
    closePool();
}

/*
 * 静态方法，方法内静态初始化可以保证线程安全，调用该函数返回这一个静态实例的引用
 */
SqlConnPool *SqlConnPool::instance() {
    static SqlConnPool connPool;

    return &connPool;
}

/*
 * 初始化数据库连接池
 */
void SqlConnPool::init(const char *host, int port, const char *user,
                       const char *pwd, const char *dbName, int connSize) {
    assert(connSize > 0);
    for (int i = 0; i < connSize; i++) {
        /*初始化以及配置一个sql连接*/
        MYSQL *sql = nullptr;
        sql = mysql_init(sql);
        if (!sql) {
            LOG_ERROR("Mysql init error!");
            assert(sql);
        }
        sql = mysql_real_connect(sql, host, user, pwd, dbName, port, nullptr, 0);
        if (!sql) {
            LOG_ERROR("MySql Connect error!");
        }
        /*将sql连接入队*/
        connQue_.push(sql);
    }
    /*初始化信号量值为数据池连接个数*/
    MAX_CONN_ = connSize;
    sem_init(&semId_, 0, MAX_CONN_);
}

/*
 * 获得数据库连接池中的一个连接
 */
MYSQL *SqlConnPool::getConn() {
    MYSQL *sql = nullptr;
    if (connQue_.empty()) {
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    /*将信号量减1*/
    sem_wait(&semId_);

    {
        /*RAII的应用，加锁局部块作用域*/
        std::lock_guard<std::mutex> locker(mtx_);
        sql = connQue_.front();
        connQue_.pop();
    }
    assert(sql);

    return sql;
}

/*
 * 释放一个数据库连接，重新将连接入池
 */
void SqlConnPool::freeConn(MYSQL *sql) {
    assert(sql);
    std::lock_guard<std::mutex> locker(mtx_);
    connQue_.push(sql);
    /*将信号量加1*/
    sem_post(&semId_);
}

/*
 * 关闭数据库连接池
 */
void SqlConnPool::closePool() {
    std::lock_guard<std::mutex> locker(mtx_);
    while (!connQue_.empty()) {
        auto sql = connQue_.front();
        connQue_.pop();
        mysql_close(sql);
    }
    mysql_library_end();
}

/*
 * 获取空闲连接的数量
 */
int SqlConnPool::getFreeConnCount() {
    std::lock_guard<std::mutex> locker(mtx_);

    return connQue_.size();
}




