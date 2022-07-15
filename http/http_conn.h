/*
request任务 即是http_conn对象
*/
#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <map>
#include <fstream>
#include "../mysql/sql_connection_pool.h"
#include "../timer/timer.h"

class http_conn
{
public:
    //设置读取文件的名称m_real_file大小
    static const int FILENAME_LEN = 200;
    //设置读缓冲区m_read_buf大小
    static const int READ_BUFFER_SIZE = 2048;
    ////设置写缓冲区m_write_buf大小
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE
    {
        //当前正在分析请求首行
        CHECK_STATE_REQUESTLINE = 0,
        //当前正在分析头部字段
        CHECK_STATE_HEADER,
        //当前正在解析请求体
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE
    {
        //请求不完整，需要继续读取客户数据
        NO_REQUEST,
        //表示获得了一个完成的客户请求
        GET_REQUEST,
        //表示客户请求语法错误
        BAD_REQUEST,
        //表示服务器没有资源
        NO_RESOURCE,
        //表示客户对资源没有足够的访问权限
        FORBIDDEN_REQUEST,
        //文件请求,获取文件成功
        FILE_REQUEST,
        //表示服务器内部错误
        INTERNAL_ERROR,
        //表示客户端已经关闭连接了
        CLOSED_CONNECTION
    };
    enum LINE_STATUS
    {
        //读取到一个完整的行
        LINE_OK = 0,
        //行出错
        LINE_BAD,
        //行数据尚且不完整
        LINE_OPEN
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    //初始化套接字地址，函数内部会调用私有方法init
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);
    //关闭http连接
    void close_conn(bool real_close = true);
    //执行过程
    void process();
    //读取浏览器端发来数据
    bool read_once();
    //响应报文写入函数
    bool write();
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    //同步线程初始化数据库读取表
    void initmysql_result(connection_pool *connPool);
    int timer_flag;
    int improv;

private:
    void init();
    //从m_read_buf读取，并处理请求报文
    HTTP_CODE process_read();
    //向m_write_buf写入响应报文数据
    bool process_write(HTTP_CODE ret);
    //主状态机解析报文中的请求行数据
    HTTP_CODE parse_request_line(char *text);
    //主状态机解析报文中的请求头数据
    HTTP_CODE parse_headers(char *text);
    //主状态机解析报文中的请求内容
    HTTP_CODE parse_content(char *text);
    //对请求文件进行地址映射
    HTTP_CODE do_request();
    //get_line用于将指针向后偏移，指向未处理的字符
    char *get_line() { return m_read_buf + m_start_line; };

    //从状态机读取一行，分析是请求报文的哪一部分
    LINE_STATUS parse_line();
    void unmap();

    //根据响应报文格式，生成对应8个部分，以下函数均由do_request调用
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    //公有静态成员
    static int m_epollfd;
    static int m_user_count;
    //数据库连接
    MYSQL *mysql;
    int m_state;  //读为0, 写为1

private:
    int m_sockfd;
    sockaddr_in m_address;

    //存储读取的请求报文数据
    char m_read_buf[READ_BUFFER_SIZE];
    //缓冲区中m_read_buf中数据的最后一个字节的下一个位置
    int m_read_idx;
    //m_read_buf读取的位置m_checked_idx
    int m_checked_idx;
    //m_read_buf中已经解析的字符个数
    int m_start_line;

    //存储发出的响应报文数据
    char m_write_buf[WRITE_BUFFER_SIZE];
    //指示buffer中的长度
    int m_write_idx;

    //主状态机的状态
    CHECK_STATE m_check_state;
    //请求方法
    METHOD m_method;

    //存储请求报文的解析结果
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;

    //读取服务器上的文件地址
    char *m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;        //是否启用的POST
    char *m_string; //存储请求头数据
    int bytes_to_send; //剩余发送字节数
    int bytes_have_send; //已发送字节数
    char *doc_root;

    map<string, string> m_users;
    int m_TRIGMode;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];

    //skipList
};

#endif
