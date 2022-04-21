//
// Created by zzh on 2022/4/20.
//
#include"epoller.h"

/*
 * 构造函数
 * 初始化列表中初始化epoll文件描述符与最大监听数量
 */
Epoller::Epoller(int maxEvent) : epollFd_(epoll_create(512)), events_(maxEvent) {
    assert(epollFd_ >= 0 && events_.size() > 0);
}

/*
 * 释放对应资源
 */
Epoller::~Epoller() {
    close(epollFd_);
}

/*
 * 添加EPOLL监听事件
 */
bool Epoller::addFd(int fd, uint32_t events) {
    if (fd < 0) {
        return false;
    }
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;

    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
}

/*
 * 修改监听事件类型或是重新添加监听事件因为之前设置了EPOLLONESHOT，表示指挥监听一次
 */
bool Epoller::modFd(int fd, uint32_t events) {
    if (fd < 0) {
        return false;
    }
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;

    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

/*
 * 删除对指定描述符的监听事件
 */
bool Epoller::delFd(int fd) {
    if (fd < 0) {
        return false;
    }
    epoll_event ev = {0};

    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
}

/*
 * 包装epoll等待函数
 */
int Epoller::wait(int timeoutMS) {
    /*因为events_是vector，所以应该取events_[0]数据所在的地址才对*/
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMS);
}

/*
 *获取对应位置文件描述符的接口
 */
int Epoller::getEventFd(size_t i) const {
    assert(i < events_.size() && i >= 0);

    return events_[i].data.fd;
}

/*
 *获取对应位置epoll事件的接口
 */
uint32_t Epoller::getEvents(size_t i) const {
    assert(i < events_.size() && i >= 0);

    return events_[i].events;
}












