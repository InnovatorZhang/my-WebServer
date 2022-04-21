//
// Created by zzh on 2022/4/20.
//
#include"webserver.h"

/*
 * 构造函数中初始化程序需要用到的资源
 */
WebServer::WebServer(int port, int trigMode, int timeoutMS, bool optLinger,
                     int sqlPort, const char *sqlUser, const char *sqlPwd,
                     const char *dbName, int connPoolNum, int threadNum,
                     bool openLog, int logLevel, int logQueSize) :
        port_(port), openLinger_(optLinger), timeoutMS_(timeoutMS), isClose_(false),
        timer_(new HeapTimer()), threadPool_(new ThreadPool(threadNum)), epoller_(new Epoller()) {
    /*获取资源目录*/
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    strncat(srcDir_, "/resources/", 16);

    /*初始化http连接类的静态变量值以及数据库连接池*/
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;
    SqlConnPool::instance()->init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    /*根据参数设置连接事件与监听事件的出发模式LT或ET*/
    initEventMode_(trigMode);

    /*初始化监听套接字*/
    if (!initSocket_()) {
        isClose_ = true;
    }
    /*日志开关*/
    if (openLog) {
        /*初始化LOG类设置*/
        Log::instance()->init(logLevel, "./log", ".log", logQueSize);
        if (isClose_) {
            LOG_ERROR("=================Server init error!===================");
        } else {
            LOG_INFO("=============Server init================");
            LOG_INFO("Port: %d,OpenLinger: %s", port, optLinger ? "true" : "false");
            LOG_INFO("Listen Mode: %s, Conn Mode: %s", (listenEvent_ & EPOLLET ? "ET" : "LT"),
                     (connEvent_ & EPOLLET ? "ET" : "LT"));
            LOG_INFO("Log level: %d", logLevel);
            LOG_INFO("srcDir: %s", srcDir_);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

/*
 * 析构函数中关闭连接以及申请的资源
 */
WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
    SqlConnPool::instance()->closePool();
}

/*
 * 初始化事件工作模式
 * EPOLLIN：表示对应的文件描述符可以读（包括对端SOCKET正常关闭）
 * EPOLLOUT：表示对应的文件描述符可以写
 * EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）
 * EPOLLERR：表示对应的文件描述符发生错误
 * EPOLLHUP：表示对应的文件描述符被挂断；读写关闭
 * EPOLLRDHUP 表示读关闭
 * EPOLLET：将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)而言的
 * EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里
 */
void WebServer::initEventMode_(int trigMode) {
    listenEvent_ = EPOLLHUP;
    /*EPOLLONESHOT，为了保证当前连接在同一时刻只被一个线程处理*/
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;
    /*根据触发模式设置对应选项*/
    switch (trigMode) {
        case 0:
            break;
        case 1:
            /*连接模式为ET，边沿触发*/
            connEvent_ |= EPOLLET;
            break;
        case 2:
            /*监听模式为ET，边沿触发*/
            listenEvent_ |= EPOLLET;
            break;
        case 3:
            /*处理模式皆为边沿触发*/
            connEvent_ |= EPOLLET;
            listenEvent_ |= EPOLLET;
            break;
        default:
            connEvent_ |= EPOLLET;
            listenEvent_ |= EPOLLET;
            break;
    }
    /*若连接事件为ET模式，那么设置Http类中的标记isET为true*/
    HttpConn::isET = (connEvent_ & EPOLLET);
}

/*
 * 向客户端发送错误消息
 */
void WebServer::sendError_(int fd, const char *info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    /*关闭连接*/
    close(fd);
}

/*
 * 删除epoller描述符监听事件,关闭连接
 */
void WebServer::closeConn_(HttpConn *client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->getFd());
    epoller_->delFd(client->getFd());
    client->close();
}

/*
 * 设置文件描述符为非阻塞，使用fcntl(fd, F_GETFD, 0)获取原有描述符
 * F_SETFL 设置描述符状态标志
 */
int WebServer::setFdNonblock(int fd) {
    assert(fd > 0);
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);

    return old_option;
}

/*
 * 创建监听描述符
 */
