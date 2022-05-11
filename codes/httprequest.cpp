//
// Created by zzh on 2022/4/20.
//
#include"httprequest.h"

/*
 * 保存默认界面名字的静态变量，所有对以下界面的请求都会加上 .html 后缀
 */
const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML{
        "/index", "/register", "/login",
        "/welcome", "/video", "/picture",};

/*
 * 静态变量，保存默认的HTML标签
 */
const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG{
        {"/register.html", 0},
        {"/login.html",    1}};

/*
 * 构造函数中初始化
 */
HttpRequest::HttpRequest() {
    init();
}

/*
 * 初始化相关变量
 */
void HttpRequest::init() {
    /*所有变量清空*/
    method_ = path_ = version_ = body_ = "";
    /*状态重置到解析请求行状态*/
    state_ = REQUEST_LINE;
    /*清空以下变量*/
    header_.clear();
    post_.clear();
}

/*
 * 返回客户端是否有长连接请求
 */
bool HttpRequest::isKeepAlive() const {
    if (header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

/*
 * 将16进制数转为十进制数
 */
int HttpRequest::convertHex(char ch) {
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    return ch - '0';
}

/*
 * 返回解析请求行得到的state_变量
 */
HttpRequest::PARSE_STATE HttpRequest::state() const {
    return state_;
}

/*
 * 返回解析请求行得到的path_变量
 */
std::string &HttpRequest::path() {
    return path_;
}

/*
 * 上面函数的重载版本
 */
std::string HttpRequest::path() const {
    return path_;
}

/*
 * 返回解析请求行得到的method_变量
 */
std::string HttpRequest::method() const {
    return method_;
}

/*
 * 返回解析请求行得到的version_变量
 */
std::string HttpRequest::version() const {
    return version_;
}

/*
 * 返回请求头中指定键对应的数据
 */
std::string HttpRequest::getPost(const std::string &key) const {
    assert(key != "");
    if (post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

/*
 * 返回请求头中指定键对应的数据，重载版本
 */
std::string HttpRequest::getPost(const char *key) const {
    assert(key != nullptr);
    if (post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

/*
 * 使用正则库解析请求行
 * GET请求的请求行示例
 * GET /562f25980001b1b106000338.jpg HTTP/1.1
 * POST请求的请求行示例
 * POST / HTTP1.1
 */
bool HttpRequest::parseRequestLine_(const std::string &line) {
    /*指定匹配规则，^表示行开始，$表示行尾，[^ ]表示匹配非空格,括号()括住的代表我们需要得到的字符串，最后会送入submatch中*/
    std::regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch subMatch;
    if (std::regex_match(line, subMatch, pattern)) {
        /*解析请求行获取需要的信息*/
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        /*切换到下一个状态，即解析请求头*/
        state_ = HEADER;
        return true;
    }
    LOG_ERROR("RequestLine error! %s", line.c_str());

    return false;
}

/*
 * 使用正则库解析请求头
 * 请求头由如下键值对组成，由 : 分割键值对
 * Host:img.mukewang.com
 */
void HttpRequest::parseHeader_(const std::string &line) {
    /*指定匹配规则，^表示行开始，[^ ]表示匹配非空格，括号()括住的代表我们需要得到的字符串，最后会送入submatch中*/
    std::regex pattern("^([^:]*): ?(.*)$");
    std::smatch subMatch;
    if (std::regex_match(line, subMatch, pattern)) {
        /*将解析出来的信息以键值对的形式存入unordered_map里面*/
        header_[subMatch[1]] = subMatch[2];
    } else {
        /*如果没有匹配到，说明进入到了请求头后的空行，若是post请求，则将state_设置为BODY进入解析请求体的流程*/
        /*调用此函数的函数通过可读的数据量判断是POST还是GET请求，若是GET请求则直接将state_设置为FINISH状态结束解析，这里后面需要改一下逻辑，*/
        state_ = BODY;
    }
}

/*
 * 解析请求体
 */
bool HttpRequest::parseBody_(const std::string &line) {
    /*将line赋值给类变量body_*/
    body_ = line;
    /*调用ParsePost_函数解析POST中带的请求体数据*/
    if (!parsePost_()) {
        return false;
    }
    /*将状态置为FINISH，指示解析完成*/
    state_ = FINISH;

    LOG_DEBUG("Body: %s, len: %d", line.c_str(), line.size());
    return true;
}

/*
 * 解析请求体中的内容
 * 目前只能解析application/x-www-form-urlencoded此种格式，以后可以根据需要添加格式，比如json
 * 如果请求方式时POST Content-Type为application/x-www-form-urlencoded就可以解析，表示以键值对的数据格式提交
 * 使用post的形式进行登录信息的传输
 */
bool HttpRequest::parsePost_() {
    if (body_.size() < atol(header_["Content-Length"].c_str())) {
        /*判断post数据是否接受完整，未接收完则返回false，表示继续请求*/
        return false;
    }
    /*以后可以添加其他种类的Content-Type的支持*/
    if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        /*将请求体中的内容解析到post_变量中*/
        parseFromUrlencoded_();
        /*用户是否请求的默认DEFAULT_HTML_TAG（登录与注册）网页*/
        if (DEFAULT_HTML_TAG.count(path_)) {
            /*获取登录与注册对应的标识*/
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if (tag == 0 || tag == 1) {
                /*通过标识确定用户请求的是登录还是注册*/
                bool isLogin = (tag == 1);
                if (userVerify(post_["username"], post_["password"], isLogin)) {
                    /*验证成功，进入下一步，设置为成功页面*/
                    path_ = "/welcome.html";
                } else {
                    /*验证失败，设置返回错误页面*/
                    path_ = "/error.html";
                }
            }
        }
    }
    return true;
}

/*
 * 从请求体中的application/x-www-form-urlencoded类型信息提取信息
 */
void HttpRequest::parseFromUrlencoded_() {
    if (body_.size() == 0) {
        return;
    }

    std::string key, value, temp;
    int num = 0;
    int n = body_.size();
    int i = 0;

    for (; i < n; i++) {
        char ch = body_[i];
        switch (ch) {
            case '=':
                /*等号之前是key*/
                key = temp;
                temp.clear();
                break;
            case '+':
                /* + 号改为 空格 ，因为浏览器会将空格编码为+号*/
                temp += ' ';
                break;
            case '%':
                /*浏览器会将非字母字母字符，encode成百分号+其ASCII码的十六进制*/
                /*%后面跟的是十六进制码,将十六进制转化为10进制*/
                /*作者这里的实现是有一点问题，这里应该是转成ASCII码对应的字符的，而不是转换成对应的十进制字符，况且转化后的十进制也只能局限在0-99范围内*/
                num = convertHex(body_[i + 1]) * 16 + convertHex(body_[i + 2]);
                /*根据ascii码转换为字符*/
                temp += static_cast<char>(num);
                /*向后移动两个位置*/
                i += 2;
                break;
            case '&':
                /*&号前是val*/
                value = temp;
                temp.clear();
                /*添加键值对*/
                post_[key] = value;
                LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
                break;
            default:
                temp += ch;
                break;
        }
    }
    /*获取最后一个键值对*/
    if (post_.count(key) == 0) {
        value = temp;
        post_[key] = value;
    }
}

/*
 * 验证用户
 */
bool HttpRequest::userVerify(const std::string &name, const std::string &pwd, bool isLogin) {
    /*密码或用户名为空，直接错误*/
    if (name == "" || pwd == "") {
        return false;
    }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());

    /*获取一个sql连接*/
    MYSQL *sql;
    /*RAII*/
    SqlConnRAII sqlConnRaii(&sql, SqlConnPool::instance());
    assert(sql);

    /*初始化一系列相关变量*/
    bool flag = false;
    unsigned int j = 0;
    char order[256] = {0};
    MYSQL_FIELD *fields = nullptr;
    MYSQL_RES *res = nullptr;

    /*如果是注册，那么将flag置为true*/
    if (!isLogin) {
        flag = true;
    }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if (mysql_query(sql, order)) {
        mysql_free_result(res);
        return false;
    }
    /*从表中检索完整的结果集*/
    res = mysql_store_result(sql);
    /*返回结果集中的列数*/
    j = mysql_num_fields(res);
    /*返回所有字段结构的数组*/
    fields = mysql_fetch_fields(res);

    /*从结果集中获取下一行*/
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        std::string password(row[1]);
        /*登录验证*/
        if (isLogin) {
            if (pwd == password) {
                flag = true;
            } else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        } else {
            flag = false;
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if (!isLogin && flag) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        /*插入数据库，用户注册成功*/
        if (mysql_query(sql, order)) {
            LOG_DEBUG("Insert error!");
            flag = false;
        }
        flag = true;
    }

    LOG_DEBUG("UserVerify success!!");
    return flag;
}

/*
 * 将客户端传来的path变量添加完整，以目录结束的路径添加上默认页面
 * 在DEFAULT_HTML中的界面添加上文件后缀名
 */
void HttpRequest::parsePath_() {
    if (path_ == "/") {
        path_ = "/index.html";
    } else {
        for (auto &item: DEFAULT_HTML) {
            if (item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

/*
 * 解析收到的http请求内容
 * 作者实现有点问题，需要改进，可以按照tinywebserver的解析方式进行改进
 */
HttpRequest::HTTP_CODE HttpRequest::parse(Buffer &buff) {
    /*在请求头的每一行结尾都有下面这两个字符*/
    const char CRLF[] = "\r\n";
    /*若没有内容可以被读取，直接返回false，这里是有问题的，应该返回一个NO_REQUEST状态，调用函数接受到该状态之后重新修改EPOLL事件注册一个EPOLLIN事件*/
    if (buff.readableBytes() <= 0) {
        return NO_REQUEST;
    }

    /*状态机方式解析http请求头*/
    while (buff.readableBytes() && state_ != FINISH) {
        /*首先通过查找CRLF标志找到一行的结尾，用于在序列 A 中查找序列 B 第一次出现的位置,lineEnd指针会指向\r位置，也就是有效字符串的后一个位置，当序列A中没有序列B中的字符时，会返回序列A的尾后指针*/
        const char *lineEnd = std::search(buff.peek(), buff.beginWriteConst(), CRLF, CRLF + 2);
        /*根据查找到的行尾的位置初始化一个行字符串*/
        std::string line(buff.peek(), lineEnd);

        /*开始解析*/
        switch (state_) {
            case REQUEST_LINE:
                /*首先解析请求行*/
                if (!parseRequestLine_(line)) {
                    /*如果失败的话，false*/
                    return BAD_REQUEST;;
                }
                /*将客户端传来的path变量添加完整,目录加上默认页面，没有后缀的指定文件加上后缀*/
                parsePath_();
                break;
            case HEADER:
                /*解析完请求行之后，state_变量会被置为HEADERS，所以会跳到此处处理请求头部*/
                parseHeader_(line);
                /*这里逻辑有点问题，因为可能会存在请求未接受完整的情况，所以后面可以改成根据method判断是否跳转到FINISH阶段*/
                /*因为在ParseHeader_中请求头解析到空行时会将state_置为body，所以这里的判断条件可以改为 if(state_==body && method_==GET){state_=FINISH;}*/
                /*这样就可以实验GET与POST的对应处理*/
                if (state_ == BODY && method_ == "GET") {
                    state_ = FINISH;
                    buff.retrieveAll();
                    return GET_REQUEST;
                }
                break;
            case BODY:
                /*解析完请求行之后，若是POST请求则会进入解析请求体这一步*/
                if (!parseBody_(line)) {
                    return NO_REQUEST;
                }
                buff.retrieveAll();
                return GET_REQUEST;
            default:
                return INTERNAL_ERROR;
        }
        if (lineEnd == buff.beginWrite()) {
            break;
        }
        buff.retrieveUntil(lineEnd + 2);
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    /*最后最好直接返回NO_REQUEST状态，表示如果执行到这一部分，说明请求没有接受完整，需要继续接受请求，若是请求完整的在之前就会return出while*/
    return NO_REQUEST;
}