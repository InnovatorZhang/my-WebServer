//
// Created by zzh on 2022/4/19.
//

#ifndef MY_WEBSERVER_BLOCKQUEUE_H
#define MY_WEBSERVER_BLOCKQUEUE_H

#include<mutex>
#include<deque>
#include<assert.h>
#include<condition_variable>
#include<sys/time.h>

/*
 * 阻塞队列
 * 实现的线程安全的双端队列
 */
template<class T>
class BlockDeque {
public:
    explicit BlockDeque(size_t MaxCapacity = 1000);

    ~BlockDeque();

    void clear();

    bool empty();

    bool full();

    void close();

    size_t size();

    size_t capacity();

    T front();

    T back();

    void push_back(const T &item);

    void push_front(const T &item);

    bool pop(T &item);

    bool pop(T &item, int timeout);

    void flush();

private:
    std::deque<T> deq_;  /*包装的deque*/
    std::mutex mtx_;  /*互斥量*/
    std::condition_variable condConsumer_;  /*消费者条件变量*/
    std::condition_variable condProducer_;  /*生产者条件变量*/

    bool isClose_;  /*是否关闭*/
    size_t capacity_;  /*最大容量*/
};

/*
 * 构造函数，初始化队列容量
 */
template<typename T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity):capacity_(MaxCapacity), isClose_(false) {
    assert(MaxCapacity > 0);
}

/*
 * 析构函数中释放资源
 */
template<typename T>
BlockDeque<T>::~BlockDeque() {
    close();
}

/*
 * 清空队列，唤醒所有等待的事件
 */
template<typename T>
void BlockDeque<T>::close() {
    {
        std::lock_guard<std::mutex> locker(mtx_);
        deq_.clear();
        isClose_ = true;
    }
    condProducer_.notify_all();
    condConsumer_.notify_all();
}

/*
 * 唤醒一个消费者执行任务
 */
template<typename T>
void BlockDeque<T>::flush() {
    condConsumer_.notify_one();
}

/*
 * 清空队列
 */
template<typename T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}

/*
 * 获取首部的元素
 */
template<typename T>
T BlockDeque<T>::front() {
    std::lock_guard<std::mutex> locker(mtx_);

    return deq_.front();
}

/*
 * 获取尾部的元素
 */
template<typename T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locker(mtx_);

    return deq_.back();
}

/*
 * 获取队列大小
 */
template<typename T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mtx_);

    return deq_.size();
}

/*
 * 获取队列容量
 */
template<typename T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx_);

    return capacity_;
}

/*
 * 向队尾加入一个元素
 */
template<typename T>
void BlockDeque<T>::push_back(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    /*条件变量控制，如果元素超过了容量，需要阻塞等待消费者消费后才能加入*/
    while (deq_.size() >= capacity_) {
        /*生产者等待*/
        condProducer_.wait(locker);
    }
    deq_.push_back(item);
    /*唤醒一个消费者*/
    condConsumer_.notify_one();
}

/*
 * 向队头加入一个元素
 */
template<typename T>
void BlockDeque<T>::push_front(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    /*条件变量控制，如果元素超过了容量，需要阻塞等待消费者消费后才能加入*/
    while (deq_.size() >= capacity_) {
        /*生产者等待*/
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    /*唤醒一个消费者*/
    condConsumer_.notify_one();
}

/*
 * 返回队列是否为空
 */
template<typename T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx_);

    return deq_.empty();
}

/*
 * 返回队列是否满了
 */
template<typename T>
bool BlockDeque<T>::full() {
    std::lock_guard<std::mutex> locker(mtx_);

    return deq_.size() >= capacity_;
}

/*
 * 在队头位置弹出一个元素
 */
template<typename T>
bool BlockDeque<T>::pop(T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    /*如果队列为空，消费者条件变量等待，若关闭变量isClose为true这返回false*/
    while (deq_.empty()) {
        condConsumer_.wait(locker);
        if (isClose_) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    /*唤醒一个生产者*/
    condProducer_.notify_one();

    return true;
}

/*
 * 在队头位置弹出一个元素
 * 重载版
 * 规定了最大等待时间，若在规定时间内未等到则直接返回false
 */
template<typename T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    while (deq_.empty()) {
        if (condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) == std::cv_status::timeout) {
            return false;
        }
        if (isClose_) {
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    /*唤醒一个生产者*/
    condProducer_.notify_one();

    return true;
}
#endif //MY_WEBSERVER_BLOCKQUEUE_H
