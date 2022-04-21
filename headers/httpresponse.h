//
// Created by zzh on 2022/4/19.
//

#ifndef MY_WEBSERVER_HTTPRESPONSE_H
#define MY_WEBSERVER_HTTPRESPONSE_H

#include<unordered_map>
#include<fcntl.h>
#include<unistd.h>
#include<sys/stat.h>
#include<sys/mman.h>  /*mmap,munmap*/

#include"log.h"
#include"buffer.h"


class HttpResponse {
public:
    HttpResponse();

    ~HttpResponse();

    void init(const std::string &srcDir, std::string &path, bool isKeepAlive = false, int code = -1);

    void makeResponse(Buffer &buff);

    void unmapFile();

    char *file();

    size_t fileLen() const;

    void errorContent(Buffer &buff, std::string message);

    int code() const;

private:
    void addStateLine_(Buffer &buff);

    void addHeader_(Buffer &buff);

    void addContent_(Buffer &buff);

    void errorHtml_();

    std::string getFileType_();

    int code_;  /*返回码*/
    bool isKeepAlive_;  /*是否保持长连接*/
    char *mmFile_;  /*发送的文件名*/
    struct stat mmFileStat_;  /*发送的文件信息*/
    std::string path_;  /*返回的文件路径*/
    std::string srcDir_;  /*项目根目录*/

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;  /*返回类型键值对*/
    static const std::unordered_map<int, std::string> CODE_STATUS;  /*状态码键值对*/
    static const std::unordered_map<int, std::string> CODE_PATH;  /*错误码与页面对应关系*/
};


#endif //MY_WEBSERVER_HTTPRESPONSE_H
