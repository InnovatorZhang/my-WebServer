//
// Created by zzh on 2022/4/19.
//

#ifndef MY_WEBSERVER_HEAPTIMER_H
#define MY_WEBSERVER_HEAPTIMER_H

#include<queue>
#include<chrono>
#include<algorithm>
#include<functional>
#include<unordered_map>
#include<time.h>
#include<assert.h>
#include<arpa/inet.h>

#include"log.h"

typedef std::function<void()> TimeoutCallBack;  /*定义的functional对象，接受一个bind函数绑定的函数对象*/
typedef std::chrono::high_resolution_clock Clock;  /*获取时间的类*/
typedef std::chrono::milliseconds MS;  /*毫秒*/
typedef Clock::time_point TimeStamp;  /*时间戳*/

/*
 * 定义的绑定文件描述符，超时时间，删除函数的结构体
 * 同时还重载了 < 运算符
 */
struct TimerNode {
    int id;  /*节点对应的文件描述符*/
    TimeStamp expires;  /*超时时间，时间戳*/
    TimeoutCallBack cb;  /*回调函数，这里接受的是删除节点后的对应操作，WebServer::CloseConn_*/
    /*重载的比较运算符，方便比较时间*/
    bool operator<(const TimerNode &t) {
        return expires < t.expires;
    }
};


class HeapTimer {
public:
    HeapTimer();

    ~HeapTimer();

    void adjust(int id, int newExpires);

    void add(int id, int timeout, const TimeoutCallBack &cb);

    void doWork(int id);

    void clear();

    void tick();

    void pop();

    int getNextTick();

private:
    void del_(size_t i);

    void siftup_(size_t i);

    bool siftdown_(size_t index, size_t n);

    void swapNode_(size_t i, size_t j);

    std::vector<TimerNode> heap_;  /*使用vector实现的堆*/
    std::unordered_map<int, size_t> ref_;  /*使用hash方便确定节点是否存在，O(1)*/
};


#endif //MY_WEBSERVER_HEAPTIMER_H
