#include "webserver.h"

//主线程维护users数组保存http连接对象以及它的一个定时器
WebServer::WebServer(int argc,char **argv):m_port(9006),m_log_write(1),m_OPT_LINGER(0),m_TRIGMode(1),
m_sql_num(16),m_thread_num(16),m_close_log(0),m_actormodel(0)
{
    //http_conn类对象
    users = new http_conn[MAX_FD];

    //工作路径
    char server_path[200];
    getcwd(server_path, 200);

    //设置m_root的资源根目录
    char root[6] = "/src";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    //定时器数组
    users_timer = new client_data[MAX_FD];

    //mysql连接参数
    m_user = "root";
    m_passWord = "1";
    m_databaseName = "yourdb";

    //命令行参数
    m_argc = argc;
    m_argv = argv;
}

//析构函数
WebServer::~WebServer()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

//设置lfd和cfd的属性,0为LT 1为ET
void WebServer::trig_mode()
{
    //LT + LT
    if (0 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }
    //LT + ET
    else if (1 == m_TRIGMode)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    //ET + LT
    else if (2 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    //ET + ET
    else if (3 == m_TRIGMode)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}

//初始化日志
void WebServer::log_write()
{
    //是否开启日志
    if (0 == m_close_log)
    {
        //同步日志 or 异步日志
        if (1 == m_log_write)
            Log::get_instance()->init("./log_file/log", m_close_log, 2000, 800000, 800);
        else
            Log::get_instance()->init("./log_file/log", m_close_log, 2000, 800000, 0);
    }
}

//初始化数据库连接
void WebServer::sql_pool()
{
    //创建数据库连接，单例模式
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_num, m_close_log);

    //初始化数据库读取表
    users->initmysql_result(m_connPool);
}

//初始化线程池
void WebServer::thread_pool()
{
    //线程池
    m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

//socket创建监听,以及工具组件的初始化
void WebServer::eventListen()
{
    //网络编程基础步骤
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    //优雅关闭连接
    if (0 == m_OPT_LINGER)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (1 == m_OPT_LINGER)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    //转换为网络字节序大端对齐
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    //第三次挥手的时候会有个等待释放时间,端口不会迅速的被释放.
    //端口复用解决
    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    //绑定本地IP和PORT
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);

    //监听
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    utils.init(TIMESLOT);

    //epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    //将lfd注册进入内核事件表,不开启oneshot
    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;

    //使用socketpair函数能够创建一对套接字进行通信
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);

    //设置管道写端为非阻塞
    utils.setnonblocking(m_pipefd[1]);

    //设置管道读端添加epoll监视
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    //对一个对端已经关闭的socket调用两次write, 第二次将会生成SIGPIPE信号, 该信号默认结束进程.
    utils.addsig(SIGPIPE, SIG_IGN);

    //传递给主循环的信号值，这里只关注SIGALRM和SIGTERM
    
    /*定时器每隔TIMESLOT(5)秒发送SIGALRM信号
      信号处理函数捕捉到SIGALRM信号时通过管道写出去.
      主线程监听管道读的一侧描述符,若可读则会把timeout = true
      并且主线程去执行工具类的handler检测是否有定时器节点超时,若有则删除该节点释放http连接
      然后再次执行alarm(TIMESLOT)5秒后继续发送SIGALRM信号以此循环
    */
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);

    //工具类,信号和描述符基础操作
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}

//创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
void WebServer::timer(int connfd, struct sockaddr_in client_address)
{
    users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode, m_close_log, m_user, m_passWord, m_databaseName);

    //初始化该连接对应的连接资源
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;

    //创建定时器节点
    util_timer *timer = new util_timer;
    //设置定时器节点对应的连接资源
    timer->user_data = &users_timer[connfd];
    //设置回调函数
    timer->cb_func = cb_func;
    time_t cur = time(NULL);

    //设置绝对超时时间
    timer->expire = cur + 3 * TIMESLOT;

    //创建该连接对应的定时器，初始化为前述临时变量
    users_timer[connfd].timer = timer;
    //将该定时器添加到链表中
    utils.m_timer_lst.add_timer(timer);
}

