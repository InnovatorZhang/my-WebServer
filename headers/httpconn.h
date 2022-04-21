//
// Created by zzh on 2022/4/19.
//

#ifndef MY_WEBSERVER_HTTPCONN_H
#define MY_WEBSERVER_HTTPCONN_H

#include<errno.h>
#include<stdlib.h>
#include<sys/uio.h>
#include<arpa/inet.h>
#include<sys/types.h>

#include"log.h"
#include"buffer.h"
#include"sqlconnRAII.h"
#include"httprequest.h"
#include"httpresponse.h"

class HttpConn {
public:
    HttpConn();

    ~HttpConn();

    void init(int sockfd, const sockaddr_in &addr);

    ssize_t read(int *saveErrno);

    ssize_t write(int *saveErrno);

    void close();

    int getFd() const;

    int getPort() const;

    const char *getIP() const;

    sockaddr_in getAddr() const;

    bool process();

    int toWriteBytes();

    bool isKeepAlive() const;

    static bool isET;  /*指示工作模式*/
    static const char *srcDir;  /*资源文件目录*/
    static std::atomic<int> userCount;  /*指示用户连接个数*/

private:
    int fd_;  /*socket对应的文件描述符*/
    bool isClose_;  /*指示工作状态*/
    int iovCnt_;  /*输出数据的个数，不在连续区域*/

    struct sockaddr_in addr_;  /*socket对应的地址*/
    struct iovec iov_[2];  /*代表输出哪些数据的结构体*/

    Buffer readBuff_;  /*读缓冲区*/
    Buffer writeBuff_;  /*写缓冲区*/

    HttpRequest request_;  /*包装的处理http请求的类*/
    HttpResponse response_;  /*包装的处理http回应的类*/
};

#endif //MY_WEBSERVER_HTTPCONN_H
