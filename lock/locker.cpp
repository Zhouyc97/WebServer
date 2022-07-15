#include "locker.h"

//互斥锁
locker::locker()
{
    int ret = pthread_mutex_init(&m_mutex,NULL);
    //ret != 0 -> init error
    if(ret) throw std::exception();
}

locker::~locker()
{
    pthread_mutex_destroy(&m_mutex);
}

bool locker::lock()
{
    int ret = pthread_mutex_lock(&m_mutex);
    return ret == 0;
}

bool locker::unlock()
{
    int ret = pthread_mutex_unlock(&m_mutex);
    return ret == 0;
}

pthread_mutex_t* locker::get()
{
    return &m_mutex;
}

//条件变量
cond::cond()
{
    int ret = pthread_cond_init(&m_cond,NULL);
    if(ret) throw std::exception();
}

cond::~cond()
{
    pthread_cond_destroy(&m_cond);
}

bool cond::wait(pthread_mutex_t* m_mutex)
{
    int ret = pthread_cond_wait(&m_cond,m_mutex);
    return ret == 0;
}

bool cond::timewait(pthread_mutex_t* m_mutex,timespec time)
{
   int ret = pthread_cond_timedwait(&m_cond,m_mutex,&time);
   return ret == 0;
}

bool cond::signal()
{
   int ret = pthread_cond_signal(&m_cond);
   return ret == 0;
}

bool cond::broadcast()
{
    int ret = pthread_cond_broadcast(&m_cond);
    return ret == 0;
}

//信号量
sem::sem()
{
    int ret = sem_init(&m_sem,0,0);
    if(ret) throw std::exception();
}

sem::sem(int num)
{
    int ret = sem_init(&m_sem,0,num);
    if(ret) throw std::exception();
}

sem::~sem()
{
    sem_destroy(&m_sem);
}

bool sem::wait()
{
    int ret = sem_wait(&m_sem);
    return ret == 0;
}

bool sem::post()
{
    int ret = sem_post(&m_sem);
    return ret == 0;
}