//若有数据传输，则将定时器往后延迟3个单位
//并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(util_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer once");
}

//移除对应的定时器
void WebServer::deal_timer(util_timer *timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
    {
        utils.m_timer_lst.del_timer(timer);
    }

    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

//处理新客户端连接
bool WebServer::dealclinetdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (0 == m_LISTENTrigmode)
    {
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0)
        {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        //超过最大连接数量
        if (http_conn::m_user_count >= MAX_FD)
        {
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address);
    }
    else
    {
        while (1)
        {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0)
            {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

//主线程读取信号,根据信号去执行相应逻辑操作
bool WebServer::dealwithsignal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    //从管道读端读出信号值，成功返回字节数，失败返回-1
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        return false;
    }

    else if (ret == 0)
    {
        return false;
    }
    //读取成功
    else
    {
        //处理信号值对应的逻辑
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
            case SIGALRM:
            {
                timeout = true;
                break;
            }
            case SIGTERM:
            {
                stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

//监听到cfd可读
void WebServer::dealwithread(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;

    //reactor,读写与业务处理均由工作线程完成
    if (1 == m_actormodel)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        //若监测到读事件，将该事件放入请求队列
        m_pool->append(users + sockfd, 0);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        //proactor,主线程来读写,工作线程完成业务处理
        if (users[sockfd].read_once())
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            //若监测到读事件，将该事件放入请求队列
            m_pool->append_p(users + sockfd);

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

//监听到cfd可写
void WebServer::dealwithwrite(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;
    //reactor
    if (1 == m_actormodel)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        m_pool->append(users + sockfd, 1);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        //proactor
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

//主循环一直监听epoll是否有范围表示有fd可读或者可写
void WebServer::mainStart()
{
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        //忽略EINTR导致的阻塞返回,EINTR表示在读/写的时候出现了中断错误
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }
        //遍历需要处理的事件
        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            //处理新到的客户连接
            if (sockfd == m_listenfd)
            {
                bool flag = dealclinetdata();
                if (false == flag)
                    continue;
            }
            //处理异常事件
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //服务器端关闭连接，移除对应的定时器
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            //处理信号
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = dealwithsignal(timeout, stop_server);
                if (false == flag)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
            //处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                dealwithread(sockfd);
            }
            //将响应报文写回给用户
            else if (events[i].events & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
        
        //处理定时器为非必须事件，收到信号并不是立马处理
        //完成读写事件后，再进行处理
        if (timeout)
        {
            utils.timer_handler();

            LOG_INFO("%s", "timer tick");

            timeout = false;
        }
    }
}

//运行环境初始化
void WebServer::runEnvironment_init()
{

    //解析命令行
    parse_arg(m_argc,m_argv);

    //日志初始化
    log_write();

    //数据库初始化
    sql_pool();

    //线程池初始化
    thread_pool();

    //触发模式初始化
    trig_mode();

    //socket、定时器初始化
    eventListen();

}

//解析命令行
void WebServer::parse_arg(int argc, char*argv[]){
    int opt;
    const char *str = "p:l:m:o:s:t:c:a:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
        case 'p':
        {
            m_port = atoi(optarg);
            break;
        }
        case 'l':
        {
            m_log_write = atoi(optarg);
            break;
        }
        case 'm':
        {
            m_TRIGMode = atoi(optarg);
            break;
        }
        case 'o':
        {
            m_OPT_LINGER = atoi(optarg);
            break;
        }
        case 's':
        {
            m_sql_num = atoi(optarg);
            break;
        }
        case 't':
        {
            m_thread_num = atoi(optarg);
            break;
        }
        case 'c':
        {
            m_close_log = atoi(optarg);
            break;
        }
        case 'a':
        {
            m_actormodel = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }
}
