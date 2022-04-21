//
// Created by zzh on 2022/4/19.
//

#ifndef MY_WEBSERVER_SQLCONNRAII_H
#define MY_WEBSERVER_SQLCONNRAII_H

#include"sqlconnpoll.h"

/*
 * 资源在对象构造初始化 资源在对象析构时释放
 */
class SqlConnRAII {
public:
    /*
     * 因为这里需要修改指针的值，所以需要拆入二级指针，其实这里可以用指针的引用接收，这样只需要传入指针即可
     * 构造函数中获取连接
     */
    SqlConnRAII(MYSQL **sql, SqlConnPool *connpool) {
        assert(connpool);
        *sql = connpool->getConn();
        sql_ = *sql;
        connpool_ = connpool;
    }

    /*
     * 析构函数中释放
     */
    ~SqlConnRAII(){
        if(sql_){
            connpool_->freeConn(sql_);
        }
    }

private:
    MYSQL *sql_;  /*数据库连接*/
    SqlConnPool *connpool_;  /*连接池*/
};


#endif //MY_WEBSERVER_SQLCONNRAII_H
