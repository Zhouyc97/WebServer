/*
    服务器类
*/
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <unistd.h>
#include <cstdlib>

#include "threadpool/threadpool.hpp"
#include "./http/http_conn.h"

const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 15000; //最大事件数
const int TIMESLOT = 5;             //最小超时单位

class WebServer
{
public:
    WebServer(int argc,char **argv);
    ~WebServer();

    //初始化运行环境
    void runEnvironment_init();
    //主线程运行函数
    void mainStart();

    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(util_timer *timer);

    
    void deal_timer(util_timer *timer, int sockfd);
    bool dealclinetdata();
    bool dealwithsignal(bool& timeout, bool& stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

public:
    //基础
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    // 1为reactor
    int m_actormodel;

    //管道通信
    int m_pipefd[2];
    int m_epollfd;

    //保存客户端连接的信息
    http_conn *users;

    //数据库相关
    connection_pool *m_connPool;
    string m_user;         //登陆数据库用户名
    string m_passWord;     //登陆数据库密码
    string m_databaseName; //使用数据库名
    int m_sql_num;

    //线程池相关
    threadpool<http_conn> *m_pool;
    int m_thread_num;

    //epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode;
    int m_LISTENTrigmode;
    int m_CONNTrigmode;

    //定时器相关
    client_data *users_timer;
    Utils utils;
private:
    //被调用公有运行环境初始化调用的内部私有函数
    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void eventListen();

    //解析命令行并且保存参数
    void parse_arg(int argc, char*argv[]);

    int m_argc;
    char** m_argv;
};

#endif
