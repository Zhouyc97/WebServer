/*
    使用RAII机制封装线程同步的机制类
*/    
#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <exception>
#include <semaphore.h>


//锁类
class locker
{
public:
    locker();
    ~locker();
    bool lock();
    bool unlock();
    pthread_mutex_t* get();
private:
    pthread_mutex_t m_mutex;
};

//条件变量
class cond
{
public:
    cond();
    ~cond();
    bool wait(pthread_mutex_t* m_mutex);
    bool timewait(pthread_mutex_t* m_mutex,timespec time);
    bool signal();
    bool broadcast();
private:
    pthread_cond_t m_cond;
};

//信号量
class sem
{
public:
    sem();
    sem(int num);
    ~sem();
    bool wait();
    bool post();
private:
    sem_t m_sem;
};

#endif
