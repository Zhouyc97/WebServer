//将生产者-消费者模型进行封装，使用循环数组实现队列，作为两者共享的缓冲区。

#ifndef BLOCK_QUE_HPP
#define BLOCK_QUE_HPP

#include "../lock/locker.h"
#include <stdlib.h>

template <class T>
class block_queue
{
public:
    block_queue(int max_size = 1000);
    void clear();
    ~block_queue();
    bool full() ;
    bool empty();
    bool front(T &value);
    bool back(T &value);
    int size();
    int max_size();
    bool push(const T &item);
    bool pop(T &item);

private:
    locker m_mutex; //锁
    cond m_cond;    //信号量

    T *m_array; //循环数组
    int m_size; //已储存个数
    int m_max_size; //最大储存个数
    int m_front; //队首
    int m_back; //队尾
};

//初始化私有成员
template <class T>
block_queue<T>::block_queue(int max_size)
{
    if (max_size <= 0)
    {
        exit(-1);
    }

    //构造函数创建循环数组
    m_max_size = max_size;
    m_array = new T[max_size];
    m_size = 0;
    m_front = -1;
    m_back = -1;
}

//将循环数组置为初始状态
template <class T>
void block_queue<T>::clear()
{
    m_mutex.lock();
    m_size = 0;
    m_front = -1;
    m_back = -1;
    m_mutex.unlock();
}

//析构销毁循环数组
template <class T>
block_queue<T>::~block_queue()
{
    m_mutex.lock();
    if (m_array != NULL)
        delete [] m_array;
    m_mutex.unlock();
}

//判断队列是否已满
template <class T>
bool block_queue<T>::full()
{
    m_mutex.lock();
    if (m_size >= m_max_size)
    {

        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

//判断队列是否为空
template <class T>
bool block_queue<T>::empty()
{
    m_mutex.lock();
    if (0 == m_size)
    {
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

//返回队首元素
template <class T>
bool block_queue<T>::front(T &value)
{
    m_mutex.lock();
    if (0 == m_size)
    {
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_front];
    m_mutex.unlock();
    return true;
}

//返回队尾元素
template <class T>
bool block_queue<T>::back(T &value)
{
    m_mutex.lock();
    if (0 == m_size)
    {
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_back];
    m_mutex.unlock();
    return true;
}

//已储存元素个数
template <class T>
int block_queue<T>::size()
{
    int tmp = 0;

    m_mutex.lock();
    tmp = m_size;

    m_mutex.unlock();
    return tmp;
}

//最大储存个数
template <class T>
int block_queue<T>::max_size()
{
    int tmp = 0;

    m_mutex.lock();
    tmp = m_max_size;

    m_mutex.unlock();
    return tmp;
}

//往队列添加元素,添加到队尾
template <class T>
bool block_queue<T>::push(const T &item)
{
    m_mutex.lock();
    if (m_size >= m_max_size)
    {
        //阻塞队列已满,无法添加并通知消费者消费
        m_cond.broadcast();
        m_mutex.unlock();
        return false;
    }

    //将新增数据放在循环数组的对应位置
    m_back = (m_back + 1) % m_max_size;
    m_array[m_back] = item;

    m_size++;

    m_cond.broadcast();
    m_mutex.unlock();
    return true;
}

//从队列取出元素,取出队头置于传出参数
template <class T>
bool block_queue<T>::pop(T &item)
{
    
    m_mutex.lock();
    //无元素可取,释放锁并且等待生产者生产
    while (m_size <= 0)
    {
        if (!m_cond.wait(m_mutex.get()))
        {
            m_mutex.unlock();
            return false;
        }
    }
    //取出队列首的元素
    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];
    m_size--;
    m_mutex.unlock();
    return true;
}

#endif
