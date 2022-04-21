//
// Created by zzh on 2022/4/19.
//

#ifndef MY_WEBSERVER_HTTPREQUEST_H
#define MY_WEBSERVER_HTTPREQUEST_H

#include<regex>
#include<string>
#include<unordered_map>
#include<unordered_set>
#include<errno.h>
#include<mysql/mysql.h>

#include<log.h>
#include<buffer.h>
#include<sqlconnpoll.h>
#include<sqlconnRAII.h>

class HttpRequest {
public:
    /*指示解析到请求头的哪一部分的枚举变量*/
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADER,
        BODY,
        FINISH
    };

    /*指示解析结果的枚举变量*/
    enum HTTP_CODE {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

    HttpRequest();

    /*
     * 生成默认析构函数
     */
    ~HttpRequest() = default;

    void init();

    bool parse(Buffer &buff);

    std::string path() const;

    std::string &path();

    std::string method() const;

    std::string version() const;

    std::string getPost(const std::string &key) const;

    std::string getPost(const char *key) const;

    bool isKeepAlive() const;

private:
    static int convertHex(char ch);

    static bool userVerify(const std::string &name, const std::string &pwd, bool isLogin);

    bool parseRequestLine_(const std::string &line);

    void parseHeader_(const std::string &line);

    void parseBody_(const std::string &line);

    void parsePath_();

    void parsePost_();

    void parseFromUrlencoded_();

    PARSE_STATE state_;  /*状态机状态*/
    std::string method_, path_, version_, body_;  /*http请求头中信息对应的字符串*/
    std::unordered_map<std::string, std::string> header_;  /*请求头字段转化为hashmap*/
    std::unordered_map<std::string, std::string> post_;  /*保存请求体中的信息，以键值对的方式，因为本服务器接受的是application/x-www-form-urlencoded这种表单形式*/

    static const std::unordered_set<std::string> DEFAULT_HTML;  /*静态常量，默认的返回页面的地址*/
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;  /*静态变量，保存默认的HTML标签*/
};


#endif //MY_WEBSERVER_HTTPREQUEST_H
