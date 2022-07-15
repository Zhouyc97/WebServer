/*
池是一组资源的集合，这组资源在服务器启动之初就被完全创建好并初始化。
当系统开始处理客户请求的时候，如果它需要相关的资源，可以直接从池中获取，无需动态分配
当服务器处理完一个客户连接后,可以把相关的资源放回池中，无需执行系统调用释放资源

当系统需要访问数据库时，先系统创建数据库连接，完成数据库操作，然后系统断开数据库连接

若系统需要频繁访问数据库，则需要频繁创建和断开数据库连接，而创建数据库连接是一个很耗时的操作，也容易对数据库造成安全隐患
在程序初始化的时候，集中创建多个数据库连接，并把他们集中管理，供程序使用，可以保证较快的数据库读写速度，更加安全可靠
*/

#ifndef SQL_CONNECTION_POOL_H
#define SQL_CONNECTION_POOL_H


#include <list>
#include <mysql/mysql.h>
#include <error.h>

#include "../log/log.h"

class connection_pool
{
public:
	MYSQL *GetConnection();				 //获取数据库连接
	bool ReleaseConnection(MYSQL *conn); //释放连接
	int GetFreeConn();					 //获取连接
	void DestroyPool();					 //销毁所有连接

	//单例模式
	static connection_pool *GetInstance();

	void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log); 

private:
	connection_pool();
	~connection_pool();

	int m_MaxConn;  //最大连接数
	int m_CurConn;  //当前已使用的连接数
	int m_FreeConn; //当前空闲的连接数
	locker lock;
	list<MYSQL *> connList; //连接池
	sem reserve;

public:
	string m_url;			 //主机地址
	string m_Port;		 //数据库端口号
	string m_User;		 //登陆数据库用户名
	string m_PassWord;	 //登陆数据库密码
	string m_DatabaseName; //使用数据库名
	int m_close_log;	//日志开关
};

//封装数据库连接操作RAII机制,无需手动释放
class connectionRAII{

public:
	connectionRAII(MYSQL **con, connection_pool *connPool);
	~connectionRAII();
	
private:
	MYSQL *conRAII;
	connection_pool *poolRAII;
};

#endif
