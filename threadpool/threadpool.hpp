/*
空间换时间,浪费服务器的硬件资源,换取运行效率.
当服务器进入正式运行阶段,开始处理客户请求的时候,如果它需要相关的资源,可以直接从池中获取,无需动态分配.
当服务器处理完一个客户连接后,可以把相关的资源放回池中,无需执行系统调用释放资源.
*/

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "../mysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
    //thread_number是线程池中线程的数量
    //max_requests是请求队列中最多允许的、等待处理的请求的数量
    //connPool是数据库连接池指针
    threadpool(int actor_model, connection_pool *connPool, 
    int thread_number = 8, int max_request = 10000);
    ~threadpool();
    bool append(T *request, int state);
    bool append_p(T *request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //是否有任务需要处理
    connection_pool *m_connPool;  //数据库
    int m_actor_model;          //模型切换
};

//构造初始化
template <typename T>
threadpool<T>::threadpool( int actor_model, connection_pool *connPool, int thread_number, 
int max_requests) : m_actor_model(actor_model),
m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL),m_connPool(connPool)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();

    //线程id初始化
    m_threads = new pthread_t[m_thread_number];

    if (!m_threads)
        throw std::exception();
    
    //循环创建线程，并将工作线程按要求进行运行
    for (int i = 0; i < thread_number; ++i)
    {
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }

        //将线程进行分离后，不用单独对工作线程进行回收
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

//析构释放线程id数组
template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}

//向请求队列中添加任务
template <typename T>
bool threadpool<T>::append(T *request, int state)
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;
    //添加任务
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    //信号量提醒有任务要处理
    m_queuestat.post();
    return true;
}

//向请求队列中添加任务
template <typename T>
bool threadpool<T>::append_p(T *request)
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

//线程调用的工作函数
template <typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

//线程从请求队列取出并且执行任务
template <typename T>
void threadpool<T>::run()
{
    while (true)
    {
        //信号量等待
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }

        //从请求队列中取出第一个任务
        //将任务从请求队列删除
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if (!request)
            continue;
        
        //reactor,工作线程自己读自己写
        if (1 == m_actor_model)
        {
            // 若是读取任务
            if (0 == request->m_state)
            {
                if (request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            // 若是写任务,不会调用process函数进行处理
            else
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        //proactor,由主线程读写m_read_buf和m_write_buf,工作线程仅仅只负责逻辑处理函数
        //进来时一定是读任务,且已经被主线程读完，因此直接执行业务逻辑即可。因为写任务被主线程所执行
        else
        {
            //RAII机制无需手动释放数据库连接
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            //process(模板类中的方法,这里是http类)进行处理
            request->process();
        }
    }
}

#endif
