//
// Created by zzh on 2022/4/19.
//

#ifndef MY_WEBSERVER_WEBSERVER_H
#define MY_WEBSERVER_WEBSERVER_H

#include<unordered_map>
#include<fcntl.h>
#include<errno.h>
#include<unistd.h>
#include<assert.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#include"log.h"
#include"epoller.h"
#include"httpconn.h"
#include"heaptimer.h"
#include"threadpool.h"
#include"sqlconnpoll.h"
#include"sqlconnRAII.h"


class WebServer {
public:
    WebServer(int port, int trigMode, int timeoutMS, bool optLinger,
              int sqlPort, const char *sqlUser, const char *sqlPwd,
              const char *dbName, int connPoolNum, int threadNum,
              bool openLog, int logLevel, int logQueSize);

    ~WebServer();

    void start();

private:
    static int setFdNonblock(int fd);

    bool initSocket_();

    void initEventMode_(int trigMode);

    void addClient_(int fd, sockaddr_in addr);

    void dealListen_();

    void dealWrite_(HttpConn *client);

    void dealRead_(HttpConn *client);

    void sendError_(int fd, const char *info);

    void extentTime_(HttpConn *client);

    void closeConn_(HttpConn *client);

    void onRead_(HttpConn *client);

    void onWrite_(HttpConn *client);

    void onProcess_(HttpConn *client);

    static const int MAX_FD = 65536;  /*最大文件描述符数量*/

    int port_;  /*监听的端口*/
    int timeoutMS_;  /*毫秒MS*/
    int listenFd_;  /*监听的文件描述符*/
    bool openLinger_;  /*SO_LINGER将决定系统如何处理残存在套接字发送队列中的数据。处理方式无非两种：丢弃或者将数据继续发送至对端，优雅关闭连接。*/
    bool isClose_;  /*指示InitSocket操作是否成功*/
    char *srcDir_;  /*资源文件目录*/

    uint32_t listenEvent_;  /*监听描述符上的epoll事件*/
    uint32_t connEvent_;  /*监听描述符上的epoll事件*/

    std::unique_ptr<HeapTimer> timer_;  /*基于小根堆的定时器*/
    std::unique_ptr<ThreadPool> threadPool_;  /*线程池*/
    std::unique_ptr<Epoller> epoller_;  /*epoller变量*/
    std::unordered_map<int,HttpConn> users_;  /*使用hash实现的文件描述符，这样可以用一个实例化一个，不用一开始就初始化很多个*/
};

#endif //MY_WEBSERVER_WEBSERVER_H