bool WebServer::initSocket_() {
    int ret = 0;
    struct sockaddr_in addr;

    /*合法性检查*/
    if (port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port: %d exceed range!", port_);
        return false;
    }
    /*设置监听地址*/
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);
    /*设置openLinger项*/
    struct linger optLinger = {0};
    if (openLinger_) {
        /*优雅关闭: 直到所剩数据发送完毕或超时*/
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }
    /*创建监听套接字*/
    listenFd_ = socket(PF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        LOG_ERROR("Create socket error!");
        return false;
    }
    /*设置连接选项，是否优雅关闭连接*/
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!");
        return false;
    }

    int optVal = 1;
    /* 端口复用，SO_REUSEADDR 立即开启这个端口，不用管之前关闭连接后的2MSL*/
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void *) &optVal, sizeof(int));
    if (ret < 0) {
        LOG_ERROR("Set socket reuse address error!");
        close(listenFd_);
        return false;
    }
    /*绑定套接字监听地址*/
    ret = bind(listenFd_, (struct sockaddr *) &addr, sizeof(addr));
    if (ret < 0) {
        LOG_ERROR("Bind socket error!");
        close(listenFd_);
        return false;
    }
    /*开始监听，socket可以排队的最大连接数最大6个*/
    ret = listen(listenFd_, 6);
    if (ret < 0) {
        LOG_ERROR("Listen port: %d error!", port_);
        close(listenFd_);
        return false;
    }
    /*将监听描述符加入到epoll的监听事件中，监听EPOLLIN读事件*/
    bool res = epoller_->addFd(listenFd_, listenEvent_ | EPOLLIN);
    if (!res) {
        LOG_ERROR("Add epoll listen error!");
        close(listenFd_);
        return false;
    }
    /*设置监听事件为非阻塞的*/
    setFdNonblock(listenFd_);
    LOG_INFO("Server init success! Server port is: %d", port_);

    return true;
}

/*
 * 处理客户端连接事件
 */
void WebServer::dealListen_() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    /*使用do-while很巧妙，因为无论如何都会进入一次循环体，如果监听事件设置为LT模式，则只会调用一次accept与addClient方法
     * 若监听事件是ET模式，则会将连接一次性接受完，直到accept返回-1，表示当前没有连接了
     */
    do {
        int fd = accept(listenFd_, (struct sockaddr *) &addr, &len);
        if (fd < 0) {
            return;
        } else if (HttpConn::userCount >= MAX_FD) {
            /*当前连接数太多，超过了预定义了最大数量，向客户端发送错误信息*/
            sendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        /*添加客户事件*/
        addClient_(fd, addr);
    } while (listenEvent_ & EPOLLET);
}

/*
 * 初始化httpconn类对象，添加对应连接的计时器，添加epoll监听事件
 */
void WebServer::addClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    /*初始化httpconn类对象*/
    users_[fd].init(fd, addr);
    if (timeoutMS_ > 0) {
        /*若设置了超时事件，则需要向定时器里添加这一项*/
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::closeConn_, this, &users_[fd]));
    }
    /*添加epoll监听EPOLLIN事件，连接设置为非阻塞*/
    epoller_->addFd(fd, EPOLLIN | connEvent_);
    setFdNonblock(fd);

    LOG_INFO("Client[%d] in!", users_[fd].getFd());
}

/*
 * 表示对应连接上有读写事件发生，需要调整计时器中的过期时间
 */
void WebServer::extentTime_(HttpConn *client) {
    assert(client);
    if (timeoutMS_ > 0) {
        timer_->adjust(client->getFd(), timeoutMS_);
    }
}

/*
 * 解析http报文
 */
void WebServer::onProcess_(HttpConn *client) {
    if (client->process()) {
        /*成功处理则将epoll在该文件描述符上的监听事件改为EPOLLOUT写事件*/
        epoller_->modFd(client->getFd(), connEvent_ | EPOLLOUT);
    } else {
        /*未成功处理，说明数据还没有读完，需要继续使用epoll监听该连接上的EPOLLIN读事件*/
        epoller_->modFd(client->getFd(), connEvent_ | EPOLLIN);
    }
}

/*
 * 读取socket传来的数据，并调用onProcess函数处理
 */
