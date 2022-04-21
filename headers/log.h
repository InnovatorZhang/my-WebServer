//
// Created by zzh on 2022/4/19.
//

#ifndef MY_WEBSERVER_LOG_H
#define MY_WEBSERVER_LOG_H

#include<mutex>
#include<string>
#include<thread>
#include<string.h>
#include<stdarg.h>  /*va_start va_end*/
#include<assert.h>
#include<sys/stat.h>
#include<sys/time.h>

#include"buffer.h"
#include"blockqueue.h"


class Log {
public:
    void init(int level = 1, const char *path = "./log",
              const char *suffix = ".log", int maxQueueSize = 1024);

    static Log *instance();

    static void flushLogThread();

    void write(int level, const char *format, ...);

    void flush();

    int getLevel();

    void setLevel(int level);

    bool isOpen();

private:
    Log();

    virtual ~Log();

    void appendLogLevelTitle_(int level);

    void asyncWrite_();

    static const int LOG_PATH_LEN = 256;  /*最大log文件路径长度*/
    static const int LOG_NAME_LEN = 256;  /*最大log文件名长度*/
    static const int MAX_LINES = 50000;  /*log文件最大行数，超过这个数量就要单独划分文件*/

    const char *path_;  /*log文件路径*/
    const char *suffix_;  /*文件后缀名*/

    int lineCount_;  /*当前log文件行数*/
    int today_;  /*本月的哪一天*/
    bool isOpen_;  /*是否开启*/
    int level_;  /*记录级别*/
    bool isAsync_;  /*是否异步模式写入log*/

    FILE *fp_;  /*log文件指针*/
    Buffer buff_;  /*缓冲区*/

    std::unique_ptr<BlockDeque<std::string>> deque_;  /*log信息阻塞队列*/
    std::unique_ptr<std::thread> writeThread_;  /*写入日志的线程*/
    std::mutex mtx_;  /*互斥量*/
};

/*
 * 定义log日志相关的宏
 * 只会像文件写入规定level以下的log信息 log->GetLevel() <= level
 * 为什么写成do while(0); ?
 */
#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::instance();\
        if (log->isOpen() && log->getLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);

#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif //MY_WEBSERVER_LOG_H
