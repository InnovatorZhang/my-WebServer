//
// Created by zzh on 2022/4/20.
//
#include"heaptimer.h"

/*
 * 构造函数中预申请一些空间
 */
HeapTimer::HeapTimer() {
    heap_.reserve(64);
}

/*
 * 析构函数中清空空间
 */
HeapTimer::~HeapTimer() {
    clear();
}

/*
 * 调整堆中元素
 */
void HeapTimer::siftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    size_t j = (i - 1) / 2;
    while(j >= 0) {
        if(heap_[j] < heap_[i]) { break; }
        swapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

/*
 * 交换堆中两个节点
 */
void HeapTimer::swapNode_(size_t i, size_t j) {
    assert(i >= 0 && i < heap_.size());
    assert(j >= 0 && j < heap_.size());
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;
}

/*
 * 调整堆中元素
 */
bool HeapTimer::siftdown_(size_t index, size_t n) {
    assert(index >= 0 && index < heap_.size());
    assert(n >= 0 && n <= heap_.size());
    size_t i = index;
    size_t j = i * 2 + 1;
    while(j < n) {
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++;
        if(heap_[i] < heap_[j]) break;
        swapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

/*
 * 为指定文件描述符添加一个定时器到堆中
 */
void HeapTimer::add(int id, int timeout, const TimeoutCallBack& cb) {
    assert(id >= 0);
    size_t i;
    if(ref_.count(id) == 0) {
        /*新节点：堆尾插入，调整堆*/
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({id, Clock::now() + MS(timeout), cb});
        siftup_(i);
    }
    else {
        /*已有结点：调整堆*/
        i = ref_[id];
        heap_[i].expires = Clock::now() + MS(timeout);
        heap_[i].cb = cb;
        if(!siftdown_(i, heap_.size())) {
            siftup_(i);
        }
    }
}

/*
 * 删除指定id结点，并触发回调函数，本项目中未用到
 */
void HeapTimer::doWork(int id) {
    /*删除指定id结点，并触发回调函数*/
    if(heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id];
    TimerNode node = heap_[i];
    node.cb();
    del_(i);
}

/*
 * 删除指定节点
 */
void HeapTimer::del_(size_t index) {
    /* 删除指定位置的结点 */
    assert(!heap_.empty() && index >= 0 && index < heap_.size());
    /* 将要删除的结点换到队尾，然后调整堆 */
    size_t i = index;
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if(i < n) {
        swapNode_(i, n);
        if(!siftdown_(i, n)) {
            siftup_(i);
        }
    }
    /*队尾元素删除*/
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

/*
 * 调整当前节点的过期时间为 当前时间+timeout
 */
void HeapTimer::adjust(int id, int timeout) {
    /* 调整指定id的结点 */
    assert(!heap_.empty() && ref_.count(id) > 0);
    heap_[ref_[id]].expires = Clock::now() + MS(timeout);;
    siftdown_(ref_[id], heap_.size());
}

/*
 * 删除队中所有的超时节点
 */
void HeapTimer::tick() {
    /* 清除超时结点 */
    if(heap_.empty()) {
        return;
    }
    while(!heap_.empty()) {
        TimerNode node = heap_.front();
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
            break;
        }
        node.cb();
        pop();
    }
}

/*
 * 删除第一个节点
 */
void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);
}

/*
 * 清空队中元素
 */
void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}

/*
 * 删除所有超时节点，返回最近一个超时节点的超时时间
 */
int HeapTimer::getNextTick() {
    /*删除超时节点*/
    tick();
    size_t res = -1;
    if(!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}
