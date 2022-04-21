//
// Created by zzh on 2022/4/19.
//

#ifndef MY_WEBSERVER_BUFFER_H
#define MY_WEBSERVER_BUFFER_H

#include<vector>
#include<atomic>
#include<cstring>
#include<iostream>
#include<assert.h>
#include<unistd.h>
#include<sys/uio.h>  /*readv*/

class Buffer {
public:
    Buffer(int initBufferSize = 1024);

    ~Buffer() = default;

    size_t writableBytes() const;

    size_t readableBytes() const;

    size_t prependableBytes() const;

    const char *peek() const;

    void ensureWritable(size_t len);

    void hasWritten(size_t len);

    void retrieve(size_t len);

    void retrieveUntil(const char *end);

    void retrieveAll();

    std::string retrieveAllToStr();

    const char *beginWriteConst() const;

    char *beginWrite();

    void append(const std::string &str);

    void append(const char *str, size_t len);

    void append(const void *data, size_t len);

    void append(const Buffer &buff);

    ssize_t readFd(int fd, int *saveErrno);

    ssize_t writeFd(int fd, int *saveErrno);

private:
    char *beginPtr_();

    const char *beginPtr_() const;

    void makeSpace_(size_t len);

    std::vector<char> buffer_;  /*vector实现的自增长缓冲区*/
    std::atomic<std::size_t> readPos_;  /*读指针所在的下表*/
    std::atomic<std::size_t> writePos_;  /*写指针所在的下表*/
};


#endif //MY_WEBSERVER_BUFFER_H