void WebServer::onRead_(HttpConn *client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;

    /*调用httpconn类的read方法，读取数据*/
    ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN) {
        /*若返回值小于0，且信号不为EAGAIN说明发生了错误*/
        closeConn_(client);
        return;
    }
    /*调用onProcess函数解析数据*/
    onProcess_(client);
}

/*
 * 处理连接中的读取数据事件，调整当前连接的过期时间，向线程池中添加读数据的任务
 */
void WebServer::dealRead_(HttpConn *client) {
    assert(client);
    extentTime_(client);
    threadPool_->addTask(std::bind(&WebServer::onRead_, this, client));
}

/*
 * 向对应的socket发送数据
 */
void WebServer::onWrite_(HttpConn *client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;

    /*调用httpconn类的write方法向socket发送数据*/
    ret = client->write(&writeErrno);
    if (client->toWriteBytes() == 0) {
        /*如果还需要写的数据为0，那么完成传输，检查客户端是否设置了长连接字段*/
        if (client->isKeepAlive()) {
            /*如果客户端设置了长连接，那么调用OnProcess函数，因为此时的client->process()会返回false，所以该连接会重新注册epoll的EPOLLIN事件*/
            onProcess_(client);
            return;
        }
    } else if (ret < 0) {
        /*建议修改为 ret<0 || client->ToWriteBytes()>0*/
        /*若是缓冲区满了，errno会返回EAGAIN，这时需要重新注册EPOLL上的EPOLLOUT事件*/
        if (writeErrno == EAGAIN) {
            /*建议修改为 writeErrno == EAGAIN || client->ToWriteBytes()>0，这两种情况下都要注册EPOLLOUT事件，后者是为了防止LT模式下没写完的问题*/
            /*若返回值小于0，且信号为EAGAIN说明数据还没有发送完，重新在EPOLL上注册该连接的EPOLLOUT事件*/
            epoller_->modFd(client->getFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    /*其余情况，关闭连接*/
    closeConn_(client);
}

/*
 * 处理连接中的发送数据事件，调整当前连接的过期时间，向线程池中添加发送数据的任务
 */
void WebServer::dealWrite_(HttpConn *client) {
    assert(client);
    extentTime_(client);
    threadPool_->addTask(std::bind(&WebServer::onWrite_, this, client));
}

/*
 * 启动服务器
 */
void WebServer::start() {
    /*epoll wait timeout == -1 无事件将阻塞，如果timeout大于0时才会设置超时信号，后面可以改为根据最接近的超时事件设置超时时长*/
    int timeMS = -1;
    if (!isClose_) {
        LOG_INFO("================Server start================");
    }

    /*根据不同的事件调用不同的函数*/
    while (!isClose_) {
        /*每开始一轮的处理事件时，若设置了超时时间，那就处理一下超时事件*/
        if (timeoutMS_ > 0) {
            /*获取最近的超时时间，同时删除超时节点*/
            timeMS = timer_->getNextTick();
        }
        /*epoll等待事件的唤醒，等待时间为最近一个连接会超时的时间*/
        int eventCount = epoller_->wait(timeMS);
        for (int i = 0; i < eventCount; i++) {
            /*获取对应文件描述符与epoll事件*/
            int fd = epoller_->getEventFd(i);
            uint32_t events = epoller_->getEvents(i);

            /*根据不同情况进入不同分支*/
            if (fd == listenFd_) {
                /*若对应文件描述符为监听描述符，进入文件处理流程*/
                dealListen_();
            } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                /*若epoll事件为 (EPOLLRDHUP | EPOLLHUP | EPOLLERR) 其中之一，表示连接出现问题，需要关闭该连接*/
                assert(users_.count(fd) > 0);
                closeConn_(&users_[fd]);
            } else if (events & EPOLLIN) {
                /*若epoll事件为EPOLLIN，表示有对应套接字收到数据，需要读取出来*/
                assert(users_.count(fd) > 0);
                dealRead_(&users_[fd]);
            } else if (events & EPOLLOUT) {
                /*若epoll事件为EPOLLOUT，表示返回给客户端的数据已准备好，需要向对应套接字连接发送数据*/
                assert(users_.count(fd) > 0);
                dealWrite_(&users_[fd]);
            } else {
                /*其余事件皆为错误，向log文件写入该事件*/
                LOG_ERROR("Unexpected event");
            }
        }
    }
}










