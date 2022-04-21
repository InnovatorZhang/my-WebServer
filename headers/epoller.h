//
// Created by zzh on 2022/4/19.
//

#ifndef MY_WEBSERVER_EPOLLER_H
#define MY_WEBSERVER_EPOLLER_H

#include<vector>
#include<fcntl.h>  /*fcntl*/
#include<errno.h>
#include<unistd.h>
#include<assert.h>
#include<sys/epoll.h>

class Epoller {
public:
    explicit Epoller(int maxEvent = 1024);

    ~Epoller();

    bool addFd(int fd, uint32_t events);

    bool modFd(int fd, uint32_t events);

    bool delFd(int fd);

    int wait(int timeoutMS = -1);

    int getEventFd(size_t i) const;

    uint32_t getEvents(size_t i) const;

private:
    int epollFd_;  /*EPOLL自己对应的文件描述符*/

    std::vector<struct epoll_event> events_;  /*EPOLL事件组成的向量*/
};

#endif //MY_WEBSERVER_EPOLLER_H
