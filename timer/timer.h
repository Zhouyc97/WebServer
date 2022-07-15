/*
客户端与服务器端建立连接后，长时间不交换数据，一直占用服务器端的文件描述符，导致连接资源的浪费
服务器每隔一段时间定期检测非活跃连接,从内核事件表删除事件，并关闭文件描述符，释放连接资源
*/

#ifndef TIMER_H
#define TIMER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include "../log/log.h"

//连接资源结构体成员需要用到定时器类
class util_timer;

//客户端连接时会有一个定时器
struct client_data
{
    //客户端socket地址
    sockaddr_in address;

    //socket文件描述符
    int sockfd;

    //定时器
    util_timer *timer;
};

//定时器节点
class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    //超时时间
    time_t expire;
    //回调函数
    void (* cb_func)(client_data *);
    //连接资源
    client_data *user_data;
    //前向定时器
    util_timer *prev;
    //前向定时器
    util_timer *next;
};

//定时器容器类
class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer *timer);
    void adjust_timer(util_timer *timer);
    void del_timer(util_timer *timer);
    void tick();

private:
    void add_timer(util_timer *timer, util_timer *lst_head);

    util_timer *head;
    util_timer *tail;
};

//使用组件
class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

//定时器回调函数,定时事件
void cb_func(client_data *user_data);

#endif
