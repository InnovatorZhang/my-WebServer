//
// Created by zzh on 2022/4/20.
//
#include"log.h"


/*
 * 构造函数中初始化一系列变量
 */
Log::Log() : lineCount_(0), isAsync_(false), writeThread_(nullptr),
             deque_(nullptr), today_(0), fp_(nullptr) {

}

/*
 * 在析构时将所有log信息写入文件，再关闭写入线程
 */
Log::~Log() {
    if (writeThread_ && writeThread_->joinable()) {
        while (!deque_->empty()) {
            deque_->flush();
        };
        deque_->close();
        writeThread_->join();
    }
    if (fp_) {
        std::lock_guard<std::mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

/*
 * 返回文件是否打开
 */
bool Log::isOpen() {
    return isOpen_;
}

/*
 * 返回日志级别
 */
int Log::getLevel() {
    std::lock_guard<std::mutex> locker(mtx_);

    return level_;
}

/*
 * 设置日志级别
 */
void Log::setLevel(int level) {
    std::lock_guard<std::mutex> locker(mtx_);
    level_ = level;
}

/*
 * 向缓冲区中加入log的级别信息
 */
void Log::appendLogLevelTitle_(int level) {
    switch (level) {
        case 0:
            buff_.append("[debug]: ", 9);
            break;
        case 1:
            buff_.append("[info]:  ", 9);
            break;
        case 2:
            buff_.append("[warn]:  ", 9);
            break;
        case 3:
            buff_.append("[error]: ", 9);
            break;
        default:
            buff_.append("[info]:  ", 9);
            break;
    }
}

/*
 * 清空缓冲区，将当前的数据都写入到文件中去
 */
void Log::flush() {
    if (isAsync_) {
        /*如果是异步模式，需要将整个队列都写入*/
        deque_->flush();
    }
    /*刷新文件缓冲区*/
    fflush(fp_);
}

/*
 * 异步取出队列中数据，写入文件中
 * 不停循环取出数据写入文件，只有队列为空时会阻塞等待
 */
void Log::asyncWrite_() {
    std::string str = "";
    while (deque_->pop(str)) {
        std::lock_guard<std::mutex> locker(mtx_);
        fputs(str.c_str(), fp_);
    }
}

/*
 * 局部静态变量，线程安全的初始化方式
 * 单例模式，返回一个LOG对象的引用
 */
Log *Log::instance() {
    static Log instance;

    return &instance;
}

/*
 * 线程的执行函数
 */
void Log::flushLogThread() {
    Log::instance()->asyncWrite_();
}

/*
 * 初始化Log类设置
 */
void Log::init(int level, const char *path, const char *suffix, int maxQueueSize) {
    isOpen_ = true;
    level_ = level;
    /*如果消息队列大于0，说明启用了异步写入log，那就要初始化异步写入所需的变量*/
    if (maxQueueSize > 0) {
        /*表示开启异步写入*/
        isAsync_ = true;
        if (!deque_) {
            /*获取deque_的unique智能指针*/
            deque_ = std::make_unique<BlockDeque<std::string>>();
            /*获取writeThread_的unique智能指针*/
            writeThread_ = std::make_unique<std::thread>(flushLogThread);
        }
    } else {
        /*表示不开启异步写入*/
        isAsync_ = false;
    }

    /*从第一行开始*/
    lineCount_ = 0;
    /*获取当前时间，保存到变量t中*/
    time_t timer = time(nullptr);
    struct tm *sysTime = localtime(&timer);
    struct tm t = *sysTime;
    /*初始化文件路径以及后缀名*/
    path_ = path;
    suffix_ = suffix;
    /*根据 路径+时间+后缀名创建log文件*/
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s",
             path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
    /*将日期保存到today_变量中*/
    today_ = t.tm_mday;

    /*创建文件，使用互斥量保证线程安全*/
    {
        std::lock_guard<std::mutex> locker(mtx_);
        /*确保buff回收完全*/
        if (fp_) {
            flush();
            fclose(fp_);
        }
        /*创建文件目录与文件*/
        fp_ = fopen(fileName, "a");
        /*如果未创建成功，说明没有对应的目录，创建目录后再创建文件即可*/
        if (!fp_) {
            /*创建目录*/
            mkdir(path_, 0777);
            /*创建文件*/
            fp_ = fopen(fileName, "a");
        }
        assert(fp_ != nullptr);
    }
}

/*
 * 向log文件中写入log信息
 */
void Log::write(int level, const char *format, ...) {
    /*获取当前时间*/
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;

    /* ... 使用的可变参数列表*/
    va_list vaList;
    /*如果日期变了，也就是到第二天了，或者当前log文件行数达到规定的最大值时都需要创建一个新的log文件*/
    if (today_ != t.tm_mday || (lineCount_ && (lineCount_ % MAX_LINES) == 0)) {
        /*最终文件路径存储变量*/
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        /*根据时间获取文件名*/
        snprintf(tail, 36, "%04d_%02d_%02d",
                 t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        /*如果日期变化了，那么需要将toDay变量更改为当前日期*/
        if (today_ != t.tm_mday) {
            /*拼接 path_ tail suffix_ 获取最终文件名*/
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            /*更改today变量*/
            today_ = t.tm_mday;
            /*重置文件行计数变量*/
            lineCount_ = 0;
        } else {
            /*进入到此分支表示文件行数超过了最大行数，需要分出第二个log文件来存储今日的文件*/
            /*文件名由 path_  tail (lineCount_  / MAX_LINES) suffix_ 组成*/
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_,
                     tail, (lineCount_ / MAX_LINES), suffix_);
        }

        /*创建文件时上锁，保证线程安全*/
        std::unique_lock<std::mutex> locker(mtx_);
        /*将缓冲区的数据写入到之前的文件中*/
        flush();
        /*关闭上一个文件*/
        fclose(fp_);
        /*根据上面操作得到的文件名创建新的log文件，将指针赋值给fp_变量*/
        fp_ = fopen(newFile, "a");
        assert(fp_ != nullptr);
    }

    {
        /*上锁，保证线程安全*/
        std::unique_lock<std::mutex> locker(mtx_);
        /*增加文件行数指示变量*/
        lineCount_++;
        /*组装信息至缓冲区buff中*/
        int n = snprintf(buff_.beginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                         t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                         t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        /*移动缓冲区的指针，表示写了多少字节数据到缓冲区中*/
        buff_.hasWritten(n);
        /*向缓冲区中写入日志级别信息*/
        appendLogLevelTitle_(level);

        /*根据用户传入的参数，向缓冲区添加数据*/
        va_start(vaList, format);
        int m = vsnprintf(buff_.beginWrite(), buff_.writableBytes(), format, vaList);
        va_end(vaList);
        /*移动缓冲区的指针，表示写了多少字节数据到缓冲区中*/
        buff_.hasWritten(m);
        /*行尾写入换行符*/
        buff_.append("\n\0", 2);

        /*根据变量选择是否异步写入*/
        if (isAsync_ && deque_ && !deque_->full()) {
            /*如果以上三个条件都满足，那么进行异步写入*/
            deque_->push_back(buff_.retrieveAllToStr());
        } else {
            /*否则直接写入至文件*/
            fputs(buff_.peek(), fp_);
        }
        /*回收所有空间*/
        buff_.retrieveAll();
    }
}












